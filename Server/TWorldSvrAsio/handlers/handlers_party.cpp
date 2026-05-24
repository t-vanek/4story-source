#include "handlers.h"
#include "../senders/senders.h"
#include "../services/corps_constants.h"
#include "../services/guild_cabinet_codec.h"
#include "../services/party_constants.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace tworldsvr::handlers {

namespace {

// Find the map peer serving a given main_server_id (msi), or
// nullptr if that map is offline. Mirrors legacy FindMapSvr —
// same helper the guild invite/answer relay paths use.
std::shared_ptr<PeerSession>
FindMapPeer(const HandlerContext& ctx, std::uint8_t msi)
{
    if (msi == 0 || !ctx.peers) return nullptr;
    for (auto& p : ctx.peers->Snapshot())
        if (static_cast<std::uint8_t>(p->Wid() & 0xFF) == msi)
            return p;
    return nullptr;
}

// Snapshot a char's MW_PARTYJOIN_REQ describe-fields + its session
// key + main_server_id. Returns false if the char isn't online.
// Resolves the guild name through GuildRegistry (NAME_NULL = "" when
// guildless). Takes the char lock then, separately, the guild lock
// — never nested, honouring the char-before-guild ordering rule.
bool SnapshotMember(const HandlerContext& ctx, std::uint32_t char_id,
                    senders::PartyMemberInfo& info,
                    std::uint32_t& key, std::uint8_t& msi)
{
    auto c = ctx.chars->Find(char_id);
    if (!c) return false;
    std::uint32_t guild_id = 0;
    {
        std::lock_guard g(c->lock);
        info.char_id = c->char_id;
        info.name    = c->name;
        info.level   = c->level;
        info.max_hp  = c->max_hp;
        info.hp      = c->hp;
        info.max_mp  = c->max_mp;
        info.mp      = c->mp;
        info.race    = c->race;
        info.sex     = c->sex;
        info.face    = c->face;
        info.hair    = c->hair;
        info.klass   = c->klass;
        key          = c->key;
        msi          = c->main_server_id;
        guild_id     = c->guild_id;
    }
    info.guild_name.clear(); // NAME_NULL
    if (guild_id != 0 && ctx.guilds)
        if (auto g = ctx.guilds->Find(guild_id))
        {
            std::lock_guard gl(g->lock);
            info.guild_name = g->name;
        }
    return true;
}

// Add a char to an existing party and run the legacy JoinParty
// fan-out: announce each current member to the joiner and the
// joiner to each member (pairwise MW_PARTYJOIN_REQ), then commit
// the member-add + back-pointer and push the joiner a
// MW_PARTYATTR_REQ HUD refresh. Offline members are skipped (their
// map can't receive the packet) — same as legacy's per-member
// FindMapSvr `continue`.
//
// Lock discipline: the party's member list + meta are snapshotted
// under the party lock and released before any char lock is taken,
// so a char lock is never held across the party lock.
boost::asio::awaitable<void>
JoinPartyFanout(const HandlerContext& ctx,
                std::shared_ptr<TParty> party,
                std::uint32_t joining_char_id)
{
    senders::PartyMemberInfo joining_info;
    std::uint32_t joining_key = 0;
    std::uint8_t  joining_msi = 0;
    if (!SnapshotMember(ctx, joining_char_id, joining_info,
                        joining_key, joining_msi))
        co_return;
    auto joining_peer = FindMapPeer(ctx, joining_msi);

    std::vector<std::uint32_t> member_ids;
    std::uint32_t chief_id = 0;
    std::uint16_t pid      = 0;
    std::uint8_t  obtain   = 0;
    {
        std::lock_guard pg(party->lock);
        member_ids = party->members;
        chief_id   = party->chief_char_id;
        pid        = party->id;
        obtain     = party->obtain_type;
    }

    for (auto mid : member_ids)
    {
        senders::PartyMemberInfo minfo;
        std::uint32_t mkey = 0;
        std::uint8_t  mmsi = 0;
        if (!SnapshotMember(ctx, mid, minfo, mkey, mmsi)) continue;
        auto member_peer = FindMapPeer(ctx, mmsi);
        if (!member_peer) continue;

        if (joining_peer)
            co_await senders::SendMwPartyJoinReq(joining_peer,
                joining_info.char_id, joining_key, pid, chief_id,
                /*commander=*/0, obtain, minfo);
        co_await senders::SendMwPartyJoinReq(member_peer, minfo.char_id,
            mkey, pid, chief_id, /*commander=*/0, obtain, joining_info);
    }

    {
        std::lock_guard pg(party->lock);
        party->AddMember(joining_char_id);
    }
    if (auto jc = ctx.chars->Find(joining_char_id))
    {
        std::lock_guard g(jc->lock);
        jc->party_id = pid;
    }
    if (joining_peer)
        co_await senders::SendMwPartyAttrReq(joining_peer,
            joining_info.char_id, joining_key, pid, obtain, chief_id,
            /*commander=*/0);
}

} // namespace

