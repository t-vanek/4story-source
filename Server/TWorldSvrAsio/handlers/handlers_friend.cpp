#include "handlers.h"
#include "../senders/senders.h"
#include "../services/friend_constants.h"
#include "../wire_codec.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>
#include <string>

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

// Upsert a char's friend entry to a connected mutual friend
// (FT_FRIENDFRIEND). Caller holds the char's lock.
void UpsertMutual(TChar& c, std::uint32_t id, const std::string& name,
                  std::uint32_t region)
{
    for (auto& f : c.friends)
        if (f.id == id)
        {
            f.type = frnd::kTypeFriendFriend;
            f.connected = true;
            f.region = region;
            return;
        }
    c.friends.push_back({id, name, frnd::kTypeFriendFriend, true, region, 0});
}

} // namespace

boost::asio::awaitable<void>
OnFriendProtectedAskAck(std::shared_ptr<PeerSession> peer,
                        std::vector<std::byte>       body,
                        const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnFriendProtectedAskAck[{}]: registries not wired", ip);
        co_return;
    }

    // A char tried to friend a protection-enabled target (the map
    // gates that); world relays the auto-refuse to the target's map,
    // naming the requester (legacy OnMW_FRIENDPROTECTEDASK_ACK).
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   target_name;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(target_name))
    {
        spdlog::warn("OnFriendProtectedAskAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto requester = ctx.chars->Find(char_id);
    if (!requester) co_return;
    std::string req_name;
    { std::lock_guard g(requester->lock);
      if (requester->key != key) co_return;
      req_name = requester->name; }

    auto target = ctx.chars->FindByName(target_name);
    if (!target) co_return;
    std::uint32_t t_id = 0, t_key = 0; std::uint8_t t_msi = 0;
    {
        std::lock_guard g(target->lock);
        t_id = target->char_id; t_key = target->key;
        t_msi = target->main_server_id;
    }
    if (auto p = FindMapPeer(ctx, t_msi))
        co_await senders::SendMwFriendAddReq(p, t_id, t_key, frnd::kRefuse,
            /*friend_id=*/0, req_name, 0, 0, 0, 0);
    co_return;
}

boost::asio::awaitable<void>
OnFriendAskAck(std::shared_ptr<PeerSession> peer,
               std::vector<std::byte>       body,
               const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnFriendAskAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   target_name;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(target_name))
    {
        spdlog::warn("OnFriendAskAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto requester = ctx.chars->Find(char_id);
    if (!requester) co_return;
    std::uint32_t actual_key = 0, req_region = 0;
    std::uint8_t  req_country = 0;
    std::string   req_name;
    {
        std::lock_guard g(requester->lock);
        actual_key  = requester->key;
        req_country = requester->country;
        req_region  = requester->region;
        req_name    = requester->name;
    }
    if (actual_key != key) co_return;

    auto reply = [&](std::uint8_t result, std::uint32_t friend_id,
                     const std::string& name, std::uint8_t level,
                     std::uint8_t klass, std::uint32_t region)
        -> boost::asio::awaitable<void> {
        co_await senders::SendMwFriendAddReq(peer, char_id, key, result,
            friend_id, name, level, /*group=*/0, klass, region);
    };

    auto target = ctx.chars->FindByName(target_name);
    if (!target)
    {
        co_await reply(frnd::kNotFound, 0, target_name, 0, 0, 0);
        co_return;
    }
    std::uint32_t t_id = 0, t_region = 0, t_key = 0;
    std::uint8_t  t_country = 0, t_level = 0, t_class = 0, t_msi = 0;
    std::string   t_name;
    {
        std::lock_guard g(target->lock);
        t_id      = target->char_id;
        t_country = target->country;
        t_level   = target->level;
        t_class   = target->klass;
        t_region  = target->region;
        t_key     = target->key;
        t_msi     = target->main_server_id;
        t_name    = target->name;
    }
    if (req_country != t_country)
    {
        co_await reply(frnd::kNotFound, 0, target_name, 0, 0, 0);
        co_return;
    }

    // Inspect the requester's list: does an entry for target exist
    // (and is it the pending FT_TARGET?), and how many non-target
    // friends are there?
    bool         req_has_entry = false;
    std::uint8_t req_entry_type = 0;
    std::uint8_t req_nontarget = 0;
    {
        std::lock_guard g(requester->lock);
        for (const auto& f : requester->friends)
        {
            if (f.type != frnd::kTypeTarget) ++req_nontarget;
            if (f.id == t_id) { req_has_entry = true; req_entry_type = f.type; }
        }
    }
    if (req_has_entry && req_entry_type != frnd::kTypeTarget)
    {
        co_await reply(frnd::kAlready, 0, target_name, 0, 0, 0);
        co_return;
    }
    if (req_nontarget == frnd::kMaxFriend)
    {
        co_await reply(frnd::kMax, 0, target_name, 0, 0, 0);
        co_return;
    }

    // Inspect the target's list: does it hold an entry for the
    // requester, and how many of its slots count toward its cap?
    bool         tgt_has_entry = false;
    std::uint8_t tgt_relevant = 0;
    {
        std::lock_guard g(target->lock);
        for (const auto& f : target->friends)
        {
            if (f.type != frnd::kTypeTarget && f.id != char_id) ++tgt_relevant;
            if (f.id == char_id) tgt_has_entry = true;
        }
    }

    // Both sides already hold a pending entry → complete the
    // friendship immediately (legacy pTF && pTT branch).
    if (req_has_entry && tgt_has_entry)
    {
        {
            std::lock_guard g(requester->lock);
            for (auto& f : requester->friends)
                if (f.id == t_id)
                {
                    f.type = frnd::kTypeFriendFriend;
                    f.connected = true;
                    f.region = t_region;
                    f.group = 0;
                }
        }
        {
            std::lock_guard g(target->lock);
            for (auto& f : target->friends)
                if (f.id == char_id)
                {
                    f.type = frnd::kTypeFriendFriend;
                    f.connected = true;
                    f.region = req_region;
                }
        }
        // Persist both directed edges (legacy DM_FRIENDINSERT_REQ ×2).
        if (ctx.friend_repo)
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.friend_repo, char_id, t_id]
                {
                    repo->InsertFriend(t_id, char_id);
                    repo->InsertFriend(char_id, t_id);
                });
        co_await reply(frnd::kSuccess, t_id, t_name, t_level, t_class,
            t_region);
        spdlog::info("OnFriendAskAck[{}]: char_id={} + '{}' now mutual "
                     "friends", ip, char_id, target_name);
        co_return;
    }

    if (tgt_relevant == frnd::kMaxFriend)
    {
        co_await reply(frnd::kRefuse, 0, target_name, 0, 0, 0);
        co_return;
    }

    // Forward the friend dialog to the target's map.
    if (auto tp = FindMapPeer(ctx, t_msi))
        co_await senders::SendMwFriendAskReq(tp, t_id, t_key, req_name,
            char_id);
    spdlog::info("OnFriendAskAck[{}]: char_id={} asked '{}' to befriend",
        ip, char_id, target_name);
    co_return;
}

