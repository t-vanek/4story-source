#include "handlers.h"
#include "../wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <mutex>

namespace tworldsvr::handlers {

boost::asio::awaitable<void>
OnAddCharAck(std::shared_ptr<WorldSession> sess,
             std::vector<std::byte>        body,
             const HandlerContext&         ctx)
{
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
            sess->RemoteIPv4(), body.size());
        co_return;
    }

    if (!ctx.chars)
    {
        spdlog::warn("OnAddCharAck[{}]: char registry not wired — dropped",
            sess->RemoteIPv4());
        co_return;
    }

    // Existing char in the registry? Two legacy paths:
    //
    //   (a) Same char already known — the map server is reporting
    //       an additional connection (e.g. cross-zone teleport).
    //       The legacy module validates IP/port/key against the
    //       existing TCharCon entry and either accepts the new
    //       link or fires MW_INVALIDCHAR_REQ.
    //   (b) Unknown char — first connection; build a fresh TChar
    //       and a single TCharCon for this map server.
    //
    // W2 implements path (b) end-to-end. Path (a) is logged with a
    // TODO marker until W3 lands the peer registry needed for
    // SendMW_INVALIDCHAR_REQ + cross-map deduplication. Bug bar:
    // until path (a) is wired, a stale TChar with a mismatched key
    // is treated as "ignore the new connect, keep the old entry"
    // — the same outcome the legacy module produces under an
    // unreachable-main-server condition (SSHandler.cpp:809–826),
    // so no client-visible regression vs. that edge case.
    if (auto existing = ctx.chars->Find(char_id))
    {
        std::lock_guard g(existing->lock);
        const bool any_match = std::any_of(
            existing->cons.begin(), existing->cons.end(),
            [&](const TCharCon& c) {
                return c.server_id == 0  // server_id wired in W3
                    && c.ip_addr  == ip_addr
                    && c.port     == port;
            });
        if (existing->key == key && !any_match)
        {
            existing->cons.push_back(TCharCon{
                /*server_id*/ 0,   // TODO W3: pSERVER->m_wID from peer registry
                /*ip_addr*/   ip_addr,
                /*port*/      port,
                /*ready*/     false,
                /*valid*/     true,
            });
            spdlog::info("OnAddCharAck[{}]: additional connect char_id={} "
                         "user_id={} (W2 partial — peer-svr id not wired)",
                sess->RemoteIPv4(), char_id, user_id);
            co_return;
        }
        spdlog::warn("OnAddCharAck[{}]: char_id={} already registered with "
                     "key=0x{:08X} (incoming key=0x{:08X}) — dropping (W3 "
                     "will fire MW_INVALIDCHAR_REQ)",
            sess->RemoteIPv4(), char_id, existing->key, key);
        co_return;
    }

    auto ch = std::make_shared<TChar>();
    ch->char_id        = char_id;
    ch->key            = key;
    ch->user_id        = user_id;
    ch->main_server_id = 0;          // TODO W3: pSERVER->m_wID
    ch->logout         = false;
    ch->saving         = false;
    ch->db_loading     = false;
    ch->main_id_changing = false;
    ch->cons.push_back(TCharCon{
        /*server_id*/ 0,             // TODO W3: pSERVER->m_wID
        /*ip_addr*/   ip_addr,
        /*port*/      port,
        /*ready*/     false,
        /*valid*/     true,
    });

    if (!ctx.chars->Insert(std::move(ch)))
    {
        // Race: a concurrent OnAddCharAck for the same char won
        // the insert. The other handler will own this character;
        // we just drop. Legacy is single-threaded here, so this
        // path is new — and benign.
        spdlog::info("OnAddCharAck[{}]: lost insert race for char_id={} "
                     "— concurrent handler owns it", sess->RemoteIPv4(),
            char_id);
        co_return;
    }
    ctx.chars->MarkUserActive(user_id);

    spdlog::info("OnAddCharAck[{}]: char_id={} key=0x{:08X} user_id={} "
                 "ip={}.{}.{}.{}:{} — registered (total={}, active_users={})",
        sess->RemoteIPv4(), char_id, key, user_id,
        (ip_addr >>  0) & 0xFF, (ip_addr >>  8) & 0xFF,
        (ip_addr >> 16) & 0xFF, (ip_addr >> 24) & 0xFF, port,
        ctx.chars->Size(), ctx.chars->ActiveUserCount());

    // TODO W3: SendMW_ENTERSVR_REQ(true, char_id, key) back to the
    // map server. Requires the peer-session wrapper that knows
    // which map server this WorldSession represents.
    co_return;
}

boost::asio::awaitable<void>
OnCloseCharAck(std::shared_ptr<WorldSession> sess,
               std::vector<std::byte>        body,
               const HandlerContext&         ctx)
{
    wire::Reader r(body.data(), body.size());
    std::uint32_t char_id = 0;
    std::uint32_t key     = 0;
    if (!r.Read(char_id) || !r.Read(key))
    {
        spdlog::warn("OnCloseCharAck[{}]: short body ({} bytes) — dropped",
            sess->RemoteIPv4(), body.size());
        co_return;
    }

    if (!ctx.chars)
    {
        spdlog::warn("OnCloseCharAck[{}]: char registry not wired — dropped",
            sess->RemoteIPv4());
        co_return;
    }

    auto removed = ctx.chars->Remove(char_id);
    if (!removed)
    {
        // Legacy: SendMW_DELCHAR_REQ back with bForceDelete=true so
        // the map server cleans up its half. The sender lands in W3
        // — for W2 we log and skip; the map server will retry the
        // close and converge eventually.
        spdlog::info("OnCloseCharAck[{}]: char_id={} not in registry "
                     "(stale close) — TODO W3: SendMW_DELCHAR_REQ",
            sess->RemoteIPv4(), char_id);
        co_return;
    }

    std::uint32_t removed_user_id = 0;
    {
        std::lock_guard g(removed->lock);
        // Key mismatch is a stale-close race; legacy still proceeds
        // with CloseChar (SSHandler.cpp:2014). We mirror that — the
        // registry entry is already out by this point.
        if (removed->key != key)
            spdlog::warn("OnCloseCharAck[{}]: key mismatch for char_id={} "
                         "(registry=0x{:08X} incoming=0x{:08X}) — removed anyway",
                sess->RemoteIPv4(), char_id, removed->key, key);
        removed_user_id = removed->user_id;
    }

    // The user becomes inactive only if no other char of theirs
    // is still online. W2 lacks a user→char[] index, so the cheap
    // check is "scan a shard for any char with this user_id". That
    // scan is bounded by 1/16 of the active population; for the
    // legacy worst-case of ~5k concurrent chars that's ~300 lookups.
    // W3 will introduce a proper secondary index when guild lookups
    // demand it.
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
        sess->RemoteIPv4(), char_id, removed_user_id, any_other,
        ctx.chars->Size());
    co_return;
}

} // namespace tworldsvr::handlers
