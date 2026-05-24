#include "handlers.h"
#include "../senders/senders.h"
#include "../services/party_constants.h"
#include "../wire_codec.h"

#include <spdlog/spdlog.h>

#include <algorithm>
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

// A one-shot snapshot of the bits a TMS handler needs off a TChar,
// taken under the char's lock and used after release (no entity
// lock is held while a TTms lock is taken — README §5).
struct Route
{
    bool          found = false;
    std::uint32_t id    = 0;
    std::uint32_t key   = 0;
    std::uint8_t  msi   = 0;
    std::string   name;
    std::uint8_t  klass = 0;
    std::uint8_t  level = 0;
    std::uint8_t  country = 0;
    std::uint8_t  aid   = 0;
    std::vector<std::uint32_t> tms;   // the char's conference id-set
};

Route Snap(const HandlerContext& ctx, std::uint32_t cid)
{
    Route r;
    r.id = cid;
    auto c = ctx.chars->Find(cid);
    if (!c) return r;
    std::lock_guard g(c->lock);
    r.found   = true;
    r.key     = c->key;
    r.msi     = c->main_server_id;
    r.name    = c->name;
    r.klass   = c->klass;
    r.level   = c->level;
    r.country = c->country;
    r.aid     = c->aid_country;
    r.tms     = c->tms;
    return r;
}

bool HasTms(const Route& r, std::uint32_t tms_id)
{
    return std::find(r.tms.begin(), r.tms.end(), tms_id) != r.tms.end();
}

void AddTmsToChar(const HandlerContext& ctx, std::uint32_t cid,
                  std::uint32_t tms_id)
{
    auto c = ctx.chars->Find(cid);
    if (!c) return;
    std::lock_guard g(c->lock);
    if (std::find(c->tms.begin(), c->tms.end(), tms_id) == c->tms.end())
        c->tms.push_back(tms_id);
}

void RemoveTmsFromChar(const HandlerContext& ctx, std::uint32_t cid,
                       std::uint32_t tms_id)
{
    auto c = ctx.chars->Find(cid);
    if (!c) return;
    std::lock_guard g(c->lock);
    c->tms.erase(std::remove(c->tms.begin(), c->tms.end(), tms_id),
                 c->tms.end());
}

// Snapshot a conference's current member id list.
std::vector<std::uint32_t>
MemberIds(const std::shared_ptr<TTms>& tms)
{
    std::lock_guard g(tms->lock);
    return tms->members;
}

// Build the TMSINVITE_REQ roster (id/name/class/level) for the
// given member ids, dropping any that have since gone offline.
std::vector<senders::TmsMemberInfo>
Roster(const HandlerContext& ctx, const std::vector<std::uint32_t>& ids)
{
    std::vector<senders::TmsMemberInfo> out;
    out.reserve(ids.size());
    for (auto id : ids)
    {
        auto rt = Snap(ctx, id);
        if (!rt.found) continue;
        out.push_back({rt.id, rt.name, rt.klass, rt.level});
    }
    return out;
}

// Fan a TMSRECV to every current member of a conference.
boost::asio::awaitable<void>
BroadcastRecv(const HandlerContext& ctx,
              const std::vector<std::uint32_t>& member_ids,
              std::uint32_t tms_id, const std::string& sender_name,
              const std::string& message)
{
    for (auto id : member_ids)
    {
        auto rt = Snap(ctx, id);
        if (!rt.found) continue;
        if (auto p = FindMapPeer(ctx, rt.msi))
            co_await senders::SendMwTmsRecvReq(p, rt.id, rt.key, tms_id,
                                               sender_name, message);
    }
}

} // namespace

// --- SEND ----------------------------------------------------------
boost::asio::awaitable<void>
OnTmsSendAck(std::shared_ptr<PeerSession> peer,
             std::vector<std::byte>       body,
             const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.tms)
    {
        spdlog::warn("OnTmsSendAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, tms_id = 0;
    std::string   message;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(tms_id) ||
        !r.ReadString(message))
    {
        spdlog::warn("OnTmsSendAck[{}]: short body ({} bytes)", ip,
                     body.size());
        co_return;
    }

    auto sender = Snap(ctx, char_id);
    if (!sender.found || sender.key != key) co_return;
    if (!HasTms(sender, tms_id)) co_return;
    auto group = ctx.tms->Find(tms_id);
    if (!group) co_return;

    auto member_ids = MemberIds(group);

    // A solo conference re-pairs its last departed member: pop the
    // "rejoin?" dialog on their client instead of fanning the message.
    if (member_ids.size() == 1)
    {
        std::string last;
        { std::lock_guard g(group->lock); last = group->last_member; }
        auto target = last.empty() ? nullptr : ctx.chars->FindByName(last);
        if (target)
        {
            std::uint32_t t_id = 0, t_key = 0; std::uint8_t t_msi = 0;
            {
                std::lock_guard g(target->lock);
                t_id = target->char_id; t_key = target->key;
                t_msi = target->main_server_id;
            }
            if (auto p = FindMapPeer(ctx, t_msi))
                co_await senders::SendMwTmsInviteAskReq(
                    p, t_id, t_key, char_id, key, tms_id, message);
            co_return;
        }
        // No receiver: legacy substitutes a localized server message
        // (TMS_NORECEIVER); the server-message table isn't ported, so
        // the message is blanked (same deferral as the chat operator
        // sub-case) and echoed back to the lone member.
        message.clear();
    }

    co_await BroadcastRecv(ctx, member_ids, tms_id, sender.name, message);
}

