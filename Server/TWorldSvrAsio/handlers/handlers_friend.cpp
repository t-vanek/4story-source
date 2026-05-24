#include "handlers.h"
#include "../senders/senders.h"
#include "../services/friend_constants.h"
#include "../wire_codec.h"

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

} // namespace

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
        // (Persistence — legacy DM_FRIENDINSERT_REQ — deferred.)
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

} // namespace tworldsvr::handlers
