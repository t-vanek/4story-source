#include "handlers.h"
#include "party_authority.h"
#include "../senders/senders.h"
#include "../services/corps_constants.h"
#include "../services/guild_cabinet_codec.h"
#include "../services/party_constants.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

namespace tworldsvr::handlers {

namespace {

// Find the map peer serving a given main_server_id (msi), or
// nullptr if that map is offline. Mirrors legacy FindMapSvr —
// same helper the guild invite/answer relay paths use.
std::shared_ptr<PeerSession>
FindMapPeer(const HandlerContext& ctx, std::uint8_t msi)
{
    return communication::Map(ctx,msi);
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
    if (guild_id != 0) {
        auto g=ctx.guilds?ctx.guilds->Find(guild_id):nullptr;
        if(!g)return false; // Missing hydration is not source guildlessness.
        std::lock_guard gl(g->lock);info.guild_name=g->name;
    }
    return true;
}

boost::asio::awaitable<bool> SendAttr(const HandlerContext&,std::uint32_t,
    std::uint16_t,std::uint8_t,std::uint32_t);

boost::asio::awaitable<bool> AuthorizePartyRoster(const HandlerContext& ctx,
    const std::shared_ptr<TParty>& party,const std::shared_ptr<PeerSession>& peer,
    const std::shared_ptr<TChar>& actor) {
    if(!party||!actor)co_return false;
    if(!party_authority::CanMutate(ctx,actor))co_return false;
    const auto identity=co_await party_authority::Authorize(ctx,peer,actor);
    if(!identity.valid)co_return false;
    if(!ctx.chat_repo)co_return true; // explicit source cache adapter
    std::uint16_t id=0;std::uint32_t chief=0;std::uint8_t type=0;
    std::vector<std::uint32_t> members;
    {std::lock_guard pg(party->lock);
     if(party->corps_id||party->arena||!party->IsMember(actor->char_id))co_return false;
     id=party->id;chief=party->chief_char_id;type=party->obtain_type;members=party->members;}
    struct Route {std::shared_ptr<TChar> actor;std::shared_ptr<PeerSession> peer;std::uint32_t key;};
    std::vector<Route> routes;
    for(const auto mid:members) {
        auto member=ctx.chars->Find(mid);if(!member)co_return false;
        if(!party_authority::CanMutate(ctx,member))co_return false;
        std::uint8_t server=0;std::uint32_t key=0;
        {std::lock_guard g(member->lock);server=member->main_server_id;key=member->key;
         if(member->party_id!=id)co_return false;}
        auto map=FindMapPeer(ctx,server);
        const auto pair=co_await party_authority::AuthorizePair(ctx,peer,actor,map,member);
        if(pair[0]!=identity)co_return false;
        routes.push_back({member,map,key});
    }
    for(const auto& route:routes)if(!party_authority::Current(ctx,route.peer,route.actor,route.key))co_return false;
    if(ctx.parties->Find(id)!=party)co_return false;
    {std::lock_guard pg(party->lock);
     if(party->corps_id||party->arena||party->members!=members||party->chief_char_id!=chief||party->obtain_type!=type)co_return false;}
    co_return true;
}

// Add a char to an existing party and run the legacy JoinParty
// fan-out: announce each current member to the joiner and the
// joiner to each member (pairwise MW_PARTYJOIN_REQ), followed by
// MW_PARTYATTR_REQ. Native participants are preflighted before membership
// changes and pairwise reauthorized across suspended packet writes.
//
// Lock discipline: the party's member list + meta are snapshotted
// under the party lock and released before any char lock is taken,
// so a char lock is never held across the party lock.
boost::asio::awaitable<bool>
JoinPartyFanout(const HandlerContext& ctx,
                std::shared_ptr<TParty> party,
                std::uint32_t joining_char_id)
{
    senders::PartyMemberInfo joining_info;
    std::uint32_t joining_key = 0;
    std::uint8_t  joining_msi = 0;
    if (!SnapshotMember(ctx, joining_char_id, joining_info,
                        joining_key, joining_msi))
        co_return false;
    auto joining_peer = FindMapPeer(ctx, joining_msi);
    auto joining=ctx.chars->Find(joining_char_id);
    if(!party_authority::CanMutate(ctx,joining))co_return false;
    const auto joining_identity=co_await party_authority::Authorize(ctx,joining_peer,joining);
    if(!joining_identity.valid)co_return false;

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
    struct Member {
        std::shared_ptr<TChar> actor;std::shared_ptr<PeerSession> peer;
        senders::PartyMemberInfo info;std::uint32_t key;PartyIdentity identity;
    };
    std::vector<Member> roster;
    for(const auto mid:member_ids) {
        senders::PartyMemberInfo info;std::uint32_t key=0;std::uint8_t server=0;
        if(!SnapshotMember(ctx,mid,info,key,server)){if(ctx.chat_repo)co_return false;continue;}
        auto actor=ctx.chars->Find(mid);auto map=FindMapPeer(ctx,server);
        if(!party_authority::CanMutate(ctx,actor))co_return false;
        const auto identities=co_await party_authority::AuthorizePair(ctx,joining_peer,joining,map,actor);
        if(!identities[0].valid||identities[0]!=joining_identity) {
            if(ctx.chat_repo)co_return false;continue;
        }
        {std::lock_guard g(actor->lock);if(actor->party_id!=pid)co_return false;}
        roster.push_back({actor,map,std::move(info),key,identities[1]});
    }
    const auto current=[&] {
        if(ctx.parties->Find(pid)!=party||!party_authority::Current(ctx,joining_peer,joining,joining_key))return false;
        for(const auto& member:roster)if(!party_authority::Current(ctx,member.peer,member.actor,member.key))return false;
        return true;
    };
    if(!current())co_return false;
    {std::lock_guard pg(party->lock);
     if(party->members!=member_ids||party->chief_char_id!=chief_id||party->obtain_type!=obtain||
        (ctx.chat_repo&&party->corps_id!=0))co_return false;}
    // The complete local membership decision precedes suspended sends; source
    // packet order remains each pair of JOINs followed by the joining ATTR.
    {std::lock_guard pg(party->lock);if(!party->AddMember(joining_char_id))co_return false;}
    {std::lock_guard g(joining->lock);joining->party_id=pid;}

    for (const auto& member : roster)
    {
        auto identities=co_await party_authority::AuthorizePair(ctx,joining_peer,joining,member.peer,member.actor);
        if(!current()||identities[0]!=joining_identity||identities[1]!=member.identity)co_return false;
        co_await senders::SendMwPartyJoinReq(joining_peer,
            joining_info.char_id,joining_key,pid,chief_id,0,obtain,member.info);
        identities=co_await party_authority::AuthorizePair(ctx,joining_peer,joining,member.peer,member.actor);
        if(!current()||identities[0]!=joining_identity||identities[1]!=member.identity)co_return false;
        co_await senders::SendMwPartyJoinReq(member.peer,member.info.char_id,
            member.key,pid,chief_id,0,obtain,joining_info);
    }

    if(!current()||!co_await SendAttr(ctx,joining_char_id,pid,obtain,chief_id))co_return false;
    co_return current();
}

} // namespace

boost::asio::awaitable<void>
OnPartyAddAck(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    auto operation=co_await ctx.party_operations->Acquire();
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
        !r.Read(max_hp) || !r.Read(hp) || !r.Read(max_mp) || !r.Read(mp)||!r.Eof()||
        request_name.empty()||request_name.size()>50||request_name.find('\0')!=std::string::npos||
        target_name.find('\0')!=std::string::npos||obtain_type>party::kObtainOrder||hp>max_hp||mp>max_mp)
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
    const auto requester_identity=co_await party_authority::Authorize(ctx,peer,requester);
    if(!requester_identity.valid)co_return;
    if(!party_authority::CanMutate(ctx,requester)||
       (target&&!party_authority::CanMutate(ctx,target)))co_return;

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
        if(!party_authority::Current(ctx,peer,requester,req_key))co_return;
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
                if(ctx.chat_repo&&(!pty->IsMember(req_char_id)||pty->corps_id!=0))co_return;
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
            if(ctx.chat_repo)co_return;
            spdlog::warn("OnPartyAddAck[{}]: requester char_id={} has stale "
                         "party_id={} — treating as partyless",
                ip, req_char_id, req_party_id);
        }
    }

    // Forward the AGREE dialog to the target's map peer.
    auto target_peer = FindMapPeer(ctx, tgt_msi);
    if (!target_peer)
    {
        spdlog::info("OnPartyAddAck[{}]: target='{}' main_server_id={} "
                     "offline — invite dropped", ip, target_name, tgt_msi);
        co_return;
    }
    const auto identities=co_await party_authority::AuthorizePair(ctx,peer,requester,target_peer,target);
    if(!identities[0].valid||identities[0]!=requester_identity)co_return;
    const auto& target_identity=identities[1];
    {std::lock_guard g(requester->lock);
     requester->max_hp=max_hp;requester->hp=hp;requester->max_mp=max_mp;requester->mp=mp;}
    auto invitation=std::make_shared<PartyInvitation>();
    invitation->origin=requester;invitation->origin_peer=peer;invitation->target_peer=target_peer;
    invitation->origin_key=req_key;invitation->target_key=tgt_key;invitation->obtain=obtain_type;
    invitation->origin_name=request_name;
    invitation->origin_identity=requester_identity;invitation->target_identity=target_identity;
    {std::lock_guard g(target->lock);target->party_waiter=true;target->party_invite=invitation;}
    co_await senders::SendMwPartyAddReq(target_peer, tgt_char_id, tgt_key,
        request_name, target_name, obtain_type, party::kAgree, req_char_id);
    {
        std::lock_guard g(target->lock);
        if(!target_peer->Wire()->IsOpen()||target->closing||target->logout) {
            if(target->party_invite==invitation){target->party_waiter=false;target->party_invite.reset();}
        }
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
    auto operation=co_await ctx.party_operations->Acquire();
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
        !r.Read(max_hp) || !r.Read(hp) || !r.Read(max_mp) || !r.Read(mp)||!r.Eof()||
        origin_name.empty()||target_name.empty()||origin_name.size()>50||target_name.size()>50||origin_name.find('\0')!=std::string::npos||
        target_name.find('\0')!=std::string::npos||obtain_type>party::kObtainOrder||response>2||hp>max_hp||mp>max_mp)
    {
        spdlog::warn("OnPartyJoinAck[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }

    auto origin = ctx.chars->FindByName(origin_name); // inviter / chief
    auto target = ctx.chars->FindByName(target_name); // invitee answering
    if(!target)co_return;
    if(!party_authority::CanMutate(ctx,target)||
       (origin&&!party_authority::CanMutate(ctx,origin)))co_return;
    std::shared_ptr<const PartyInvitation> invitation;
    {std::lock_guard g(target->lock);invitation=target->party_invite;}
    if(!invitation||invitation->target_peer.lock()!=peer||invitation->obtain!=obtain_type||
       invitation->origin_name!=origin_name)co_return;
    const auto target_identity=co_await party_authority::Authorize(ctx,peer,target);
    if(!target_identity.valid||target_identity!=invitation->target_identity)co_return;
    auto expected_origin=invitation->origin.lock();
    auto expected_origin_peer=invitation->origin_peer.lock();
    if(origin!=expected_origin)origin.reset();
    if(origin) {
        const auto identity=co_await party_authority::Authorize(ctx,expected_origin_peer,origin);
        if(!identity.valid||identity!=invitation->origin_identity)origin.reset();
    }
    if(!party_authority::Current(ctx,peer,target,invitation->target_key))co_return;
    {std::lock_guard g(target->lock);
     if(target->party_invite!=invitation||target->key!=invitation->target_key)co_return;
     target->party_waiter=false;target->party_invite.reset();}
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
    if(origin&&(!party_authority::Current(ctx,origin_peer,origin,o_key)||
                origin_peer!=expected_origin_peer||o_key!=invitation->origin_key))co_return;

    auto reply = [&](std::shared_ptr<PeerSession> p, std::uint32_t char_id,
                     std::uint32_t key, std::uint8_t result)
        -> boost::asio::awaitable<void> {
        auto recipient=ctx.chars->Find(char_id);
        if (party_authority::Current(ctx,p,recipient,key))
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
    if(!party_authority::ModesCompatible(o_msi,t_msi))co_return;
    const auto identities=co_await party_authority::AuthorizePair(ctx,origin_peer,origin,target_peer,target);
    if(identities[0]!=invitation->origin_identity||identities[1]!=invitation->target_identity)co_return;

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
                if(ctx.chat_repo&&(!party->IsMember(o_char_id)||party->corps_id!=0))co_return;
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
            if(!co_await JoinPartyFanout(ctx, party, t_char_id)) {
                bool joined=false;{std::lock_guard g(target->lock);joined=target->party_id==party->id;}
                // A preflight rejection has no membership to undo. Once a JOIN
                // was published, a stale participant requires ordinary teardown
                // of the joining actor; never leave a successful partial roster.
                if(joined){operation.reset();co_await CloseChar(target,ctx);}
                co_return;
            }
            spdlog::info("OnPartyJoinAck[{}]: char_id={} joined party {} "
                         "(chief={})", ip, t_char_id, o_party_id, o_char_id);
            co_return;
        }
        if(ctx.chat_repo)co_return;
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
    senders::PartyMemberInfo origin_info,target_info;
    std::uint32_t origin_key=0,target_key=0;std::uint8_t origin_server=0,target_server=0;
    if(!SnapshotMember(ctx,o_char_id,origin_info,origin_key,origin_server)||
       !SnapshotMember(ctx,t_char_id,target_info,target_key,target_server)||
       !party_authority::Current(ctx,origin_peer,origin,o_key)||
       !party_authority::Current(ctx,target_peer,target,t_key))co_return;
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

    // Form both backlinks before the first suspension. Source order is the
    // chief's ATTR, target receiving chief JOIN, chief receiving target JOIN,
    // then target ATTR; no empty/half-formed room can accept another request.
    {std::lock_guard pg(party->lock);party->AddMember(o_char_id);party->AddMember(t_char_id);}
    {std::lock_guard g(origin->lock);origin->party_id=party->id;}
    {std::lock_guard g(target->lock);target->party_id=party->id;}
    if(!co_await SendAttr(ctx,o_char_id,party->id,obtain_type,o_char_id)) {
        operation.reset();co_await CloseChar(target,ctx);co_return;
    }
    auto current_pair=co_await party_authority::AuthorizePair(ctx,origin_peer,origin,target_peer,target);
    if(current_pair!=identities){operation.reset();co_await CloseChar(target,ctx);co_return;}
    co_await senders::SendMwPartyJoinReq(target_peer,t_char_id,t_key,party->id,o_char_id,0,obtain_type,origin_info);
    current_pair=co_await party_authority::AuthorizePair(ctx,origin_peer,origin,target_peer,target);
    if(current_pair!=identities){operation.reset();co_await CloseChar(target,ctx);co_return;}
    co_await senders::SendMwPartyJoinReq(origin_peer,o_char_id,o_key,party->id,o_char_id,0,obtain_type,target_info);
    current_pair=co_await party_authority::AuthorizePair(ctx,origin_peer,origin,target_peer,target);
    if(current_pair!=identities||!co_await SendAttr(ctx,t_char_id,party->id,obtain_type,o_char_id)) {
        operation.reset();co_await CloseChar(target,ctx);co_return;
    }
    spdlog::info("OnPartyJoinAck[{}]: formed party {} chief={} member={}",
        ip, party->id, o_char_id, t_char_id);
    co_return;
}

namespace {

// Push a char a MW_PARTYATTR_REQ if their map is online. Small
// shared helper for the succession refresh + the leaver's clear.
boost::asio::awaitable<bool>
SendAttr(const HandlerContext& ctx, std::uint32_t char_id,
         std::uint16_t party_id, std::uint8_t obtain,
         std::uint32_t chief_id)
{
    auto c = ctx.chars->Find(char_id);
    if (!c) co_return false;
    // Keep later metadata behind any real removal which was never sent.
    // Ready recovery drains those events before publishing current attributes.
    if(!party_authority::CanMutate(ctx,c))co_return false;
    std::uint32_t key = 0;
    std::uint8_t  msi = 0;
    {
        std::lock_guard g(c->lock);
        key = c->key;
        msi = c->main_server_id;
    }
    auto main=FindMapPeer(ctx,msi);
    if(!(co_await party_authority::Authorize(ctx,main,c)).valid)co_return false;
    std::vector<std::uint8_t> routes{msi};
    {std::lock_guard g(c->lock);
     for(const auto& con:c->cons)if(con.valid&&con.ready&&con.server_id!=msi)routes.push_back(con.server_id);}
    for(const auto server:routes) {
        if(!party_authority::Current(ctx,main,c,key))co_return false;
        auto p=FindMapPeer(ctx,server);if(!p||!p->Wire()->IsOpen())continue;
        bool current=false;
        {std::lock_guard g(c->lock);
         current=std::any_of(c->cons.begin(),c->cons.end(),[server](const auto& con){return con.server_id==server&&con.valid&&con.ready;});}
        if(current)co_await senders::SendMwPartyAttrReq(p,char_id,key,party_id,obtain,chief_id,0);
    }
    co_return party_authority::Current(ctx,main,c,key);
}

boost::asio::awaitable<bool> FlushDeferredPartyRemovals(const HandlerContext& ctx,
    const std::shared_ptr<PeerSession>& main,const std::shared_ptr<TChar>& character,std::uint32_t key) {
    // Caller holds party_operations and has independently proved SQL Ready.
    // Pop before the single attempt: a completed/failed/unknown network write
    // cannot turn into another client DEL on the next ready probe.
    while(party_authority::Current(ctx,main,character,key)) {
        DeferredPartyRemoval event;
        {std::lock_guard g(character->lock);
         if(character->deferred_party_removals.empty())co_return true;
         event=character->deferred_party_removals.front();
         character->deferred_party_removals.erase(character->deferred_party_removals.begin());}
        co_await senders::SendMwPartyDelReq(main,character->char_id,key,event.member,
            event.chief,event.commander,event.party,event.kick);
    }
    co_return false;
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
    bool          defer_supported = false;
    {
        std::lock_guard pg(party->lock);
        pid            = party->id;
        obtain         = party->obtain_type;
        members_before = party->members;
        defer_supported=ctx.chat_repo&&party->corps_id==0;
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
        party->RemoveMember(leaver_id);
    }
    auto leaver=ctx.chars->Find(leaver_id);
    if(leaver){std::lock_guard g(leaver->lock);if(leaver->party_id==pid)leaver->party_id=0;}

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
        const std::uint32_t chief_field = (mid != leaver_id) ? chief_after : 0;
        const std::uint16_t party_field = chief_field ? pid : 0;
        if(!party_authority::Current(ctx,p,c,mkey)||!party_authority::CanMutate(ctx,c)) {
            if(defer_supported&&ctx.chars->Find(mid)==c) {
                std::lock_guard g(c->lock);
                if(c->key==mkey&&!c->closing&&!c->logout)
                    c->deferred_party_removals.push_back({leaver_id,chief_field,0,party_field,kick});
            }
            continue;
        }
        co_await senders::SendMwPartyDelReq(p, mid, mkey, leaver_id,
            chief_field, /*commander=*/0, party_field, kick);
    }

    // Membership/backlink removal preceded suspended writes. Now push the
    // source's cleared PARTYATTR after its DEL fan-out.
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

boost::asio::awaitable<void> RefreshReadyParty(const HandlerContext& ctx,
    const std::shared_ptr<PeerSession>& peer,const std::shared_ptr<TChar>& character,std::uint32_t key) {
    auto operation=co_await ctx.party_operations->Acquire();
    if(!ctx.chat_repo||!ctx.chars||!ctx.parties||!character)co_return;
    std::uint8_t server=0;
    {std::lock_guard g(character->lock);server=character->main_server_id;}
    auto main=FindMapPeer(ctx,server);const bool primary=peer==main;
    if(primary) {
        if(!(co_await party_authority::Authorize(ctx,peer,character)).valid)co_return;
    } else if(!(co_await party_authority::AuthorizeReplica(ctx,peer,main,character,key))[0].valid)co_return;
    if(!party_authority::Current(ctx,main,character,key))co_return;
    bool had_deferred=false;
    {std::lock_guard g(character->lock);had_deferred=!character->deferred_party_removals.empty();}
    if(!co_await FlushDeferredPartyRemovals(ctx,main,character,key))co_return;
    std::uint16_t id=0;std::uint8_t obtain=0;std::uint32_t chief=0;
    {std::lock_guard g(character->lock);id=character->party_id;}
    if(id) {
        auto party=ctx.parties->Find(id);if(!party)co_return;
        std::lock_guard pg(party->lock);
        if(!party->IsMember(character->char_id)||party->corps_id!=0)co_return;
        obtain=party->obtain_type;chief=party->chief_char_id;
    }
    if(primary||had_deferred) {
        // A deferred DEL went to the actual primary even when a ready replica
        // triggered recovery. Publish current metadata to that primary too:
        // the original Map forwards DEL with its previous cached attributes.
        // A replica which becomes ready later sends its own entered probe.
        if(!co_await SendAttr(ctx,character->char_id,id,obtain,chief))co_return;
    } else if(party_authority::CurrentReplica(ctx,peer,main,character,key))
        co_await senders::SendMwPartyAttrReq(peer,character->char_id,key,id,obtain,chief,0);
}

void InvalidatePartyInvitations(const HandlerContext& ctx,const std::shared_ptr<TChar>& character) {
    if(!ctx.chars||!character)return;
    for(const auto id:ctx.chars->SnapshotIds())if(auto target=ctx.chars->Find(id)) {
        std::lock_guard g(target->lock);
        if(target==character||(target->party_invite&&target->party_invite->origin.lock()==character)) {
            target->party_invite.reset();target->party_waiter=false;
        }
    }
}

boost::asio::awaitable<void> RetirePartyForClose(const HandlerContext& ctx,const std::shared_ptr<TChar>& character) {
    auto operation=co_await ctx.party_operations->Acquire();
    InvalidatePartyInvitations(ctx,character);
    std::uint16_t id=0;
    {std::lock_guard g(character->lock);id=character->party_id;}
    if(!id||!ctx.parties)co_return;
    auto party=ctx.parties->Find(id);if(!party)co_return;
    bool member=false;{std::lock_guard pg(party->lock);member=party->IsMember(character->char_id);}
    if(member)co_await LeaveParty(ctx,party,character->char_id,0,true);
}

boost::asio::awaitable<void>
OnPartyDelAck(std::shared_ptr<PeerSession> peer,
              std::vector<std::byte>       body,
              const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    auto operation=co_await ctx.party_operations->Acquire();
    if (!ctx.chars || !ctx.peers || !ctx.parties)
    {
        spdlog::warn("OnPartyDelAck[{}]: registries not wired — dropped", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint16_t party_id = 0;
    std::uint32_t char_id  = 0;
    std::uint8_t  kick     = 0;
    if (!r.Read(party_id) || !r.Read(char_id) || !r.Read(kick)||!r.Eof()||kick>1)
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
    std::uint32_t issuer=char_id;
    {std::lock_guard pg(party->lock);if(kick)issuer=party->chief_char_id;}
    if(kick&&issuer==char_id)co_return;
    auto actor=ctx.chars->Find(issuer);
    if(!party_authority::CanMutate(ctx,actor))co_return;
    if(kick) {
        auto target=ctx.chars->Find(char_id);std::uint8_t server=0;
        if(!party_authority::CanMutate(ctx,target))co_return;
        {std::lock_guard g(target->lock);server=target->main_server_id;}
        if(!(co_await party_authority::AuthorizePair(ctx,peer,actor,FindMapPeer(ctx,server),target))[0].valid)co_return;
    } else if(!(co_await party_authority::Authorize(ctx,peer,actor)).valid)co_return;
    if(ctx.parties->Find(party_id)!=party)co_return;
    {std::lock_guard pg(party->lock);
     if(!party->IsMember(char_id)||!party->IsMember(issuer)||(kick&&!party->IsChief(issuer)))co_return;}

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
    auto operation=co_await ctx.party_operations->Acquire();
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
        !r.Read(max_mp) || !r.Read(mp)||!r.Eof()||hp>max_hp||mp>max_mp)
    {
        spdlog::warn("OnPartyManstatAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto subject=ctx.chars->Find(member_id);
    if(!(co_await party_authority::Authorize(ctx,peer,subject)).valid)co_return;
    std::uint16_t actual_id=0;
    {std::lock_guard g(subject->lock);actual_id=subject->party_id;}
    auto party=ctx.parties->Find(actual_id);
    std::uint8_t obtain=0;std::uint32_t chief=0;
    if(actual_id) {
        if(!party)co_return;
        std::lock_guard pg(party->lock);
        if(!party->IsMember(member_id)||(ctx.chat_repo&&party->corps_id!=0))co_return;
        obtain=party->obtain_type;chief=party->chief_char_id;
    }
    const auto effective_id=obtain==party::kObtainSolo?0:actual_id;
    if(party_id!=effective_id) {
        if(!co_await SendAttr(ctx,member_id,actual_id,obtain,chief))co_return;
    }
    if(!party||party_id!=effective_id||!party_id)co_return;
    if(ctx.parties->Find(party_id)!=party)co_return;

    std::vector<std::uint32_t> members;
    {
        std::lock_guard pg(party->lock);
        if(!party->IsMember(member_id))co_return;
        members = party->members;
    }

    // Update the subject member's stored combat stats (legacy
    // SetCharStatus — HP/MP only; level stays owned by LEVELUP).
    if (subject)
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
        auto p=FindMapPeer(ctx,mmsi);
        const auto identities=co_await party_authority::AuthorizePair(ctx,peer,subject,p,c);
        if(identities[0].valid&&party_authority::Current(ctx,p,c,mkey))
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
    auto operation=co_await ctx.party_operations->Acquire();
    if (!ctx.chars || !ctx.peers || !ctx.parties)
    {
        spdlog::warn("OnChgPartyChiefAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t chief_id = 0, key = 0, target_id = 0;
    if (!r.Read(chief_id) || !r.Read(key) || !r.Read(target_id)||!r.Eof())
    {
        spdlog::warn("OnChgPartyChiefAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto chief = ctx.chars->Find(chief_id);
    if (!chief) co_return;
    const auto identity=co_await party_authority::Authorize(ctx,peer,chief);
    if(!identity.valid)co_return;
    std::uint32_t actual_key = 0, chief_party = 0;
    {
        std::lock_guard g(chief->lock);
        actual_key  = chief->key;
        chief_party = chief->party_id;
    }
    if (actual_key != key) co_return;

    auto reply = [&](std::uint8_t result) -> boost::asio::awaitable<void> {
        if(!party_authority::Current(ctx,peer,chief,key))co_return;
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
    // Source checks chief authority before different-party/self failures.
    if(chief_party!=target_party){co_await reply(party::kNoUser);co_return;}
    if(chief_id==target_id){co_await reply(party::kAlready);co_return;}
    if(!co_await AuthorizePartyRoster(ctx,party,peer,chief))co_return;

    // Promote the target + snapshot the roster for the refresh.
    std::vector<std::uint32_t> members;
    std::uint8_t obtain = 0;
    {
        std::lock_guard pg(party->lock);
        if(!party->IsChief(chief_id)||!party->IsMember(target_id))co_return;
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
    auto operation=co_await ctx.party_operations->Acquire();
    if (!ctx.chars || !ctx.peers || !ctx.parties)
    {
        spdlog::warn("OnChgPartyTypeAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  party_type = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(party_type)||!r.Eof()||party_type>party::kObtainOrder)
    {
        spdlog::warn("OnChgPartyTypeAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto requester = ctx.chars->Find(char_id);
    if (!requester) co_return;
    if(!(co_await party_authority::Authorize(ctx,peer,requester)).valid)co_return;
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
    if(!co_await AuthorizePartyRoster(ctx,party,peer,requester))co_return;

    std::vector<std::uint32_t> members;
    {
        std::lock_guard pg(party->lock);
        if(!party->IsChief(char_id)||!party->IsMember(char_id))co_return;
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
        if (auto p = FindMapPeer(ctx, mmsi);party_authority::Current(ctx,p,c,mkey)&&
            (co_await party_authority::Authorize(ctx,p,c)).valid)
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

// --- W6-28 arena join (legacy SSHandler.cpp:13477) ----------------

boost::asio::awaitable<void>
OnArenaJoinAck(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.parties)
    {
        spdlog::warn("OnArenaJoinAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, count = 0;
    std::uint8_t  join = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(join) || !r.Read(count))
    {
        spdlog::warn("OnArenaJoinAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    std::vector<std::uint32_t> keep_ids;
    keep_ids.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i)
    {
        std::uint32_t mid = 0;
        if (!r.Read(mid))
        {
            spdlog::warn("OnArenaJoinAck[{}]: truncated keep list (count={})",
                ip, count);
            co_return;
        }
        keep_ids.push_back(mid);
    }

    auto ch = ctx.chars->Find(char_id);
    if (!ch) co_return;

    std::uint16_t party_id = 0;
    bool          key_ok = true;
    {
        std::lock_guard g(ch->lock);
        if (ch->key != key) key_ok = false;
        else                party_id = ch->party_id;
    }
    if (!key_ok || party_id == 0) co_return;

    auto party = ctx.parties->Find(party_id);
    if (!party) co_return;

    // Flag flip — runs on both join=0 and join=1 (legacy
    // SSHandler.cpp:13495 sets m_bArena = bJoin before the
    // early-return on !bJoin).
    std::uint16_t corps_id = 0;
    std::vector<std::uint32_t> members_before;
    {
        std::lock_guard pg(party->lock);
        party->arena   = (join != 0);
        corps_id       = party->corps_id;
        members_before = party->members;
    }

    if (!join)
    {
        spdlog::info("OnArenaJoinAck[{}]: char_id={} party={} arena-LEAVE "
                     "flag flipped", ip, char_id, party_id);
        co_return;
    }

    // Arena-enter: leave the corps if we were in one (arena parties
    // must be standalone), then kick every member not in the
    // keep_ids set. Lock-ordering note: NotifyCorpsLeave takes the
    // corps lock + per-party locks via the registries; LeaveParty
    // takes the party lock. We don't hold any locks across either
    // call (the snapshot above released them all).
    if (corps_id != 0 && ctx.corps)
        if (auto corps = ctx.corps->Find(corps_id))
            co_await NotifyCorpsLeave(ctx, corps, party_id);

    std::unordered_set<std::uint32_t> keep_set(keep_ids.begin(),
                                                keep_ids.end());
    spdlog::info("OnArenaJoinAck[{}]: char_id={} party={} arena-JOIN — "
                 "keeping {}/{} members, corps_id={}",
        ip, char_id, party_id, keep_set.size(), members_before.size(),
        corps_id);
    for (auto mid : members_before)
    {
        if (keep_set.find(mid) != keep_set.end()) continue;
        // Legacy LeaveParty(it->second, TRUE) — kick=true so the
        // surviving members' DEL fan-out flags the kicked flag.
        co_await LeaveParty(ctx, party, mid, /*kick=*/1, /*is_delete=*/true);
    }
}

} // namespace tworldsvr::handlers