boost::asio::awaitable<void>
OnFriendReplyAck(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnFriendReplyAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::string   inviter_name;
    std::uint8_t  reply = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.ReadString(inviter_name) ||
        !r.Read(reply))
    {
        spdlog::warn("OnFriendReplyAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto inviter = ctx.chars->FindByName(inviter_name);  // pPlayer
    auto answerer = ctx.chars->Find(char_id);            // pFriend

    bool friend_ok = false;
    std::uint32_t f_key = 0;
    if (answerer)
    { std::lock_guard g(answerer->lock); f_key = answerer->key;
      friend_ok = (f_key == key); }

    if (!inviter && !friend_ok) co_return;

    if (!inviter && friend_ok)
    {
        // Inviter logged off — tell the answerer.
        std::uint8_t msi = 0;
        { std::lock_guard g(answerer->lock); msi = answerer->main_server_id; }
        if (auto p = FindMapPeer(ctx, msi))
            co_await senders::SendMwFriendAddReq(p, char_id, key,
                frnd::kNotFound, 0, inviter_name, 0, 0, 0, 0);
        co_return;
    }
    if (inviter && !friend_ok)
    {
        std::uint32_t p_id = 0, p_key = 0;
        std::uint8_t  p_msi = 0;
        { std::lock_guard g(inviter->lock); p_id = inviter->char_id;
          p_key = inviter->key; p_msi = inviter->main_server_id; }
        if (auto p = FindMapPeer(ctx, p_msi))
            co_await senders::SendMwFriendAddReq(p, p_id, p_key,
                frnd::kNotFound, 0, std::string{}, 0, 0, 0, 0);
        co_return;
    }

    std::uint32_t p_id = 0, p_key = 0, p_region = 0;
    std::uint8_t  p_level = 0, p_class = 0, p_msi = 0;
    std::string   p_name;
    {
        std::lock_guard g(inviter->lock);
        p_id = inviter->char_id; p_key = inviter->key;
        p_region = inviter->region; p_level = inviter->level;
        p_class = inviter->klass; p_msi = inviter->main_server_id;
        p_name = inviter->name;
    }
    std::uint32_t f_region = 0;
    std::uint8_t  f_level = 0, f_class = 0, f_msi = 0;
    std::string   f_name;
    {
        std::lock_guard g(answerer->lock);
        f_region = answerer->region; f_level = answerer->level;
        f_class = answerer->klass; f_msi = answerer->main_server_id;
        f_name = answerer->name;
    }

    if (reply != 0 /* ASK_YES (NetCode.h:222) */)
    {
        // Decline — relay the answer code to the inviter.
        if (auto p = FindMapPeer(ctx, p_msi))
            co_await senders::SendMwFriendAddReq(p, p_id, p_key, reply, char_id,
                f_name, 0, 0, 0, 0);
        co_return;
    }

    // Accept — both sides become connected mutual friends.
    { std::lock_guard g(inviter->lock);
      UpsertMutual(*inviter, char_id, f_name, f_region); }
    { std::lock_guard g(answerer->lock);
      UpsertMutual(*answerer, p_id, p_name, p_region); }
    // Persist both directed edges (legacy DM_FRIENDINSERT_REQ ×2).
    if (ctx.friend_repo)
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.friend_repo, p_id, char_id]
            {
                repo->InsertFriend(p_id, char_id);
                repo->InsertFriend(char_id, p_id);
            });

    if (auto p = FindMapPeer(ctx, p_msi))
        co_await senders::SendMwFriendAddReq(p, p_id, p_key, frnd::kSuccess,
            char_id, f_name, f_level, 0, f_class, f_region);
    if (auto p = FindMapPeer(ctx, f_msi))
        co_await senders::SendMwFriendAddReq(p, char_id, key, frnd::kSuccess,
            p_id, p_name, p_level, 0, p_class, p_region);
    spdlog::info("OnFriendReplyAck[{}]: '{}' + char_id={} now friends",
        ip, inviter_name, char_id);
    co_return;
}

