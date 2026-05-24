#include "handlers.h"
#include "../senders/senders.h"
#include "../services/corps_constants.h"
#include "../services/party_constants.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace tworldsvr::handlers {

namespace {

std::shared_ptr<PeerSession>
FindMapPeer(const HandlerContext& ctx, std::uint8_t msi)
{
    if (msi == 0 || !ctx.peers) return nullptr;
    for (auto& p : ctx.peers->Snapshot())
        if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == msi)
            return p;
    return nullptr;
}

// A squad (party) snapshotted for the corps-join fan-out: the
// ADDSQUAD payload (member describe list) + parallel per-member
// routing (key + map server). Built outside any lock.
struct SquadMemberRt { std::uint32_t char_id = 0, key = 0; std::uint8_t msi = 0; };
struct Squad
{
    std::uint32_t chief_id = 0;
    std::uint16_t party_id = 0;
    std::uint8_t  obtain   = 0;
    bool          found    = false;
    std::vector<senders::SquadMemberInfo> infos;
    std::vector<SquadMemberRt>            rt;
};

Squad SnapshotSquad(const HandlerContext& ctx, std::uint16_t party_id)
{
    Squad sq;
    sq.party_id = party_id;
    auto party = ctx.parties->Find(party_id);
    if (!party) return sq;
    std::vector<std::uint32_t> mids;
    {
        std::lock_guard pg(party->lock);
        sq.chief_id = party->chief_char_id;
        sq.obtain   = party->obtain_type;
        mids        = party->members;
    }
    sq.found = true;
    for (auto mid : mids)
    {
        auto c = ctx.chars->Find(mid);
        if (!c) continue;
        senders::SquadMemberInfo info;
        SquadMemberRt rt;
        {
            std::lock_guard g(c->lock);
            info.char_id = c->char_id;
            info.name    = c->name;
            info.max_hp  = c->max_hp;
            info.hp      = c->hp;
            info.map_id  = c->map_id;
            info.pos_x   = static_cast<std::uint16_t>(c->pos_x);
            info.pos_z   = static_cast<std::uint16_t>(c->pos_z);
            info.level   = c->level;
            info.klass   = c->klass;
            info.race    = c->race;
            info.sex     = c->sex;
            info.face    = c->face;
            info.hair    = c->hair;
            rt.char_id   = c->char_id;
            rt.key       = c->key;
            rt.msi       = c->main_server_id;
        }
        sq.infos.push_back(std::move(info));
        sq.rt.push_back(rt);
    }
    return sq;
}

// Add `joining_party_id` to `corps`, mirroring legacy
// NotifyCorpsJoin + CorpsJoin: announce each existing squad to the
// joiner and the joiner to each existing squad (pairwise ADDSQUAD),
// commit the squad + the party's corps_id back-link, then push each
// joining member CORPSJOIN_REQ + a PARTYATTR carrying the commander.
boost::asio::awaitable<void>
NotifyCorpsJoin(const HandlerContext& ctx, std::shared_ptr<TCorps> corps,
                std::uint16_t joining_party_id)
{
    std::vector<std::uint16_t> existing;
    std::uint16_t corps_id  = 0;
    std::uint16_t commander = 0;
    {
        std::lock_guard cg(corps->lock);
        existing  = corps->squads;
        corps_id  = corps->id;
        commander = corps->commander_party_id;
    }

    const Squad joining = SnapshotSquad(ctx, joining_party_id);

    for (auto esid : existing)
    {
        const Squad es = SnapshotSquad(ctx, esid);
        if (!es.found) continue;
        for (const auto& rt : es.rt)
            if (auto p = FindMapPeer(ctx, rt.msi))
                co_await senders::SendMwAddSquadReq(p, rt.char_id, rt.key,
                    joining.chief_id, joining.party_id, joining.infos);
        for (const auto& rt : joining.rt)
            if (auto p = FindMapPeer(ctx, rt.msi))
                co_await senders::SendMwAddSquadReq(p, rt.char_id, rt.key,
                    es.chief_id, es.party_id, es.infos);
    }

    {
        std::lock_guard cg(corps->lock);
        corps->AddParty(joining_party_id);
    }
    if (auto party = ctx.parties->Find(joining_party_id))
    {
        std::lock_guard pg(party->lock);
        party->corps_id = corps_id;
    }

    for (const auto& rt : joining.rt)
        if (auto p = FindMapPeer(ctx, rt.msi))
        {
            co_await senders::SendMwCorpsJoinReq(p, rt.char_id, rt.key,
                corps_id, commander);
            co_await senders::SendMwPartyAttrReq(p, rt.char_id, rt.key,
                joining.party_id, joining.obtain, joining.chief_id, commander);
        }
}