// --- INVITEASK -----------------------------------------------------
boost::asio::awaitable<void>
OnTmsInviteAskAck(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.tms)
    {
        spdlog::warn("OnTmsInviteAskAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, target_id = 0, target_key = 0;
    std::uint8_t  result = 0;
    std::uint32_t tms_id = 0;
    std::string   message;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(target_id) ||
        !r.Read(target_key) || !r.Read(result) || !r.Read(tms_id) ||
        !r.ReadString(message))
    {
        spdlog::warn("OnTmsInviteAskAck[{}]: short body ({} bytes)", ip,
                     body.size());
        co_return;
    }

    auto sender = Snap(ctx, char_id);
    if (!sender.found || sender.key != key) co_return;
    if (!HasTms(sender, tms_id)) co_return;
    auto group = ctx.tms->Find(tms_id);
    if (!group) co_return;

    auto target = Snap(ctx, target_id);
    const bool accepted = target.found && target.key == target_key && result;

    if (accepted)
    {
        { std::lock_guard g(group->lock); group->AddMember(target_id); }
        AddTmsToChar(ctx, target_id, tms_id);

        // Announce the new roster to every member (the new target
        // included). Legacy keys the recipient header with the new
        // member's id + each member's own key.
        auto member_ids = MemberIds(group);
        auto roster = Roster(ctx, member_ids);
        for (auto id : member_ids)
        {
            auto rt = Snap(ctx, id);
            if (!rt.found) continue;
            if (auto p = FindMapPeer(ctx, rt.msi))
                co_await senders::SendMwTmsInviteReq(
                    p, target_id, rt.key, rt.id, tms_id, roster);
        }
    }
    else
        message.clear();   // NORECEIVER — see OnTmsSendAck note

    co_await BroadcastRecv(ctx, MemberIds(group), tms_id, sender.name,
                           message);
}

// --- INVITE --------------------------------------------------------
boost::asio::awaitable<void>
OnTmsInviteAck(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.tms)
    {
        spdlog::warn("OnTmsInviteAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, tms_id = 0;
    std::uint8_t  count = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(tms_id) ||
        !r.Read(count))
    {
        spdlog::warn("OnTmsInviteAck[{}]: short header ({} bytes)", ip,
                     body.size());
        co_return;
    }
    std::vector<std::uint32_t> targets;
    targets.reserve(count);
    for (std::uint8_t i = 0; i < count; ++i)
    {
        std::uint32_t t = 0;
        if (!r.Read(t)) { co_return; }
        targets.push_back(t);
    }

    auto sender = Snap(ctx, char_id);
    if (!sender.found || sender.key != key || count == 0) co_return;

    std::shared_ptr<TTms> group;
    if (tms_id)
    {
        if (!HasTms(sender, tms_id)) co_return;
        group = ctx.tms->Find(tms_id);
        if (!group) co_return;
    }

    // Resolve the invite targets: online, same war-country, deduped.
    const std::uint8_t sender_wc =
        party::WarCountry(sender.country, sender.aid);
    std::vector<std::uint32_t> members;
    for (auto t : targets)
    {
        if (std::find(members.begin(), members.end(), t) != members.end())
            continue;
        auto rt = Snap(ctx, t);
        if (!rt.found) continue;
        if (party::WarCountry(rt.country, rt.aid) != sender_wc) continue;
        members.push_back(t);
    }
    if (members.empty()) co_return;

    // 1:1 re-pair shortcut: inviting a single target who already has
    // a solo conference whose last departed member was us → rejoin
    // that conference rather than open a new one (legacy 7630-7663).
    if (!group && members.size() == 1)
    {
        auto target = Snap(ctx, members.front());
        for (auto tid : target.tms)
        {
            auto g = ctx.tms->Find(tid);
            if (!g) continue;
            bool match = false;
            { std::lock_guard lg(g->lock);
              match = g->members.size() == 1 && g->last_member == sender.name; }
            if (!match) continue;

            { std::lock_guard lg(g->lock); g->AddMember(char_id); }
            AddTmsToChar(ctx, char_id, tid);
            auto roster = Roster(ctx, MemberIds(g));
            if (auto p = FindMapPeer(ctx, sender.msi))
                co_await senders::SendMwTmsInviteReq(
                    p, char_id, sender.key, char_id, tid, roster);
            co_return;
        }
    }

    // Open a fresh conference if this isn't expanding an existing one.
    if (!group)
    {
        tms_id = ctx.tms->GenId();
        if (tms_id == 0) co_return;
        group = std::make_shared<TTms>();
        group->id = tms_id;
        group->members.push_back(char_id);
        if (!ctx.tms->Insert(group)) co_return;
        AddTmsToChar(ctx, char_id, tms_id);
    }

    for (auto m : members)
    {
        { std::lock_guard g(group->lock); group->AddMember(m); }
        AddTmsToChar(ctx, m, tms_id);
    }

    // Announce the full roster to every member (inviter = the
    // requester; recipient header = each member).
    auto member_ids = MemberIds(group);
    auto roster = Roster(ctx, member_ids);
    for (auto id : member_ids)
    {
        auto rt = Snap(ctx, id);
        if (!rt.found) continue;
        if (auto p = FindMapPeer(ctx, rt.msi))
            co_await senders::SendMwTmsInviteReq(
                p, rt.id, rt.key, char_id, tms_id, roster);
    }
}