boost::asio::awaitable<void>
OnFriendEraseAck(std::shared_ptr<PeerSession> peer,
                 std::vector<std::byte>       body,
                 const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnFriendEraseAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, target_id = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(target_id))
    {
        spdlog::warn("OnFriendEraseAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto self = ctx.chars->Find(char_id);
    if (!self) co_return;
    std::uint32_t actual_key = 0;
    bool          found = false, connected = false;
    std::uint8_t  type = 0;
    {
        std::lock_guard g(self->lock);
        actual_key = self->key;
        for (const auto& f : self->friends)
            if (f.id == target_id)
            { found = true; type = f.type; connected = f.connected; }
    }
    if (actual_key != key) co_return;

    if (!found)
    {
        co_await senders::SendMwFriendEraseReq(peer, char_id, key,
            frnd::kNotFound, target_id);
        co_return;
    }

    if (type == frnd::kTypeFriendFriend)
    {
        // Demote: self keeps a pending stub, the online other side
        // drops to a one-way FT_FRIEND.
        if (connected)
            if (auto tgt = ctx.chars->Find(target_id))
            {
                std::lock_guard g(tgt->lock);
                for (auto& f : tgt->friends)
                    if (f.id == char_id) f.type = frnd::kTypeFriend;
            }
        std::lock_guard g(self->lock);
        for (auto& f : self->friends)
            if (f.id == target_id) f.type = frnd::kTypeTarget;
    }
    else if (type == frnd::kTypeFriend)
    {
        // One-way friend: fully remove from both lists.
        if (connected)
            if (auto tgt = ctx.chars->Find(target_id))
            {
                std::lock_guard g(tgt->lock);
                auto& v = tgt->friends;
                for (auto it = v.begin(); it != v.end(); ++it)
                    if (it->id == char_id) { v.erase(it); break; }
            }
        std::lock_guard g(self->lock);
        auto& v = self->friends;
        for (auto it = v.begin(); it != v.end(); ++it)
            if (it->id == target_id) { v.erase(it); break; }
    }
    // (FT_TARGET stays as-is — legacy replies SUCCESS without change.)

    co_await senders::SendMwFriendEraseReq(peer, char_id, key, frnd::kSuccess,
        target_id);
    // Both the demote and one-way branches drop the char's forward
    // edge; FT_TARGET leaves the DB untouched (legacy CSPFriendErase).
    if ((type == frnd::kTypeFriendFriend || type == frnd::kTypeFriend) &&
        ctx.friend_repo)
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.friend_repo, char_id, target_id]
            { repo->EraseFriend(char_id, target_id); });
    spdlog::info("OnFriendEraseAck[{}]: char_id={} erased friend {}",
        ip, char_id, target_id);
    co_return;
}