// Legacy CheckCorpsJoin (TWorldSvr.cpp:4577): reject if both
// parties already belong to a corps; if either's corps is at the
// MAX_CORPS_PARTY cap, reject with kMaxParty. Returns kSuccess (0)
// when the merge is allowed.
std::uint8_t CheckCorpsJoin(const HandlerContext& ctx,
                            std::uint16_t origin_corps,
                            std::uint16_t target_corps)
{
    if (origin_corps != 0 && target_corps != 0)
        return corps::kWrongTarget;

    for (std::uint16_t cid : {origin_corps, target_corps})
    {
        if (cid == 0) continue;
        auto c = ctx.corps ? ctx.corps->Find(cid) : nullptr;
        if (!c) return corps::kWrongTarget;
        std::uint8_t size = 0;
        { std::lock_guard g(c->lock); size = c->Size(); }
        if (size >= corps::kMaxCorpsParty) return corps::kMaxParty;
    }
    return corps::kSuccess;
}

} // namespace

boost::asio::awaitable<void>
OnCorpsAskAck(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.parties)
    {
        spdlog::warn("OnCorpsAskAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   target_name;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(target_name))
    {
        spdlog::warn("OnCorpsAskAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto requester = ctx.chars->Find(char_id);
    if (!requester) co_return;
    std::uint32_t actual_key = 0, req_party = 0;
    std::uint8_t  req_country = 0, req_aid = 0;
    std::string   req_name;
    {
        std::lock_guard g(requester->lock);
        actual_key  = requester->key;
        req_party   = requester->party_id;
        req_country = requester->country;
        req_aid     = requester->aid_country;
        req_name    = requester->name;
    }
    if (actual_key != key) co_return;

    auto reply_self = [&](std::uint8_t result)
        -> boost::asio::awaitable<void> {
        co_await senders::SendMwCorpsReplyReq(peer, char_id, key, result,
            target_name);
    };

    auto target = ctx.chars->FindByName(target_name);
    if (!target)
    {
        co_await reply_self(corps::kWrongTarget);
        co_return;
    }
    std::uint32_t t_party = 0, t_charid = 0, t_key = 0;
    std::uint8_t  t_country = 0, t_aid = 0, t_msi = 0;
    {
        std::lock_guard g(target->lock);
        t_party   = target->party_id;
        t_charid  = target->char_id;
        t_key     = target->key;
        t_country = target->country;
        t_aid     = target->aid_country;
        t_msi     = target->main_server_id;
    }

    // Both must be the chief of a (non-arena) party, same war-country.
    bool          req_is_chief = false, req_arena = false;
    std::uint16_t req_corps = 0;
    if (req_party != 0)
        if (auto p = ctx.parties->Find(static_cast<std::uint16_t>(req_party)))
        {
            std::lock_guard pg(p->lock);
            req_is_chief = p->IsChief(char_id);
            req_arena    = p->arena;
            req_corps    = p->corps_id;
        }
    bool          t_is_chief = false, t_arena = false;
    std::uint16_t t_corps = 0;
    if (t_party != 0)
        if (auto p = ctx.parties->Find(static_cast<std::uint16_t>(t_party)))
        {
            std::lock_guard pg(p->lock);
            t_is_chief = p->IsChief(t_charid);
            t_arena    = p->arena;
            t_corps    = p->corps_id;
        }

    if (req_party == 0 || !req_is_chief || t_party == 0 || !t_is_chief ||
        party::WarCountry(req_country, req_aid) !=
            party::WarCountry(t_country, t_aid))
    {
        co_await reply_self(corps::kNoParty);
        co_return;
    }
    if (req_arena || t_arena)
    {
        co_await reply_self(corps::kBusy);
        co_return;
    }

    const std::uint8_t gate = CheckCorpsJoin(ctx, req_corps, t_corps);
    if (gate != corps::kSuccess)
    {
        co_await reply_self(gate);
        co_return;
    }

    // Forward the invite to the target chief's map.
    auto tp = FindMapPeer(ctx, t_msi);
    if (!tp)
    {
        spdlog::info("OnCorpsAskAck[{}]: target '{}' map {} offline — drop",
            ip, target_name, t_msi);
        co_return;
    }
    co_await senders::SendMwCorpsAskReq(tp, t_charid, t_key, char_id, req_name);
    spdlog::info("OnCorpsAskAck[{}]: char_id={} invited '{}' to corps",
        ip, char_id, target_name);
    co_return;
}

boost::asio::awaitable<void>
OnCorpsReplyAck(std::shared_ptr<PeerSession> peer,
                std::vector<std::byte>       body,
                const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.parties || !ctx.corps)
    {
        spdlog::warn("OnCorpsReplyAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  reply = 0;
    std::string   req_name;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(reply) ||
        !r.ReadString(req_name))
    {
        spdlog::warn("OnCorpsReplyAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto answerer = ctx.chars->Find(char_id);  // pPlayer
    if (!answerer) co_return;
    std::uint32_t actual_key = 0, a_party = 0;
    std::uint8_t  a_country = 0, a_aid = 0;
    std::string   a_name;
    {
        std::lock_guard g(answerer->lock);
        actual_key = answerer->key;
        a_party    = answerer->party_id;
        a_country  = answerer->country;
        a_aid      = answerer->aid_country;
        a_name     = answerer->name;
    }
    if (actual_key != key) co_return;

    auto reply_self = [&](std::uint8_t result)
        -> boost::asio::awaitable<void> {
        co_await senders::SendMwCorpsReplyReq(peer, char_id, key, result,
            req_name);
    };

    auto inviter = ctx.chars->FindByName(req_name);  // pRequest
    if (!inviter)
    {
        co_await reply_self(corps::kWrongTarget);
        co_return;
    }
    std::uint32_t i_charid = 0, i_key = 0, i_party = 0;
    std::uint8_t  i_country = 0, i_aid = 0, i_msi = 0;
    {
        std::lock_guard g(inviter->lock);
        i_charid  = inviter->char_id;
        i_key     = inviter->key;
        i_party   = inviter->party_id;
        i_country = inviter->country;
        i_aid     = inviter->aid_country;
        i_msi     = inviter->main_server_id;
    }
    auto inviter_peer = FindMapPeer(ctx, i_msi);
    if (!inviter_peer)
    {
        co_await reply_self(corps::kWrongTarget);
        co_return;
    }

    auto reply_inviter = [&](std::uint8_t result)
        -> boost::asio::awaitable<void> {
        co_await senders::SendMwCorpsReplyReq(inviter_peer, i_charid, i_key,
            result, a_name);
    };

    if (reply != party::kAskYes)
    {
        co_await reply_inviter(reply);
        co_return;
    }

    // Re-validate the gate (state may have changed since the invite).
    bool          a_is_chief = false, a_arena = false;
    std::uint16_t a_corps = 0;
    if (a_party != 0)
        if (auto p = ctx.parties->Find(static_cast<std::uint16_t>(a_party)))
        {
            std::lock_guard pg(p->lock);
            a_is_chief = p->IsChief(char_id);
            a_arena    = p->arena;
            a_corps    = p->corps_id;
        }
    bool          i_is_chief = false, i_arena = false;
    std::uint16_t i_corps = 0;
    if (i_party != 0)
        if (auto p = ctx.parties->Find(static_cast<std::uint16_t>(i_party)))
        {
            std::lock_guard pg(p->lock);
            i_is_chief = p->IsChief(i_charid);
            i_arena    = p->arena;
            i_corps    = p->corps_id;
        }

    if (a_party == 0 || !a_is_chief || i_party == 0 || !i_is_chief ||
        party::WarCountry(a_country, a_aid) !=
            party::WarCountry(i_country, i_aid))
    {
        co_await reply_self(corps::kWrongTarget);
        co_await reply_inviter(corps::kWrongTarget);
        co_return;
    }
    if (a_arena || i_arena)
    {
        co_await reply_self(corps::kBusy);
        co_await reply_inviter(corps::kBusy);
        co_return;
    }
    const std::uint8_t gate = CheckCorpsJoin(ctx, a_corps, i_corps);
    if (gate != corps::kSuccess)
    {
        co_await reply_self(gate);
        co_await reply_inviter(gate);
        co_return;
    }

    if (a_corps == 0 && i_corps == 0)
    {
        // Form a fresh corps: the inviter's party is the commander.
        auto corps = std::make_shared<TCorps>();
        corps->commander_party_id = static_cast<std::uint16_t>(i_party);
        corps->general_char_id    = i_charid;
        bool inserted = false;
        for (int n = 0; n < 4 && !inserted; ++n)
        {
            const std::uint16_t id = ctx.corps->GenId(ctx.parties);
            if (id == 0) break;
            corps->id = id;
            inserted = ctx.corps->Insert(corps);
        }
        if (!inserted)
        {
            spdlog::error("OnCorpsReplyAck[{}]: could not allocate corps id",
                ip);
            co_return;
        }
        co_await NotifyCorpsJoin(ctx, corps,
            static_cast<std::uint16_t>(a_party));
        co_await NotifyCorpsJoin(ctx, corps,
            static_cast<std::uint16_t>(i_party));
        spdlog::info("OnCorpsReplyAck[{}]: formed corps {} commander_party={}",
            ip, corps->id, i_party);
        co_return;
    }

    // One side already has a corps — the other party joins it.
    const std::uint16_t join_into = a_corps != 0 ? a_corps : i_corps;
    const std::uint16_t joining   = a_corps != 0
        ? static_cast<std::uint16_t>(i_party)
        : static_cast<std::uint16_t>(a_party);
    if (auto corps = ctx.corps->Find(join_into))
    {
        co_await NotifyCorpsJoin(ctx, corps, joining);
        spdlog::info("OnCorpsReplyAck[{}]: party {} joined corps {}",
            ip, joining, join_into);
    }
    co_return;
}

namespace {

// Push every member of `party_id` a CORPSJOIN_REQ (their corps id +
// the given commander) + a PARTYATTR carrying the commander —
// legacy CorpsJoin. Used to refresh survivors after succession and
// to tell a leaving squad its corps/commander are now 0 (the
// party's corps_id is already cleared before this call in that case).
boost::asio::awaitable<void>
CorpsJoinBroadcast(const HandlerContext& ctx, std::uint16_t party_id,
                   std::uint16_t commander)
{
    auto party = ctx.parties->Find(party_id);
    if (!party) co_return;
    std::uint16_t corps_id = 0;
    std::uint32_t chief    = 0;
    std::uint8_t  obtain   = 0;
    std::vector<std::uint32_t> mids;
    {
        std::lock_guard pg(party->lock);
        corps_id = party->corps_id;
        chief    = party->chief_char_id;
        obtain   = party->obtain_type;
        mids     = party->members;
    }
    for (auto mid : mids)
    {
        auto c = ctx.chars->Find(mid);
        if (!c) continue;
        std::uint32_t key = 0;
        std::uint8_t  msi = 0;
        { std::lock_guard g(c->lock); key = c->key; msi = c->main_server_id; }
        if (auto p = FindMapPeer(ctx, msi))
        {
            co_await senders::SendMwCorpsJoinReq(p, mid, key, corps_id,
                commander);
            co_await senders::SendMwPartyAttrReq(p, mid, key, party_id, obtain,
                chief, commander);
        }
    }
}

// Remove `leaving_party_id` from `corps`, mirroring legacy
// NotifyCorpsLeave. Self-recursive coroutine (dissolve pulls the
// last squad out too) → forward-declared.
boost::asio::awaitable<void>
NotifyCorpsLeave(const HandlerContext& ctx, std::shared_ptr<TCorps> corps,
                 std::uint16_t leaving_party_id);

boost::asio::awaitable<void>
NotifyCorpsLeave(const HandlerContext& ctx, std::shared_ptr<TCorps> corps,
                 std::uint16_t leaving_party_id)
{
    std::vector<std::uint16_t> squads;
    std::uint16_t corps_id = 0;
    {
        std::lock_guard cg(corps->lock);
        squads   = corps->squads;
        corps_id = corps->id;
    }

    const Squad leaving = SnapshotSquad(ctx, leaving_party_id);

    // Mutual DELSQUAD: tell each other squad the leaver is gone, and
    // tell the leaver each other squad is gone.
    for (auto sid : squads)
    {
        if (sid == leaving_party_id) continue;
        const Squad other = SnapshotSquad(ctx, sid);
        for (const auto& rt : other.rt)
            if (auto p = FindMapPeer(ctx, rt.msi))
                co_await senders::SendMwDelSquadReq(p, rt.char_id, rt.key,
                    leaving_party_id);
        for (const auto& rt : leaving.rt)
            if (auto p = FindMapPeer(ctx, rt.msi))
                co_await senders::SendMwDelSquadReq(p, rt.char_id, rt.key, sid);
    }

    bool          dissolve = false, succession = false;
    std::uint16_t new_commander = 0;
    {
        std::lock_guard cg(corps->lock);
        corps->RemoveParty(leaving_party_id);
        dissolve = (corps->squads.size() == 1);
        if (!dissolve && corps->commander_party_id == leaving_party_id &&
            !corps->squads.empty())
        {
            new_commander = corps->squads.front();
            corps->commander_party_id = new_commander;
            succession = true;
        }
    }
    if (auto p = ctx.parties->Find(leaving_party_id))
    {
        std::lock_guard pg(p->lock);
        p->corps_id = 0;
    }
    if (succession)
    {
        std::uint32_t gen = 0;
        if (auto np = ctx.parties->Find(new_commander))
        { std::lock_guard pg(np->lock); gen = np->chief_char_id; }
        std::lock_guard cg(corps->lock);
        corps->general_char_id = gen;
    }

    if (dissolve)
    {
        std::uint16_t last = 0;
        { std::lock_guard cg(corps->lock);
          if (!corps->squads.empty()) last = corps->squads.front(); }
        if (last != 0) co_await NotifyCorpsLeave(ctx, corps, last);
        if (ctx.corps) ctx.corps->Remove(corps_id);
    }
    else if (succession)
    {
        std::vector<std::uint16_t> remaining;
        { std::lock_guard cg(corps->lock); remaining = corps->squads; }
        for (auto sid : remaining)
            co_await CorpsJoinBroadcast(ctx, sid, new_commander);
    }

    // The leaver's members: corps + commander are now 0.
    co_await CorpsJoinBroadcast(ctx, leaving_party_id, 0);
}

} // namespace

boost::asio::awaitable<void>
OnCorpsLeaveAck(std::shared_ptr<PeerSession> peer,
                std::vector<std::byte>       body,
                const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.parties || !ctx.corps)
    {
        spdlog::warn("OnCorpsLeaveAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint16_t squad_id = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(squad_id))
    {
        spdlog::warn("OnCorpsLeaveAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    // The squad to remove must be a real party.
    if (!ctx.parties->Find(squad_id)) co_return;

    auto requester = ctx.chars->Find(char_id);
    if (!requester) co_return;
    std::uint32_t actual_key = 0, req_party = 0;
    {
        std::lock_guard g(requester->lock);
        actual_key = requester->key;
        req_party  = requester->party_id;
    }
    if (actual_key != key || req_party == 0) co_return;

    // The requester must be the chief of their party.
    bool          is_chief = false;
    std::uint16_t req_corps = 0;
    if (auto p = ctx.parties->Find(static_cast<std::uint16_t>(req_party)))
    {
        std::lock_guard pg(p->lock);
        is_chief  = p->IsChief(char_id);
        req_corps = p->corps_id;
    }
    if (!is_chief || req_corps == 0) co_return;

    auto corps = ctx.corps->Find(req_corps);
    if (!corps) co_return;

    // Authority: leave your own squad, or — as general — kick any.
    bool is_member = false;
    std::uint32_t general = 0;
    {
        std::lock_guard cg(corps->lock);
        is_member = corps->IsParty(squad_id);
        general   = corps->general_char_id;
    }
    if (!is_member) co_return;
    if (squad_id != static_cast<std::uint16_t>(req_party) && general != char_id)
        co_return;

    co_await NotifyCorpsLeave(ctx, corps, squad_id);
    spdlog::info("OnCorpsLeaveAck[{}]: char_id={} removed squad {} "
                 "from corps {}", ip, char_id, squad_id, req_corps);
    co_return;
}

boost::asio::awaitable<void>
OnChgCorpsCommanderAck(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.parties || !ctx.corps)
    {
        spdlog::warn("OnChgCorpsCommanderAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint16_t target_party = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(target_party))
    {
        spdlog::warn("OnChgCorpsCommanderAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto requester = ctx.chars->Find(char_id);
    if (!requester) co_return;
    std::uint32_t actual_key = 0, req_party = 0;
    {
        std::lock_guard g(requester->lock);
        actual_key = requester->key;
        req_party  = requester->party_id;
    }
    if (actual_key != key) co_return;

    auto reply = [&](std::uint8_t result) -> boost::asio::awaitable<void> {
        co_await senders::SendMwChgCorpsCommanderReq(peer, char_id, key,
            result);
    };

    if (req_party == 0)
    {
        co_await reply(corps::kNoParty);
        co_return;
    }
    std::uint16_t req_corps = 0;
    if (auto p = ctx.parties->Find(static_cast<std::uint16_t>(req_party)))
    { std::lock_guard pg(p->lock); req_corps = p->corps_id; }
    if (req_corps == 0)
    {
        co_await reply(corps::kNoParty);
        co_return;
    }
    auto corps = ctx.corps->Find(req_corps);
    if (!corps)
    {
        co_await reply(corps::kNoParty);
        co_return;
    }

    std::uint16_t commander = 0;
    std::uint32_t general = 0;
    bool          target_member = false;
    {
        std::lock_guard cg(corps->lock);
        commander     = corps->commander_party_id;
        general       = corps->general_char_id;
        target_member = corps->IsParty(target_party);
    }
    // Only the general (chief of the commander party) may reassign.
    if (static_cast<std::uint16_t>(req_party) != commander || char_id != general)
    {
        co_await reply(corps::kNotCommander);
        co_return;
    }
    if (target_party == commander)
    {
        co_await reply(corps::kWrongTarget);
        co_return;
    }
    if (!target_member)
    {
        co_await reply(corps::kTargetNoParty);
        co_return;
    }

    std::uint32_t new_general = 0;
    if (auto np = ctx.parties->Find(target_party))
    { std::lock_guard pg(np->lock); new_general = np->chief_char_id; }
    {
        std::lock_guard cg(corps->lock);
        corps->commander_party_id = target_party;
        corps->general_char_id    = new_general;
    }

    co_await reply(corps::kChgCommander);

    std::vector<std::uint16_t> squads;
    { std::lock_guard cg(corps->lock); squads = corps->squads; }
    for (auto sid : squads)
        co_await CorpsJoinBroadcast(ctx, sid, target_party);

    spdlog::info("OnChgCorpsCommanderAck[{}]: corps {} commander {}→{}",
        ip, req_corps, commander, target_party);
    co_return;
}

boost::asio::awaitable<void>
OnCorpsCmdAck(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.parties)
    {
        spdlog::warn("OnCorpsCmdAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t general = 0, key = 0, char_id = 0, target_id = 0;
    std::uint16_t map_id = 0, squad_id = 0, pos_x = 0, pos_z = 0;
    std::uint8_t  cmd = 0, target_type = 0;
    if (!r.Read(general) || !r.Read(key) || !r.Read(map_id) ||
        !r.Read(squad_id) || !r.Read(char_id) || !r.Read(cmd) ||
        !r.Read(target_id) || !r.Read(target_type) || !r.Read(pos_x) ||
        !r.Read(pos_z))
    {
        spdlog::warn("OnCorpsCmdAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto issuer = ctx.chars->Find(general);
    if (!issuer) co_return;
    std::uint32_t actual_key = 0, g_party = 0;
    {
        std::lock_guard g(issuer->lock);
        actual_key = issuer->key;
        g_party    = issuer->party_id;
    }
    if (actual_key != key || g_party == 0) co_return;

    // (Legacy caches the order on the squad's + commander's
    // m_command for late-joiner ADDSQUAD — deferred; see header.)

    std::uint16_t g_corps = 0;
    if (auto p = ctx.parties->Find(static_cast<std::uint16_t>(g_party)))
    { std::lock_guard pg(p->lock); g_corps = p->corps_id; }

    // Collect the recipient squads: every squad in the corps, or
    // just the issuer's party when corps-less.
    std::vector<std::uint16_t> recipient_parties;
    if (g_corps != 0)
    {
        if (auto c = ctx.corps->Find(g_corps))
        { std::lock_guard cg(c->lock); recipient_parties = c->squads; }
        else
            recipient_parties.push_back(static_cast<std::uint16_t>(g_party));
    }
    else
        recipient_parties.push_back(static_cast<std::uint16_t>(g_party));

    // Flatten to member char ids.
    std::vector<std::uint32_t> members;
    for (auto pid : recipient_parties)
        if (auto p = ctx.parties->Find(pid))
        {
            std::lock_guard pg(p->lock);
            members.insert(members.end(), p->members.begin(),
                p->members.end());
        }

    for (auto mid : members)
    {
        auto c = ctx.chars->Find(mid);
        if (!c) continue;
        std::uint32_t mkey = 0;
        std::uint8_t  msi  = 0;
        { std::lock_guard g(c->lock); mkey = c->key; msi = c->main_server_id; }
        if (auto p = FindMapPeer(ctx, msi))
            co_await senders::SendMwCorpsCmdReq(p, mid, mkey, squad_id,
                char_id, map_id, cmd, target_id, target_type, pos_x, pos_z);
    }
    co_return;
}

namespace {

// Shared body for the six opaque corps-chief relays (legacy
// BroadcastCorps + RelayCorpsMsg). Forwards the inbound payload
// (everything after the leading char_id + key) to every other
// squad's chief under `req_id`, with each recipient's char_id + key
// swapped in. Fires only when the sender is the chief of a party in
// a corps.
boost::asio::awaitable<void>
CorpsChiefRelay(std::shared_ptr<PeerSession> peer,
                std::vector<std::byte> body, const HandlerContext& ctx,
                std::uint16_t req_id)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.parties || !ctx.corps) co_return;

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("CorpsChiefRelay[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }
    // The opaque tail = everything after the 8-byte char_id+key head.
    std::vector<std::byte> tail(body.begin() + 8, body.end());

    auto sender = ctx.chars->Find(char_id);
    if (!sender) co_return;
    std::uint32_t actual_key = 0, party_id = 0;
    {
        std::lock_guard g(sender->lock);
        actual_key = sender->key;
        party_id   = sender->party_id;
    }
    if (actual_key != key || party_id == 0) co_return;

    // Sender must be the chief of a party that belongs to a corps.
    bool          is_chief = false;
    std::uint16_t corps_id = 0;
    if (auto p = ctx.parties->Find(static_cast<std::uint16_t>(party_id)))
    {
        std::lock_guard pg(p->lock);
        is_chief = p->IsChief(char_id);
        corps_id = p->corps_id;
    }
    if (!is_chief || corps_id == 0) co_return;

    auto corps = ctx.corps->Find(corps_id);
    if (!corps) co_return;
    std::vector<std::uint16_t> squads;
    { std::lock_guard cg(corps->lock); squads = corps->squads; }

    for (auto sid : squads)
    {
        if (sid == static_cast<std::uint16_t>(party_id)) continue; // skip own
        std::uint32_t chief_id = 0;
        if (auto sq = ctx.parties->Find(sid))
        { std::lock_guard pg(sq->lock); chief_id = sq->chief_char_id; }
        if (chief_id == 0) continue;
        auto chief = ctx.chars->Find(chief_id);
        if (!chief) continue;
        std::uint32_t ckey = 0;
        std::uint8_t  cmsi = 0;
        { std::lock_guard g(chief->lock); ckey = chief->key;
          cmsi = chief->main_server_id; }
        if (auto p = FindMapPeer(ctx, cmsi))
            co_await senders::SendMwCorpsChiefRelay(p, req_id, chief_id, ckey,
                tail);
    }
}

} // namespace

boost::asio::awaitable<void>
OnCorpsEnemyListAck(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte> body, const HandlerContext& ctx)
{
    co_await CorpsChiefRelay(std::move(peer), std::move(body), ctx,
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CORPSENEMYLIST_REQ));
}

boost::asio::awaitable<void>
OnMoveCorpsEnemyAck(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte> body, const HandlerContext& ctx)
{
    co_await CorpsChiefRelay(std::move(peer), std::move(body), ctx,
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_MOVECORPSENEMY_REQ));
}

boost::asio::awaitable<void>
OnMoveCorpsUnitAck(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte> body, const HandlerContext& ctx)
{
    co_await CorpsChiefRelay(std::move(peer), std::move(body), ctx,
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_MOVECORPSUNIT_REQ));
}

boost::asio::awaitable<void>
OnAddCorpsEnemyAck(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte> body, const HandlerContext& ctx)
{
    co_await CorpsChiefRelay(std::move(peer), std::move(body), ctx,
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_ADDCORPSENEMY_REQ));
}

boost::asio::awaitable<void>
OnDelCorpsEnemyAck(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte> body, const HandlerContext& ctx)
{
    co_await CorpsChiefRelay(std::move(peer), std::move(body), ctx,
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_DELCORPSENEMY_REQ));
}

boost::asio::awaitable<void>
OnCorpsHpAck(std::shared_ptr<PeerSession> peer,
             std::vector<std::byte> body, const HandlerContext& ctx)
{
    co_await CorpsChiefRelay(std::move(peer), std::move(body), ctx,
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CORPSHP_REQ));
}

} // namespace tworldsvr::handlers
