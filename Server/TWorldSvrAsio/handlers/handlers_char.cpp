#include "handlers.h"
#include "../senders/senders.h"
#include "../services/friend_constants.h"
#include "../services/soulmate_constants.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <vector>

namespace tworldsvr::handlers {

boost::asio::awaitable<void>
OnAddCharAck(std::shared_ptr<PeerSession>  peer,
             std::vector<std::byte>        body,
             const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0;
    std::uint32_t key     = 0;
    std::uint32_t ip_addr = 0;
    std::uint16_t port    = 0;
    std::uint32_t user_id = 0;

    if (!r.Read(char_id) || !r.Read(key) || !r.Read(ip_addr) ||
        !r.Read(port)    || !r.Read(user_id))
    {
        spdlog::warn("OnAddCharAck[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }

    if (!ctx.chars)
    {
        spdlog::warn("OnAddCharAck[{}]: char registry not wired — dropped",
            ip);
        co_return;
    }

    // The server byte that owns this char's main session — the
    // legacy LOBYTE(pSERVER->m_wID). Now sourced from the peer
    // wrapper (set by OnRelaysvrReq).
    const std::uint8_t server_id =
        static_cast<std::uint8_t>(peer->Wid() & 0xFF);

    if (auto existing = ctx.chars->Find(char_id))
    {
        std::lock_guard g(existing->lock);
        const bool any_match = std::any_of(
            existing->cons.begin(), existing->cons.end(),
            [&](const TCharCon& c) {
                return c.server_id == server_id
                    && c.ip_addr   == ip_addr
                    && c.port      == port;
            });
        if (existing->key == key && !any_match)
        {
            existing->cons.push_back(TCharCon{
                server_id, ip_addr, port, /*ready*/ false, /*valid*/ true,
            });
            spdlog::info("OnAddCharAck[{}]: additional connect char_id={} "
                         "user_id={} server_id={}", ip, char_id, user_id,
                server_id);
            co_return;
        }
        spdlog::warn("OnAddCharAck[{}]: char_id={} already registered with "
                     "key=0x{:08X} (incoming key=0x{:08X}) — dropping (W3 "
                     "will fire MW_INVALIDCHAR_REQ)",
            ip, char_id, existing->key, key);
        co_return;
    }

    auto ch = std::make_shared<TChar>();
    ch->char_id          = char_id;
    ch->key              = key;
    ch->user_id          = user_id;
    ch->main_server_id   = server_id;
    ch->logout           = false;
    ch->saving           = false;
    ch->db_loading       = false;
    ch->main_id_changing = false;
    ch->cons.push_back(TCharCon{
        server_id, ip_addr, port, /*ready*/ false, /*valid*/ true,
    });

    if (!ctx.chars->Insert(std::move(ch)))
    {
        spdlog::info("OnAddCharAck[{}]: lost insert race for char_id={}",
            ip, char_id);
        co_return;
    }
    ctx.chars->MarkUserActive(user_id);

    spdlog::info("OnAddCharAck[{}]: char_id={} key=0x{:08X} user_id={} "
                 "server_id={} ip={}.{}.{}.{}:{} — registered (total={}, "
                 "active_users={})",
        ip, char_id, key, user_id, server_id,
        (ip_addr >>  0) & 0xFF, (ip_addr >>  8) & 0xFF,
        (ip_addr >> 16) & 0xFF, (ip_addr >> 24) & 0xFF, port,
        ctx.chars->Size(), ctx.chars->ActiveUserCount());

    // TODO W3a-3: SendMW_ENTERSVR_REQ(peer, true, char_id, key).
    co_return;
}

boost::asio::awaitable<void>
OnCloseCharAck(std::shared_ptr<PeerSession>  peer,
               std::vector<std::byte>        body,
               const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0;
    std::uint32_t key     = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnCloseCharAck[{}]: short body ({} bytes) — dropped",
            ip, body.size());
        co_return;
    }

    if (!ctx.chars)
    {
        spdlog::warn("OnCloseCharAck[{}]: char registry not wired — dropped",
            ip);
        co_return;
    }

    auto removed = ctx.chars->Remove(char_id);
    if (!removed)
    {
        spdlog::info("OnCloseCharAck[{}]: char_id={} not in registry "
                     "(stale close) — TODO W3a-3: SendMW_DELCHAR_REQ",
            ip, char_id);
        co_return;
    }

    std::uint32_t removed_user_id = 0;
    {
        std::lock_guard g(removed->lock);
        if (removed->key != key)
            spdlog::warn("OnCloseCharAck[{}]: key mismatch for char_id={} "
                         "(registry=0x{:08X} incoming=0x{:08X}) — removed anyway",
                ip, char_id, removed->key, key);
        removed_user_id = removed->user_id;
    }

    // W4-7: fan out the offline presence to this char's friends +
    // soulmate (legacy LeaveFriend / LeaveSoulmate). `removed` is
    // out of the registry but kept alive by the shared_ptr.
    co_await NotifyFriendsOnLogout(ctx, removed);
    co_await NotifySoulmateOnLogout(ctx, removed);
    // W4-12: drop the char from any open TMS conferences (LeaveTMS).
    co_await NotifyTmsOnLogout(ctx, removed);

    bool any_other = false;
    for (auto other_id : ctx.chars->SnapshotIds())
    {
        if (auto other = ctx.chars->Find(other_id))
        {
            std::lock_guard g(other->lock);
            if (other->user_id == removed_user_id)
            {
                any_other = true;
                break;
            }
        }
    }
    if (!any_other)
        ctx.chars->MarkUserInactive(removed_user_id);

    spdlog::info("OnCloseCharAck[{}]: char_id={} removed user_id={} "
                 "user_still_active={} (total={})",
        ip, char_id, removed_user_id, any_other, ctx.chars->Size());
    co_return;
}

boost::asio::awaitable<void>
OnRegionAck(std::shared_ptr<PeerSession>  peer,
            std::vector<std::byte>        body,
            const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars)
    {
        spdlog::warn("OnRegionAck[{}]: char registry not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, region = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(region))
    {
        spdlog::warn("OnRegionAck[{}]: short body ({} bytes)", ip, body.size());
        co_return;
    }

    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;
    std::uint32_t soulmate_target = 0;
    struct FE { std::uint32_t id; std::uint8_t type; bool connected; };
    std::vector<FE> friends;
    bool ok = false;
    {
        std::lock_guard g(c->lock);
        if (c->key == key)
        {
            c->region = region;
            soulmate_target = c->soulmate.target;
            for (const auto& f : c->friends)
                friends.push_back({f.id, f.type, f.connected});
            ok = true;
        }
    }
    if (!ok) co_return;

    // Mirror the new region into the soulmate partner's view (and
    // mark it connected — a region update implies we're online).
    if (soulmate_target != 0)
        if (auto p = ctx.chars->Find(soulmate_target))
        {
            std::lock_guard g(p->lock);
            if (p->soulmate.target == char_id)
            { p->soulmate.connected = true; p->soulmate.region = region; }
        }

    // Mirror it into each connected real-friend's reverse entry.
    for (const auto& fe : friends)
    {
        if (fe.type == frnd::kTypeFriend || !fe.connected) continue;
        if (auto p = ctx.chars->Find(fe.id))
        {
            std::lock_guard g(p->lock);
            for (auto& pf : p->friends)
                if (pf.id == char_id) pf.region = region;
        }
    }
    co_return;
}

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
OnLevelUpAck(std::shared_ptr<PeerSession>  peer,
             std::vector<std::byte>        body,
             const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars)
    {
        spdlog::warn("OnLevelUpAck[{}]: char registry not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  level = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(level))
    {
        spdlog::warn("OnLevelUpAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;
    std::uint8_t  main_msi = 0;
    std::vector<std::uint8_t> other_msis;
    std::uint32_t sm_target = 0;
    std::uint8_t  sm_known_level = 0;
    bool ok = false;
    {
        std::lock_guard g(c->lock);
        if (c->key == key)
        {
            c->level = level;
            main_msi = c->main_server_id;
            for (const auto& con : c->cons)
                if (con.valid && con.server_id != main_msi)
                    other_msis.push_back(con.server_id);
            sm_target = c->soulmate.target;
            sm_known_level = c->soulmate.level;
            ok = true;
        }
    }
    if (!ok) co_return;

    // Fan the new level to the char's other map connections.
    for (auto msi : other_msis)
        if (auto p = FindMapPeer(ctx, msi))
            co_await senders::SendMwLevelUpReq(p, char_id, key, level);

    if (sm_target != 0)
    {
        // Keep the partner's view of our level current.
        if (auto p = ctx.chars->Find(sm_target))
        {
            std::lock_guard g(p->lock);
            if (p->soulmate.target == char_id) p->soulmate.level = level;
        }
        // Auto-dissolve if the level gap now exceeds the window
        // (legacy CheckSoulmateEnd).
        if (std::abs(int(level) - int(sm_known_level)) >
            soulmate::kLevelWindow)
        {
            { std::lock_guard g(c->lock);
              if (c->soulmate.target == sm_target) c->soulmate = TSoulmate{}; }
            if (auto p = ctx.chars->Find(sm_target))
            { std::lock_guard g(p->lock);
              if (p->soulmate.target == char_id) p->soulmate = TSoulmate{}; }
            spdlog::info("OnLevelUpAck[{}]: char_id={} level {} dissolved "
                         "soulmate {} (gap > {})", ip, char_id, level,
                sm_target, soulmate::kLevelWindow);
        }
    }
    co_return;
}

boost::asio::awaitable<void>
OnCharStatInfoAck(std::shared_ptr<PeerSession>  peer,
                  std::vector<std::byte>        body,
                  const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers) co_return;

    wire::Reader r(body.data(), body.size());
    std::uint32_t req_char_id = 0, char_id = 0;
    if (!r.Read(req_char_id) || !r.Read(char_id))
    {
        spdlog::warn("OnCharStatInfoAck[{}]: short body", ip);
        co_return;
    }

    auto target = ctx.chars->Find(char_id);
    if (!target) co_return;
    std::uint8_t msi = 0;
    { std::lock_guard g(target->lock); msi = target->main_server_id; }
    if (auto p = FindMapPeer(ctx, msi))
        co_await senders::SendMwCharStatInfoAnsReq(p, req_char_id, char_id);
    co_return;
}

boost::asio::awaitable<void>
OnCharStatInfoAnsAck(std::shared_ptr<PeerSession>  peer,
                     std::vector<std::byte>        body,
                     const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers) co_return;

    wire::Reader r(body.data(), body.size());
    std::uint32_t req_char_id = 0;
    if (!r.Read(req_char_id))
    {
        spdlog::warn("OnCharStatInfoAnsAck[{}]: short body", ip);
        co_return;
    }

    auto requester = ctx.chars->Find(req_char_id);
    if (!requester) co_return;
    std::uint8_t msi = 0;
    { std::lock_guard g(requester->lock); msi = requester->main_server_id; }
    // Relay the gathered stat block back verbatim.
    if (auto p = FindMapPeer(ctx, msi))
        co_await senders::SendMwCharStatInfoReq(p, body);
    co_return;
}

boost::asio::awaitable<void>
OnPetRidingAck(std::shared_ptr<PeerSession>  peer,
               std::vector<std::byte>        body,
               const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars || !ctx.peers)
    {
        spdlog::warn("OnPetRidingAck[{}]: registries not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0, riding = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(riding))
    {
        spdlog::warn("OnPetRidingAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;
    const std::uint8_t origin_msi =
        static_cast<std::uint8_t>(peer->Wid() & 0xFF);
    std::vector<std::uint8_t> other_msis;
    bool ok = false;
    {
        std::lock_guard g(c->lock);
        if (c->key == key)
        {
            c->riding = riding;
            // Fan to every other connection (legacy excludes the
            // originating server, which already applied it locally).
            for (const auto& con : c->cons)
                if (con.valid && con.server_id != origin_msi)
                    other_msis.push_back(con.server_id);
            ok = true;
        }
    }
    if (!ok) co_return;

    for (auto msi : other_msis)
        if (auto p = FindMapPeer(ctx, msi))
            co_await senders::SendMwPetRidingReq(p, char_id, key, riding);
    co_return;
}

boost::asio::awaitable<void>
OnHelmetHideAck(std::shared_ptr<PeerSession>  peer,
                std::vector<std::byte>        body,
                const HandlerContext&         ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars)
    {
        spdlog::warn("OnHelmetHideAck[{}]: char registry not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0, key = 0;
    std::uint8_t  hide = 0;
    if (!r.Read(char_id) || !r.Read(key) || !r.Read(hide))
    {
        spdlog::warn("OnHelmetHideAck[{}]: short body ({} bytes)", ip,
            body.size());
        co_return;
    }

    auto c = ctx.chars->Find(char_id);
    if (!c) co_return;
    bool ok = false;
    {
        std::lock_guard g(c->lock);
        if (c->key == key) { c->helmet_hide = hide; ok = true; }
    }
    if (!ok) co_return;

    // Confirm back to the originating map (legacy echoes to pSERVER).
    co_await senders::SendMwHelmetHideReq(peer, char_id, key, hide);
    co_return;
}

} // namespace tworldsvr::handlers
