#include "handlers.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <mutex>

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

} // namespace tworldsvr::handlers