boost::asio::awaitable<void>
OnFriendGroupMakeAck(std::shared_ptr<PeerSession> peer,
                     std::vector<std::byte>       body,
                     const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars) { spdlog::warn("OnFriendGroupMakeAck[{}]: no chars", ip);
        co_return; }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  group = 0;
    std::string   name;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(group) ||
        !r.ReadString(name))
    {
        spdlog::warn("OnFriendGroupMakeAck[{}]: short body", ip);
        co_return;
    }
    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;

    std::uint8_t result = frnd::kSuccess;
    bool         key_ok = true;
    {
        std::lock_guard g(c->lock);
        if (c->key != key) { key_ok = false; }
        else if (group == 0 ||
                 c->friend_groups.size() >= frnd::kMaxFriendGroup)
            result = frnd::kMax;
        else if (name.size() > frnd::kMaxGroupName)
            key_ok = false;       // legacy: silent drop on over-long name
        else
        {
            for (const auto& gp : c->friend_groups)
                if (gp.first == group || gp.second == name)
                { result = frnd::kAlready; break; }
            if (result == frnd::kSuccess)
                c->friend_groups.emplace_back(group, name);
        }
    }
    if (!key_ok) co_return;
    co_await senders::SendMwFriendGroupMakeReq(peer, char_id, key, result,
        result == frnd::kSuccess ? group : 0, name);
    if (result == frnd::kSuccess && ctx.friend_repo)
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.friend_repo, char_id, group, name]
            { repo->MakeGroup(char_id, group, name); });
    co_return;
}

boost::asio::awaitable<void>
OnFriendGroupDeleteAck(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars) { spdlog::warn("OnFriendGroupDeleteAck[{}]: no chars", ip);
        co_return; }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  group = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(group))
    {
        spdlog::warn("OnFriendGroupDeleteAck[{}]: short body", ip);
        co_return;
    }
    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;

    bool key_ok = true, exists = false, occupied = false;
    {
        std::lock_guard g(c->lock);
        if (c->key != key) key_ok = false;
        else
        {
            for (const auto& gp : c->friend_groups)
                if (gp.first == group) exists = true;
            for (const auto& f : c->friends)
                if (f.type != frnd::kTypeTarget && f.group == group)
                    occupied = true;
            if (exists && !occupied)
            {
                auto& v = c->friend_groups;
                for (auto it = v.begin(); it != v.end(); ++it)
                    if (it->first == group) { v.erase(it); break; }
            }
        }
    }
    if (!key_ok || !exists) co_return;  // legacy: no reply when absent
    co_await senders::SendMwFriendGroupDeleteReq(peer, char_id, key,
        occupied ? frnd::kRefuse : frnd::kSuccess, group);
    if (!occupied && ctx.friend_repo)
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.friend_repo, char_id, group]
            { repo->DeleteGroup(char_id, group); });
    co_return;
}