boost::asio::awaitable<void>
OnPartyAddAck(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnPartyAddAck[{}]: registries not wired — dropped", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::string   request_name, target_name;
    std::uint8_t  obtain_type = 0;
    std::uint32_t max_hp = 0, hp = 0, max_mp = 0, mp = 0;
    if (!r.ReadString(request_name) || !r.ReadString(target_name) ||
        !r.Read(obtain_type) ||
        !r.Read(max_hp) || !r.Read(hp) || !r.Read(max_mp) || !r.Read(mp))
    {
        spdlog::warn("OnPartyAddAck[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }

    auto requester = ctx.chars->FindByName(request_name);
    if (!requester)
    {
        // Legacy: !pRequest → silent drop (the inviter logged off
        // between the client request and this packet).
        spdlog::info("OnPartyAddAck[{}]: requester='{}' offline — drop",
            ip, request_name);
        co_return;
    }
    auto target = ctx.chars->FindByName(target_name);

    // Inviting yourself (same TChar) is a no-op drop (legacy
    // pRequest == pTarget short-circuit).
    if (target && target.get() == requester.get())
        co_return;

    std::uint32_t req_char_id = 0, req_key = 0, req_party_id = 0;
    std::uint8_t  req_country = 0, req_aid = 0;
    {
        std::lock_guard g(requester->lock);
        req_char_id  = requester->char_id;
        req_key      = requester->key;
        req_party_id = requester->party_id;
        req_country  = requester->country;
        req_aid      = requester->aid_country;
    }

    // All failure replies go back to the requester's map — the
    // originating peer (legacy FindMapSvr(pRequest->m_bMainID)).
    auto reply_requester =
        [&](std::uint8_t result) -> boost::asio::awaitable<void> {
        co_await senders::SendMwPartyAddReq(peer, req_char_id, req_key,
            request_name, target_name, obtain_type, result, /*request=*/0);
    };

    if (!target)
    {
        co_await reply_requester(party::kNoUser);
        co_return;
    }

    std::uint32_t tgt_char_id = 0, tgt_key = 0, tgt_party_id = 0;
    std::uint8_t  tgt_country = 0, tgt_aid = 0, tgt_msi = 0;
    bool          tgt_waiter  = false;
    {
        std::lock_guard g(target->lock);
        tgt_char_id  = target->char_id;
        tgt_key      = target->key;
        tgt_party_id = target->party_id;
        tgt_country  = target->country;
        tgt_aid      = target->aid_country;
        tgt_waiter   = target->party_waiter;
        tgt_msi      = target->main_server_id;
    }

    if (tgt_waiter)
    {
        co_await reply_requester(party::kWaiters);
        co_return;
    }
    if (tgt_party_id != 0)
    {
        co_await reply_requester(party::kAlready);
        co_return;
    }
    if (party::WarCountry(tgt_country, tgt_aid) !=
        party::WarCountry(req_country, req_aid))
    {
        co_await reply_requester(party::kCountry);
        co_return;
    }

    // Requester-already-in-a-party gate: only the chief of a
    // non-full, non-arena party may add a member.
    if (req_party_id != 0 && ctx.parties)
    {
        if (auto pty = ctx.parties->Find(req_party_id))
        {
            bool         is_chief = false, arena = false;
            std::uint8_t size     = 0;
            {
                std::lock_guard pg(pty->lock);
                is_chief = pty->IsChief(req_char_id);
                size     = pty->Size();
                arena    = pty->arena;
            }
            if (!is_chief)
            {
                co_await reply_requester(party::kNotChief);
                co_return;
            }
            if (size >= party::kMaxPartyMember)
            {
                co_await reply_requester(party::kFull);
                co_return;
            }
            if (arena)
            {
                // Legacy: silent drop — can't grow an arena party.
                spdlog::info("OnPartyAddAck[{}]: requester char_id={} in "
                             "arena party {} — drop", ip, req_char_id,
                    req_party_id);
                co_return;
            }
        }
        else
        {
            spdlog::warn("OnPartyAddAck[{}]: requester char_id={} has stale "
                         "party_id={} — treating as partyless",
                ip, req_char_id, req_party_id);
        }
    }

    // SetCharStatus(pRequest): stash the inviter's current combat
    // stats so the W3b-2 JOIN broadcast can ship them.
    {
        std::lock_guard g(requester->lock);
        requester->max_hp = max_hp;
        requester->hp     = hp;
        requester->max_mp = max_mp;
        requester->mp     = mp;
    }

    // Forward the AGREE dialog to the target's map peer.
    auto target_peer = FindMapPeer(ctx, tgt_msi);
    if (!target_peer)
    {
        spdlog::info("OnPartyAddAck[{}]: target='{}' main_server_id={} "
                     "offline — invite dropped", ip, target_name, tgt_msi);
        co_return;
    }
    co_await senders::SendMwPartyAddReq(target_peer, tgt_char_id, tgt_key,
        request_name, target_name, obtain_type, party::kAgree, req_char_id);
    {
        std::lock_guard g(target->lock);
        target->party_waiter = true;
    }
    spdlog::info("OnPartyAddAck[{}]: char_id={} invited '{}' (msi={}) "
                 "obtain={} — AGREE relayed", ip, req_char_id, target_name,
        tgt_msi, obtain_type);
    co_return;
}

boost::asio::awaitable<void>
OnPartyJoinAck(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnPartyJoinAck[{}]: registries not wired — dropped", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::string   origin_name, target_name;
    std::uint8_t  obtain_type = 0, response = 0;
    std::uint32_t max_hp = 0, hp = 0, max_mp = 0, mp = 0;
    if (!r.ReadString(origin_name) || !r.ReadString(target_name) ||
        !r.Read(obtain_type) || !r.Read(response) ||
        !r.Read(max_hp) || !r.Read(hp) || !r.Read(max_mp) || !r.Read(mp))
    {
        spdlog::warn("OnPartyJoinAck[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }

    auto origin = ctx.chars->FindByName(origin_name); // inviter / chief
    auto target = ctx.chars->FindByName(target_name); // invitee answering

    // The dialog is resolved either way — clear the waiter latch.
    if (target)
    {
        std::lock_guard g(target->lock);
        target->party_waiter = false;
    }
    if (!origin && !target) co_return;

    std::uint32_t o_char_id = 0, o_key = 0, o_party_id = 0;
    std::uint8_t  o_country = 0, o_aid = 0, o_msi = 0;
    if (origin)
    {
        std::lock_guard g(origin->lock);
        o_char_id  = origin->char_id;
        o_key      = origin->key;
        o_party_id = origin->party_id;
        o_country  = origin->country;
        o_aid      = origin->aid_country;
        o_msi      = origin->main_server_id;
    }
    std::uint32_t t_char_id = 0, t_key = 0, t_party_id = 0;
    std::uint8_t  t_country = 0, t_aid = 0, t_msi = 0;
    if (target)
    {
        std::lock_guard g(target->lock);
        t_char_id  = target->char_id;
        t_key      = target->key;
        t_party_id = target->party_id;
        t_country  = target->country;
        t_aid      = target->aid_country;
        t_msi      = target->main_server_id;
    }

    auto origin_peer = FindMapPeer(ctx, o_msi);
    auto target_peer = FindMapPeer(ctx, t_msi);

    auto reply = [&](std::shared_ptr<PeerSession> p, std::uint32_t char_id,
                     std::uint32_t key, std::uint8_t result)
        -> boost::asio::awaitable<void> {
        if (p)
            co_await senders::SendMwPartyAddReq(p, char_id, key, origin_name,
                target_name, obtain_type, result, /*request=*/0);
    };

    if (!origin)
    {
        co_await reply(target_peer, t_char_id, t_key, party::kNoReqUser);
        co_return;
    }
    if (!target)
    {
        co_await reply(origin_peer, o_char_id, o_key, party::kNoUser);
        co_return;
    }
    if (response != party::kAskYes)
    {
        // Invitee declined / was busy — relay their answer code
        // (ASK_NO/ASK_BUSY align with PARTY_DENY/PARTY_BUSY).
        co_await reply(origin_peer, o_char_id, o_key, response);
        co_return;
    }
    if (t_party_id != 0)
    {
        // Invitee joined some party between the AGREE and this ack.
        co_await reply(origin_peer, o_char_id, o_key, party::kNoUser);
        co_return;
    }
    if (party::WarCountry(t_country, t_aid) !=
        party::WarCountry(o_country, o_aid))
    {
        co_await reply(target_peer, t_char_id, t_key, party::kCountry);
        co_await reply(origin_peer, o_char_id, o_key, party::kCountry);
        co_return;
    }

    // SetCharStatus(pTarget): stash the invitee's combat stats for
    // the JOIN broadcast.
    {
        std::lock_guard g(target->lock);
        target->max_hp = max_hp;
        target->hp     = hp;
        target->max_mp = max_mp;
        target->mp     = mp;
    }

    // Legacy bails if either map is offline (it can't fan the JOIN
    // packets out to a missing peer).
    if (!target_peer || !origin_peer)
    {
        spdlog::info("OnPartyJoinAck[{}]: origin msi={} or target msi={} "
                     "offline — formation aborted", ip, o_msi, t_msi);
        co_return;
    }

    // Inviter already in a party → invitee joins it (chief-gated).
    if (o_party_id != 0)
    {
        auto party = ctx.parties ? ctx.parties->Find(o_party_id) : nullptr;
        if (party)
        {
            bool         is_chief = false, arena = false;
            std::uint8_t size     = 0;
            {
                std::lock_guard pg(party->lock);
                is_chief = party->IsChief(o_char_id);
                size     = party->Size();
                arena    = party->arena;
            }
            if (!is_chief)
            {
                co_await reply(origin_peer, o_char_id, o_key, party::kNotChief);
                co_await reply(target_peer, t_char_id, t_key, party::kNotChief);
                co_return;
            }
            if (size >= party::kMaxPartyMember)
            {
                co_await reply(origin_peer, o_char_id, o_key, party::kFull);
                co_await reply(target_peer, t_char_id, t_key, party::kFull);
                co_return;
            }
            if (arena)
            {
                co_await reply(target_peer, t_char_id, t_key, party::kBusy);
                co_return;
            }
            co_await JoinPartyFanout(ctx, party, t_char_id);
            spdlog::info("OnPartyJoinAck[{}]: char_id={} joined party {} "
                         "(chief={})", ip, t_char_id, o_party_id, o_char_id);
            co_return;
        }
        spdlog::warn("OnPartyJoinAck[{}]: inviter char_id={} has stale "
                     "party_id={} — forming a fresh party",
            ip, o_char_id, o_party_id);
    }

    // New party: inviter becomes chief, both join.
    if (!ctx.parties)
    {
        spdlog::warn("OnPartyJoinAck[{}]: party registry not wired — "
                     "cannot form party", ip);
        co_return;
    }
    auto party = std::make_shared<TParty>();
    party->obtain_type   = obtain_type;
    party->chief_char_id = o_char_id;
    bool inserted = false;
    for (int i = 0; i < 4 && !inserted; ++i)
    {
        const std::uint16_t id = ctx.parties->GenId();
        if (id == 0) break;
        party->id = id;
        inserted = ctx.parties->Insert(party);
    }
    if (!inserted)
    {
        spdlog::error("OnPartyJoinAck[{}]: could not allocate a party id — "
                      "dropped", ip);
        co_return;
    }

    co_await JoinPartyFanout(ctx, party, o_char_id); // chief first (empty)
    co_await JoinPartyFanout(ctx, party, t_char_id); // then the invitee
    spdlog::info("OnPartyJoinAck[{}]: formed party {} chief={} member={}",
        ip, party->id, o_char_id, t_char_id);
    co_return;
}

namespace {

// Push a char a MW_PARTYATTR_REQ if their map is online. Small
// shared helper for the succession refresh + the leaver's clear.
boost::asio::awaitable<void>
SendAttr(const HandlerContext& ctx, std::uint32_t char_id,
         std::uint16_t party_id, std::uint8_t obtain,
         std::uint32_t chief_id)
{
    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;
    std::uint32_t key = 0;
    std::uint8_t  msi = 0;
    {
        std::lock_guard g(c->lock);
        key = c->key;
        msi = c->main_server_id;
    }
    if (auto p = FindMapPeer(ctx, msi))
        co_await senders::SendMwPartyAttrReq(p, char_id, key, party_id,
            obtain, chief_id, /*commander=*/0);
}

// Remove `leaver_id` from `party`, mirroring legacy
// CTWorldSvrModule::LeaveParty. Survives if ≥2 members remain
// (with chief succession when the leaver was chief), else disbands
// — pulling the last member out (the recursive is_delete=false
// call) and dropping the party from the registry.
//
// Self-recursive coroutine → forward-declared.
boost::asio::awaitable<void>
LeaveParty(const HandlerContext& ctx, std::shared_ptr<TParty> party,
           std::uint32_t leaver_id, std::uint8_t kick, bool is_delete);

boost::asio::awaitable<void>
LeaveParty(const HandlerContext& ctx, std::shared_ptr<TParty> party,
           std::uint32_t leaver_id, std::uint8_t kick, bool is_delete)
{
    std::vector<std::uint32_t> members_before;
    std::uint16_t pid         = 0;
    std::uint8_t  obtain      = 0;
    std::uint32_t chief_after = 0;
    bool          will_delete = false;
    bool          succession  = false;
    {
        std::lock_guard pg(party->lock);
        pid            = party->id;
        obtain         = party->obtain_type;
        members_before = party->members;
        const std::uint8_t size = party->Size();
        const bool leaver_was_chief = party->IsChief(leaver_id);
        if (size > 2 || !is_delete)
        {
            // Chief succession: promote the first other member
            // (legacy GetNextChief). Non-chief leave leaves it be.
            if (leaver_was_chief)
                for (auto mid : party->members)
                    if (mid != leaver_id)
                    {
                        party->chief_char_id = mid;
                        succession = true;
                        break;
                    }
            chief_after = party->chief_char_id;
        }
        else
        {
            party->chief_char_id = 0;
            chief_after          = 0;
            will_delete          = true;
        }
    }

    // Succession refresh: every member (incl. the leaver) gets the
    // new chief before the DEL fan-out (legacy PartyAttr loop).
    if (succession)
        for (auto mid : members_before)
            co_await SendAttr(ctx, mid, pid, obtain, chief_after);

    // DEL fan-out to all members. The leaver hears chief=0/party=0;
    // survivors hear the surviving chief + party id.
    for (auto mid : members_before)
    {
        auto c = ctx.chars->Find(mid);
        if (!c) continue;
        std::uint32_t mkey = 0;
        std::uint8_t  mmsi = 0;
        {
            std::lock_guard g(c->lock);
            mkey = c->key;
            mmsi = c->main_server_id;
        }
        auto p = FindMapPeer(ctx, mmsi);
        if (!p) continue;
        const std::uint32_t chief_field = (mid != leaver_id) ? chief_after : 0;
        const std::uint16_t party_field = chief_field ? pid : 0;
        co_await senders::SendMwPartyDelReq(p, mid, mkey, leaver_id,
            chief_field, /*commander=*/0, party_field, kick);
    }

    // Commit the removal + clear the leaver's back-pointer, then
    // push it a cleared PARTYATTR (legacy PartyAttr(pChar) after
    // DelMember leaves m_pParty null).
    {
        std::lock_guard pg(party->lock);
        party->RemoveMember(leaver_id);
    }
    if (auto lc = ctx.chars->Find(leaver_id))
    {
        std::lock_guard g(lc->lock);
        lc->party_id = 0;
    }
    co_await SendAttr(ctx, leaver_id, /*party_id=*/0, /*obtain=*/0,
        /*chief=*/0);

    // Disband cascade: pull the last member out, then drop the
    // (now-empty) party from the registry.
    if (will_delete)
    {
        std::uint32_t last = 0;
        {
            std::lock_guard pg(party->lock);
            if (!party->members.empty()) last = party->members.front();
        }
        if (last != 0)
            co_await LeaveParty(ctx, party, last, /*kick=*/0,
                /*is_delete=*/false);
        if (ctx.parties) ctx.parties->Remove(pid);
    }
    co_return;
}

} // namespace

boost::asio::awaitable<void>
OnPartyDelAck(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.parties)
    {
        spdlog::warn("OnPartyDelAck[{}]: registries not wired — dropped", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint16_t party_id = 0;
    std::uint32_t char_id  = 0;
    std::uint8_t  kick     = 0;
    if (!r.Read(party_id) || !r.Read(char_id) || !r.Read(kick))
    {
        spdlog::warn("OnPartyDelAck[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }

    auto party = ctx.parties->Find(party_id);
    if (!party)
    {
        spdlog::info("OnPartyDelAck[{}]: party {} not found — drop",
            ip, party_id);
        co_return;
    }
    bool is_member = false;
    {
        std::lock_guard pg(party->lock);
        is_member = party->IsMember(char_id);
    }
    if (!is_member)
    {
        spdlog::info("OnPartyDelAck[{}]: char_id={} not in party {} — drop",
            ip, char_id, party_id);
        co_return;
    }

    co_await LeaveParty(ctx, party, char_id, kick, /*is_delete=*/true);
    spdlog::info("OnPartyDelAck[{}]: char_id={} left party {} (kick={})",
        ip, char_id, party_id, kick);
    co_return;
}

boost::asio::awaitable<void>
OnPartyManstatAck(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.parties)
    {
        spdlog::warn("OnPartyManstatAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint16_t party_id  = 0;
    std::uint32_t member_id = 0;
    std::uint8_t  type      = 0, level = 0;
    std::uint32_t max_hp = 0, hp = 0, max_mp = 0, mp = 0;
    if (!r.Read(party_id) || !r.Read(member_id) || !r.Read(type) ||
        !r.Read(level) || !r.Read(max_hp) || !r.Read(hp) ||
        !r.Read(max_mp) || !r.Read(mp))
    {
        spdlog::warn("OnPartyManstatAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto party = ctx.parties->Find(party_id);
    if (!party) co_return;

    std::vector<std::uint32_t> members;
    {
        std::lock_guard pg(party->lock);
        members = party->members;
    }

    // Update the subject member's stored combat stats (legacy
    // SetCharStatus — HP/MP only; level stays owned by LEVELUP).
    if (auto subject = ctx.chars->Find(member_id))
    {
        std::lock_guard g(subject->lock);
        subject->max_hp = max_hp;
        subject->hp     = hp;
        subject->max_mp = max_mp;
        subject->mp     = mp;
    }

    // Broadcast the new stats to every member's map.
    for (auto mid : members)
    {
        auto c = ctx.chars->Find(mid);
        if (!c) continue;
        std::uint32_t mkey = 0;
        std::uint8_t  mmsi = 0;
        {
            std::lock_guard g(c->lock);
            mkey = c->key;
            mmsi = c->main_server_id;
        }
        if (auto p = FindMapPeer(ctx, mmsi))
            co_await senders::SendMwPartyManstatReq(p, mid, mkey, member_id,
                type, level, max_hp, hp, max_mp, mp);
    }
    co_return;
}

boost::asio::awaitable<void>
OnChgPartyChiefAck(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       body,
                   const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.parties)
    {
        spdlog::warn("OnChgPartyChiefAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t chief_id = 0, key = 0, target_id = 0;
    if (!r.Read(chief_id) || !r.Read(key) || !r.Read(target_id))
    {
        spdlog::warn("OnChgPartyChiefAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto chief = ctx.chars->Find(chief_id);
    if (!chief) co_return;
    std::uint32_t actual_key = 0, chief_party = 0;
    {
        std::lock_guard g(chief->lock);
        actual_key  = chief->key;
        chief_party = chief->party_id;
    }
    if (actual_key != key) co_return;

    auto reply = [&](std::uint8_t result) -> boost::asio::awaitable<void> {
        co_await senders::SendMwChgPartyChiefReq(peer, chief_id, key, result);
    };

    auto target = ctx.chars->Find(target_id);
    if (!target)
    {
        co_await reply(party::kNoUser);
        co_return;
    }
    std::uint32_t target_party = 0;
    {
        std::lock_guard g(target->lock);
        target_party = target->party_id;
    }

    if (chief_party == 0 || target_party == 0)
    {
        co_await reply(party::kNoParty);
        co_return;
    }
    if (chief_party != target_party)
    {
        co_await reply(party::kNoUser);
        co_return;
    }
    if (chief_id == target_id)
    {
        co_await reply(party::kAlready);
        co_return;
    }

    auto party = ctx.parties->Find(static_cast<std::uint16_t>(chief_party));
    if (!party)
    {
        co_await reply(party::kNoParty);
        co_return;
    }

    bool is_chief = false;
    {
        std::lock_guard pg(party->lock);
        is_chief = party->IsChief(chief_id);
    }
    if (!is_chief)
    {
        co_await reply(party::kNotChief);
        co_return;
    }

    // Promote the target + snapshot the roster for the refresh.
    std::vector<std::uint32_t> members;
    std::uint8_t obtain = 0;
    {
        std::lock_guard pg(party->lock);
        party->chief_char_id = target_id;
        members = party->members;
        obtain  = party->obtain_type;
    }

    co_await reply(party::kChgChief);
    for (auto mid : members)
        co_await SendAttr(ctx, mid, static_cast<std::uint16_t>(chief_party),
            obtain, target_id);

    spdlog::info("OnChgPartyChiefAck[{}]: party {} chief {}→{}",
        ip, chief_party, chief_id, target_id);
    co_return;
}

boost::asio::awaitable<void>
OnChgPartyTypeAck(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.parties)
    {
        spdlog::warn("OnChgPartyTypeAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  party_type = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(party_type))
    {
        spdlog::warn("OnChgPartyTypeAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto requester = ctx.chars->Find(char_id);
    if (!requester) co_return;
    std::uint32_t actual_key = 0, party_id32 = 0;
    {
        std::lock_guard g(requester->lock);
        actual_key = requester->key;
        party_id32 = requester->party_id;
    }
    if (actual_key != key) co_return;
    if (party_id32 == 0) co_return; // not in a party — legacy silent drop

    auto party = ctx.parties->Find(static_cast<std::uint16_t>(party_id32));
    if (!party) co_return;

    bool is_chief = false;
    {
        std::lock_guard pg(party->lock);
        is_chief = party->IsChief(char_id);
    }
    if (!is_chief)
    {
        co_await senders::SendMwChgPartyTypeReq(peer, char_id, key,
            party::kNotChief, party_type);
        co_return;
    }

    std::vector<std::uint32_t> members;
    {
        std::lock_guard pg(party->lock);
        party->obtain_type = party_type;
        members = party->members;
    }

    for (auto mid : members)
    {
        auto c = ctx.chars->Find(mid);
        if (!c) continue;
        std::uint32_t mkey = 0;
        std::uint8_t  mmsi = 0;
        {
            std::lock_guard g(c->lock);
            mkey = c->key;
            mmsi = c->main_server_id;
        }
        if (auto p = FindMapPeer(ctx, mmsi))
            co_await senders::SendMwChgPartyTypeReq(p, mid, mkey,
                /*result=*/0, party_type);
    }
    spdlog::info("OnChgPartyTypeAck[{}]: party {} obtain_type={}",
        ip, party_id32, party_type);
    co_return;
}

boost::asio::awaitable<void>
OnPartyMemberRecallAck(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnPartyMemberRecallAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  inven_id = 0, item_id = 0;
    std::string   origin_name, target_name;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(inven_id) ||
        !r.Read(item_id) || !r.ReadString(origin_name) ||
        !r.ReadString(target_name))
    {
        spdlog::warn("OnPartyMemberRecallAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto self = ctx.chars->Find(char_id);
    if (!self) co_return;
    std::uint32_t actual_key = 0, self_party = 0;
    std::uint16_t self_map = 0;
    std::uint8_t  self_country = 0, self_aid = 0;
    std::string   self_name;
    {
        std::lock_guard g(self->lock);
        actual_key   = self->key;
        self_party   = self->party_id;
        self_map     = self->map_id;
        self_country = self->country;
        self_aid     = self->aid_country;
        self_name    = self->name;
    }
    if (actual_key != key) co_return;

    auto busy = [&](const std::string& who, std::uint8_t type)
        -> boost::asio::awaitable<void> {
        co_await senders::SendMwPartyMemberRecallReq(peer, char_id, key,
            party::kItemUseTargetBusy, who, type, 0, 0, 0, 0,
            0.0f, 0.0f, 0.0f);
    };

    if (origin_name == self_name)
    {
        // Summon the target to me (TP_RECALL): target must be in my
        // party + on my map.
        auto target = ctx.chars->FindByName(target_name);
        std::uint32_t t_party = 0, t_charid = 0, t_key = 0;
        std::uint16_t t_map = 0;
        std::uint8_t  t_msi = 0;
        if (target)
        {
            std::lock_guard g(target->lock);
            t_party  = target->party_id;
            t_charid = target->char_id;
            t_key    = target->key;
            t_map    = target->map_id;
            t_msi    = target->main_server_id;
        }
        if (target && t_party != 0 && t_map == self_map &&
            t_party == self_party)
        {
            if (auto tp = FindMapPeer(ctx, t_msi))
                co_await senders::SendMwPartyMemberRecallAnsReq(tp, t_charid,
                    t_key, origin_name, party::kTpRecall, inven_id, item_id);
            co_return;
        }
        co_await busy(target_name, party::kTpRecall);
        co_return;
    }

    // Move me to the origin (TP_MOVETO): origin must be on my map +
    // same war-country.
    auto origin = ctx.chars->FindByName(origin_name);
    std::uint32_t o_charid = 0, o_key = 0;
    std::uint16_t o_map = 0;
    std::uint8_t  o_country = 0, o_aid = 0, o_msi = 0;
    if (origin)
    {
        std::lock_guard g(origin->lock);
        o_charid  = origin->char_id;
        o_key     = origin->key;
        o_map     = origin->map_id;
        o_country = origin->country;
        o_aid     = origin->aid_country;
        o_msi     = origin->main_server_id;
    }
    if (origin && o_map == self_map &&
        party::WarCountry(o_country, o_aid) ==
            party::WarCountry(self_country, self_aid))
    {
        if (auto op = FindMapPeer(ctx, o_msi))
            co_await senders::SendMwPartyMemberRecallAnsReq(op, o_charid,
                o_key, target_name, party::kTpMoveTo, inven_id, item_id);
        co_return;
    }
    co_await busy(origin_name, party::kTpMoveTo);
    co_return;
}

boost::asio::awaitable<void>
OnPartyMemberRecallAnsAck(std::shared_ptr<PeerSession> peer,
                          std::vector<std::byte>       body,
                          const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnPartyMemberRecallAnsAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t  result = 0, type = 0, inven_id = 0, item_id = 0, channel = 0;
    std::string   user_name, target_name;
    std::uint16_t map_id = 0;
    float         pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
    if (!r.Read(result) || !r.ReadString(user_name) ||
        !r.ReadString(target_name) || !r.Read(type) || !r.Read(inven_id) ||
        !r.Read(item_id) || !r.Read(channel) || !r.Read(map_id) ||
        !r.Read(pos_x) || !r.Read(pos_y) || !r.Read(pos_z))
    {
        spdlog::warn("OnPartyMemberRecallAnsAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    auto user = ctx.chars->FindByName(user_name);
    if (!user) co_return;
    std::uint32_t u_charid = 0, u_key = 0;
    std::uint16_t u_map = 0;
    std::uint8_t  u_msi = 0;
    {
        std::lock_guard g(user->lock);
        u_charid = user->char_id;
        u_key    = user->key;
        u_map    = user->map_id;
        u_msi    = user->main_server_id;
    }
    auto user_peer = FindMapPeer(ctx, u_msi);
    if (!user_peer) co_return;

    // The teleport target must still be on the destination map and
    // not inside a small meeting room.
    if (u_map != map_id) result = party::kItemUseTargetBusy;
    if (party::IsSmallMeetingRoom(u_map)) result = party::kItemUseTargetBusy;

    co_await senders::SendMwPartyMemberRecallReq(user_peer, u_charid, u_key,
        result, target_name, type, inven_id, item_id, channel, map_id,
        pos_x, pos_y, pos_z);
    co_return;
}

boost::asio::awaitable<void>
OnPartyOrderTakeItemAck(std::shared_ptr<PeerSession> peer,
                        std::vector<std::byte>       body,
                        const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.parties)
    {
        spdlog::warn("OnPartyOrderTakeItemAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, mon_id = 0;
    std::uint16_t party_id = 0, map_id = 0, temp_mon_id = 0;
    std::uint8_t  server_id = 0, channel = 0, count = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(party_id) ||
        !r.Read(server_id) || !r.Read(channel) || !r.Read(map_id) ||
        !r.Read(mon_id) || !r.Read(temp_mon_id) || !r.Read(count))
    {
        spdlog::warn("OnPartyOrderTakeItemAck[{}]: short header ({} bytes)",
            ip, body.size());
        co_return;
    }
    std::vector<std::uint32_t> eligible;
    eligible.reserve(count);
    for (std::uint8_t i = 0; i < count; ++i)
    {
        std::uint32_t mid = 0;
        if (!r.Read(mid))
        {
            spdlog::warn("OnPartyOrderTakeItemAck[{}]: short member list", ip);
            co_return;
        }
        eligible.push_back(mid);
    }
    TGuildCabinetItem item;
    if (!ReadCabinetItem(r, item))
    {
        spdlog::warn("OnPartyOrderTakeItemAck[{}]: bad item payload", ip);
        co_return;
    }

    auto party = ctx.parties->Find(party_id);
    if (!party)
    {
        co_await senders::SendMwAddItemResultReq(peer, char_id, key, channel,
            map_id, mon_id, item.item_id_b, party::kMonItemTakeNotFound);
        co_return;
    }

    std::uint32_t next_id = 0;
    {
        std::lock_guard pg(party->lock);
        next_id = party->GetNextOrder(eligible);
    }
    if (next_id == 0) co_return; // no eligible member online — drop

    auto next = ctx.chars->Find(next_id);
    if (!next) co_return;
    std::uint32_t next_key = 0;
    std::uint8_t  next_msi = 0;
    {
        std::lock_guard g(next->lock);
        next_key = next->key;
        next_msi = next->main_server_id;
    }
    if (auto np = FindMapPeer(ctx, next_msi))
        co_await senders::SendMwPartyOrderTakeItemReq(np, next_id, next_key,
            server_id, channel, map_id, mon_id, temp_mon_id, item);
    co_return;
}

namespace {

// Snapshot a char's party_id (0 if absent / not in registry).
std::uint16_t PartyIdOf(const HandlerContext& ctx, std::uint32_t char_id)
{
    auto c = ctx.chars->Find(char_id);
    if (!c) return 0;
    std::lock_guard g(c->lock);
    return c->party_id;
}

std::uint8_t PartySize(const HandlerContext& ctx, std::uint16_t party_id)
{
    auto p = ctx.parties->Find(party_id);
    if (!p) return 0;
    std::lock_guard g(p->lock);
    return p->Size();
}

} // namespace

boost::asio::awaitable<void>
OnPartyMoveAck(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.parties)
    {
        spdlog::warn("OnPartyMoveAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   target_name, dest_name;
    std::uint16_t target_party = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(target_name) ||
        !r.ReadString(dest_name) || !r.Read(target_party))
    {
        spdlog::warn("OnPartyMoveAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto reply = [&](std::uint8_t result) -> boost::asio::awaitable<void> {
        co_await senders::SendMwPartyMoveReq(peer, char_id, key, result);
    };

    // The general must exist with a matching key (map pre-validated
    // the commander authority).
    auto general = ctx.chars->Find(char_id);
    bool key_ok = false;
    if (general)
    { std::lock_guard g(general->lock); key_ok = (general->key == key); }
    if (!general || !key_ok)
    {
        co_await reply(corps::kNotCommander);
        co_return;
    }

    auto target = ctx.chars->FindByName(target_name);
    std::uint32_t target_id = 0;
    if (target)
    { std::lock_guard g(target->lock); target_id = target->char_id; }
    const std::uint16_t tgt_party_id = target ? PartyIdOf(ctx, target_id) : 0;
    if (!target || tgt_party_id == 0)
    {
        co_await reply(corps::kNotCommander);
        co_return;
    }
    auto tg_party = ctx.parties->Find(tgt_party_id);

    if (!dest_name.empty())
    {
        // Swap mode: target ↔ dest, both parties must have ≥2.
        auto dest = ctx.chars->FindByName(dest_name);
        std::uint32_t dest_id = 0;
        if (dest)
        { std::lock_guard g(dest->lock); dest_id = dest->char_id; }
        const std::uint16_t dest_party_id = dest ? PartyIdOf(ctx, dest_id) : 0;
        if (!dest || dest_party_id == 0 ||
            PartySize(ctx, dest_party_id) <= 1 ||
            PartySize(ctx, tgt_party_id) <= 1)
        {
            co_await reply(corps::kWrongTarget);
            co_return;
        }
        auto dest_party = ctx.parties->Find(dest_party_id);
        if (!dest_party || !tg_party)
        {
            co_await reply(corps::kWrongTarget);
            co_return;
        }
        // Pull both out (no dissolve), then cross-join.
        co_await LeaveParty(ctx, dest_party, dest_id, /*kick=*/1,
            /*is_delete=*/false);
        co_await LeaveParty(ctx, tg_party, target_id, /*kick=*/1,
            /*is_delete=*/false);
        co_await JoinPartyFanout(ctx, dest_party, target_id);
        co_await JoinPartyFanout(ctx, tg_party, dest_id);
        spdlog::info("OnPartyMoveAck[{}]: swapped char {} (party {}) with "
                     "char {} (party {})", ip, target_id, tgt_party_id,
            dest_id, dest_party_id);
    }
    else
    {
        // Move mode: target → party `target_party`.
        auto dest_party = ctx.parties->Find(target_party);
        if (!dest_party || PartySize(ctx, target_party) == 0 ||
            tgt_party_id == target_party || !tg_party)
        {
            co_await reply(corps::kNotCommander);
            co_return;
        }
        co_await LeaveParty(ctx, tg_party, target_id, /*kick=*/1,
            /*is_delete=*/true);
        co_await JoinPartyFanout(ctx, dest_party, target_id);
        spdlog::info("OnPartyMoveAck[{}]: moved char {} from party {} to {}",
            ip, target_id, tgt_party_id, target_party);
    }

    co_await reply(corps::kSuccess);
    co_return;
}

boost::asio::awaitable<void>
OnEnterSoloMapAck(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.parties)
    {
        spdlog::warn("OnEnterSoloMapAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnEnterSoloMapAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;
    std::uint16_t party_id = 0;
    std::vector<std::uint8_t> cons;
    {
        std::lock_guard g(c->lock);
        if (c->key != key) co_return;
        party_id = c->party_id;
        for (const auto& con : c->cons)
            if (con.valid) cons.push_back(con.server_id);
    }

    // No party yet → spin up a solo party (legacy PT_SOLO).
    if (party_id == 0)
    {
        auto party = std::make_shared<TParty>();
        party->obtain_type   = party::kObtainSolo;
        party->chief_char_id = char_id;
        party->members.push_back(char_id);
        bool inserted = false;
        for (int i = 0; i < 4 && !inserted; ++i)
        {
            const std::uint16_t id = ctx.parties->GenId();
            if (id == 0) break;
            party->id = id;
            inserted = ctx.parties->Insert(party);
        }
        if (!inserted)
        {
            spdlog::error("OnEnterSoloMapAck[{}]: could not allocate a party "
                          "id — dropped", ip);
            co_return;
        }
        party_id = party->id;
        { std::lock_guard g(c->lock); c->party_id = party_id; }
    }

    // Snapshot the party meta for the per-connection mirror.
    std::uint8_t  obtain = party::kObtainSolo;
    std::uint32_t chief  = char_id;
    if (auto party = ctx.parties->Find(party_id))
    {
        std::lock_guard g(party->lock);
        obtain = party->obtain_type;
        chief  = party->chief_char_id;
    }

    for (auto msi : cons)
        if (auto p = FindMapPeer(ctx, msi))
            co_await senders::SendMwEnterSoloMapReq(p, char_id, key, party_id,
                obtain, chief);
    co_return;
}

boost::asio::awaitable<void>
OnLeaveSoloMapAck(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.parties)
    {
        spdlog::warn("OnLeaveSoloMapAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnLeaveSoloMapAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;
    std::uint16_t party_id = 0;
    {
        std::lock_guard g(c->lock);
        if (c->key != key) co_return;
        party_id = c->party_id;
    }
    if (party_id == 0) co_return;

    // Only a solo party is torn down on leave (legacy gate).
    bool is_solo = false;
    if (auto party = ctx.parties->Find(party_id))
    {
        std::lock_guard g(party->lock);
        is_solo = (party->obtain_type == party::kObtainSolo);
    }
    if (!is_solo) co_return;

    ctx.parties->Remove(party_id);
    { std::lock_guard g(c->lock); if (c->party_id == party_id) c->party_id = 0; }
    co_return;
}

} // namespace tworldsvr::handlers