// --- OUT -----------------------------------------------------------
boost::asio::awaitable<void>
OnTmsOutAck(std::shared_ptr<PeerSession> peer,
            std::vector<std::byte>       body,
            const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers || !ctx.tms)
    {
        spdlog::warn("OnTmsOutAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, tms_id = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(tms_id))
    {
        spdlog::warn("OnTmsOutAck[{}]: short body ({} bytes)", ip,
                     body.size());
        co_return;
    }

    auto sender = Snap(ctx, char_id);
    if (!sender.found || sender.key != key) co_return;
    if (!HasTms(sender, tms_id)) co_return;
    auto group = ctx.tms->Find(tms_id);
    if (!group) co_return;

    // Tell the whole roster (the leaver included) before mutating.
    auto member_ids = MemberIds(group);
    for (auto id : member_ids)
    {
        auto rt = Snap(ctx, id);
        if (!rt.found) continue;
        if (auto p = FindMapPeer(ctx, rt.msi))
            co_await senders::SendMwTmsOutReq(p, rt.id, rt.key, tms_id,
                                              sender.name);
    }

    bool empty = false;
    {
        std::lock_guard g(group->lock);
        group->RemoveMember(char_id);
        group->last_member = sender.name;
        empty = group->members.empty();
    }
    RemoveTmsFromChar(ctx, char_id, tms_id);
    if (empty) ctx.tms->Remove(tms_id);
}

// --- logout cleanup (legacy LeaveTMS) ------------------------------
boost::asio::awaitable<void>
NotifyTmsOnLogout(const HandlerContext& ctx, std::shared_ptr<TChar> who)
{
    if (!ctx.chars || !ctx.peers || !ctx.tms || !who) co_return;

    std::uint32_t cid = 0;
    std::string   name;
    std::vector<std::uint32_t> ids;
    {
        std::lock_guard g(who->lock);
        cid  = who->char_id;
        name = who->name;
        ids  = who->tms;
    }

    // Drop the leaver from every conference it was in. For each, the
    // remaining members are told via TMSOUT_REQ and an emptied
    // conference is destroyed (legacy LeaveTMS, TWorldSvr.cpp:2845).
    for (auto tid : ids)
    {
        auto group = ctx.tms->Find(tid);
        if (!group) continue;

        std::vector<std::uint32_t> remaining;
        bool empty = false;
        {
            std::lock_guard g(group->lock);
            group->last_member = name;
            group->RemoveMember(cid);
            remaining = group->members;
            empty = remaining.empty();
        }
        for (auto mid : remaining)
        {
            auto rt = Snap(ctx, mid);
            if (!rt.found) continue;
            if (auto p = FindMapPeer(ctx, rt.msi))
                co_await senders::SendMwTmsOutReq(p, rt.id, rt.key, tid, name);
        }
        if (empty) ctx.tms->Remove(tid);
    }
}

} // namespace tworldsvr::handlers