boost::asio::awaitable<void>
OnFriendGroupChangeAck(std::shared_ptr<PeerSession> peer,
                       std::vector<std::byte>       body,
                       const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars) { spdlog::warn("OnFriendGroupChangeAck[{}]: no chars", ip);
        co_return; }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, friend_id = 0;
    std::uint8_t  group = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(friend_id) ||
        !r.Read(group))
    {
        spdlog::warn("OnFriendGroupChangeAck[{}]: short body", ip);
        co_return;
    }
    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;

    bool key_ok = true, ok = false;
    {
        std::lock_guard g(c->lock);
        if (c->key != key) { key_ok = false; }
        else
        {
            bool group_ok = (group == 0);
            if (!group_ok)
                for (const auto& gp : c->friend_groups)
                    if (gp.first == group) group_ok = true;
            if (group_ok)
                for (auto& f : c->friends)
                    if (f.id == friend_id) { f.group = group; ok = true; }
        }
    }
    if (!key_ok || !ok) co_return;  // legacy: silent drop on bad group/friend
    co_await senders::SendMwFriendGroupChangeReq(peer, char_id, key,
        frnd::kSuccess, group, friend_id);
    if (ctx.friend_repo)
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.friend_repo, char_id, friend_id, group]
            { repo->ChangeFriendGroup(char_id, friend_id, group); });
    co_return;
}

boost::asio::awaitable<void>
OnFriendGroupNameAck(std::shared_ptr<PeerSession> peer,
                     std::vector<std::byte>       body,
                     const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars) { spdlog::warn("OnFriendGroupNameAck[{}]: no chars", ip);
        co_return; }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  group = 0;
    std::string   name;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(group) ||
        !r.ReadString(name))
    {
        spdlog::warn("OnFriendGroupNameAck[{}]: short body", ip);
        co_return;
    }
    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;

    bool         key_ok = true;
    std::uint8_t result = frnd::kSuccess;
    {
        std::lock_guard g(c->lock);
        if (c->key != key) key_ok = false;
        else if (name.size() > frnd::kMaxGroupName) key_ok = false; // drop
        else
        {
            bool name_taken = false, found = false;
            for (const auto& gp : c->friend_groups)
                if (gp.second == name) name_taken = true;
            if (name_taken) result = frnd::kRefuse;
            else
            {
                for (auto& gp : c->friend_groups)
                    if (gp.first == group) { gp.second = name; found = true; }
                if (!found) result = frnd::kNotFound;
            }
        }
    }
    if (!key_ok) co_return;
    co_await senders::SendMwFriendGroupNameReq(peer, char_id, key, result,
        group, name);
    if (result == frnd::kSuccess && ctx.friend_repo)
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.friend_repo, char_id, group, name]
            { repo->RenameGroup(char_id, group, name); });
    co_return;
}

boost::asio::awaitable<void>
NotifyFriendsOnLogout(const HandlerContext& ctx, std::shared_ptr<TChar> who)
{
    if (!ctx.chars || !ctx.peers || !who) co_return;

    std::uint32_t who_id = 0;
    std::string   who_name;
    struct FE { std::uint32_t id; std::uint8_t type; bool connected; };
    std::vector<FE> entries;
    {
        std::lock_guard g(who->lock);
        who_id = who->char_id;
        who_name = who->name;
        for (const auto& f : who->friends)
            entries.push_back({f.id, f.type, f.connected});
    }

    for (const auto& e : entries)
    {
        if (!e.connected) continue;
        auto partner = ctx.chars->Find(e.id);
        if (!partner) continue;
        std::uint32_t pkey = 0;
        std::uint8_t  pmsi = 0;
        {
            std::lock_guard g(partner->lock);
            for (auto& pf : partner->friends)
                if (pf.id == who_id) pf.connected = false;
            pkey = partner->key;
            pmsi = partner->main_server_id;
        }
        // Only a real friend (mutual / pending) gets the toast;
        // a one-way FT_FRIEND stub does not (legacy gate).
        if (e.type != frnd::kTypeFriend)
            if (auto p = FindMapPeer(ctx, pmsi))
                co_await senders::SendMwFriendConnectionReq(p, e.id, pkey,
                    frnd::kDisconnection, who_name, 0);
    }
    co_return;
}

boost::asio::awaitable<void>
NotifyFriendsOnLogin(const HandlerContext& ctx, std::shared_ptr<TChar> who)
{
    if (!ctx.chars || !ctx.peers || !who) co_return;

    std::uint32_t who_id = 0, who_region = 0;
    std::string   who_name;
    struct FE { std::uint32_t id; std::uint8_t type; };
    std::vector<FE> entries;
    {
        std::lock_guard g(who->lock);
        who_id     = who->char_id;
        who_name   = who->name;
        who_region = who->region;
        for (const auto& f : who->friends)
            entries.push_back({f.id, f.type});
    }

    // Mirror of NotifyFriendsOnLogout, fired once the char's identity
    // is loaded (OnEnterSvrAck). Online status is resolved live here
    // (vs. the stored flag) so a friend who came online between this
    // char's ADDCHAR hydrate and its ENTERSVR still gets the toast.
    for (const auto& e : entries)
    {
        auto partner = ctx.chars->Find(e.id);
        if (!partner) continue;                 // offline now → skip
        std::uint32_t pkey = 0;
        std::uint8_t  pmsi = 0;
        {
            std::lock_guard g(partner->lock);
            for (auto& pf : partner->friends)
                if (pf.id == who_id)
                { pf.connected = true; pf.region = who_region; }
            pkey = partner->key;
            pmsi = partner->main_server_id;
        }
        // Reflect the now-online partner into our own entry.
        { std::lock_guard g(who->lock);
          for (auto& f : who->friends) if (f.id == e.id) f.connected = true; }
        // Only a real friend (mutual / pending) gets the toast.
        if (e.type != frnd::kTypeFriend)
            if (auto p = FindMapPeer(ctx, pmsi))
                co_await senders::SendMwFriendConnectionReq(p, e.id, pkey,
                    frnd::kConnection, who_name, who_region);
    }
    co_return;
}

boost::asio::awaitable<void>
OnFriendListAck(std::shared_ptr<PeerSession> peer,
                std::vector<std::byte>       body,
                const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars) { spdlog::warn("OnFriendListAck[{}]: no chars", ip);
        co_return; }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnFriendListAck[{}]: short body", ip);
        co_return;
    }
    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;

    // Snapshot the groups + non-pending friend entries.
    std::vector<std::pair<std::uint8_t, std::string>> groups;
    std::vector<senders::FriendListRow> rows;
    {
        std::lock_guard g(c->lock);
        if (c->key != key) co_return;
        groups = c->friend_groups;
        for (const auto& f : c->friends)
        {
            if (f.type == frnd::kTypeTarget) continue;
            senders::FriendListRow row;
            row.id = f.id; row.name = f.name; row.group = f.group;
            rows.push_back(std::move(row));
        }
    }

    // Resolve each friend's live level/class/region + online flag.
    for (auto& row : rows)
        if (auto fc = ctx.chars->Find(row.id))
        {
            std::lock_guard fg(fc->lock);
            row.connected = 1;
            row.level  = fc->level;
            row.klass  = fc->klass;
            row.region = fc->region;
        }

    co_await senders::SendMwFriendListReq(peer, char_id, key, groups, rows);
    co_return;
}

} // namespace tworldsvr::handlers
