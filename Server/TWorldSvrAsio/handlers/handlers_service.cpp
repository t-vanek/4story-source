// W6-38 — service / control-plane handlers: the operator monitoring
// echo, the CCU-counter resync, the help-message broadcast + persist,
// and the map-initiated session teardown. See handlers.h for the
// per-handler legacy references.

#include "handlers.h"
#include "../senders/senders.h"
#include "../wire_codec.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace tworldsvr::handlers {

namespace {

// HIBYTE(wID) identifies the server group (CTProtocol.h:30).
constexpr std::uint8_t kSvrGrpMapSvr = 4;

} // namespace

boost::asio::awaitable<void>
OnCtServiceMonitorAck(std::shared_ptr<PeerSession> peer,
                      std::vector<std::byte>       body,
                      const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();

    wire::Reader r(body.data(), body.size());
    std::uint32_t tick = 0;
    if (!r.Read(tick))
    {
        spdlog::warn("OnCtServiceMonitorAck[{}]: short body ({} bytes)",
            ip, body.size());
        co_return;
    }

    // Legacy replies (tick, m_mapSESSION.size(), m_mapTCHAR.size(),
    // m_mapACTIVEUSER.size()). Our session universe differs slightly:
    // registered map peers + the ctrl-svr slot stand in for the raw
    // socket map (un-handshaked sockets aren't counted — divergence
    // noted in the README).
    std::uint32_t sessions = 0;
    if (ctx.peers)
        sessions = static_cast<std::uint32_t>(ctx.peers->Size());
    if (ctx.ctrl_svr && ctx.ctrl_svr->Get())
        ++sessions;
    const std::uint32_t chars =
        ctx.chars ? static_cast<std::uint32_t>(ctx.chars->Size()) : 0;
    const std::uint32_t users =
        ctx.chars ? static_cast<std::uint32_t>(ctx.chars->ActiveUserCount())
                  : 0;

    co_await senders::SendCtServiceMonitorReq(peer, tick, sessions,
        chars, users);
    co_return;
}

boost::asio::awaitable<void>
OnCtServiceDataClearAck(std::shared_ptr<PeerSession> peer,
                        std::vector<std::byte>       body,
                        const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.chars)
    {
        spdlog::warn("OnCtServiceDataClearAck[{}]: chars not wired", ip);
        co_return;
    }
    if (!body.empty())
        spdlog::debug("OnCtServiceDataClearAck[{}]: ignored unexpected "
                      "body ({} bytes)", ip, body.size());

    ctx.chars->RebuildActiveUsers();
    spdlog::info("OnCtServiceDataClearAck[{}]: active-user set rebuilt "
                 "from {} char(s) → {} user(s)",
        ip, ctx.chars->Size(), ctx.chars->ActiveUserCount());
    co_return;
}

boost::asio::awaitable<void>
OnCtHelpMessageReq(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       body,
                   const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!ctx.peers)
    {
        spdlog::warn("OnCtHelpMessageReq[{}]: peers not wired", ip);
        co_return;
    }

    wire::Reader r(body.data(), body.size());
    std::uint8_t id    = 0;
    std::int64_t start = 0;
    std::int64_t end   = 0;
    std::string  message;
    if (!r.Read(id) || !r.Read(start) || !r.Read(end) ||
        !r.ReadString(message))
    {
        spdlog::warn("OnCtHelpMessageReq[{}]: malformed body ({} bytes)",
            ip, body.size());
        co_return;
    }

    for (auto& p : ctx.peers->Snapshot())
        co_await senders::SendMwHelpMessageReq(p, id, start, end,
            message);

    // Legacy fires DM_HELPMESSAGE_REQ → THelpMessage SP
    // (SSHandler.cpp:13149) — repository-collapsed.
    if (ctx.service_ops)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [repo = ctx.service_ops, id, start, end, message]
            { repo->SaveHelpMessage(id, start, end, message); });
    }
    else
    {
        spdlog::info("OnCtHelpMessageReq[{}]: service_ops not wired — "
                     "broadcast only, persist skipped", ip);
    }
    co_return;
}

boost::asio::awaitable<void>
OnSmQuitServiceReq(std::shared_ptr<PeerSession> peer,
                   std::vector<std::byte>       /*body*/,
                   const HandlerContext&        /*ctx*/)
{
    // Legacy body is commented out (SSHandler.cpp:502-510) — the
    // SCM-stop / WM_QUIT path was disabled in the shipped build. We
    // keep the same "log and ignore" behavior; a cluster-wide stop
    // belongs to the control-server admin path.
    spdlog::warn("OnSmQuitServiceReq[{}]: SM_QUITSERVICE_REQ detected — "
                 "ignored (disabled in the legacy build too)",
        peer->Wire()->RemoteIPv4());
    co_return;
}

boost::asio::awaitable<void>
OnSmDelSessionReq(std::shared_ptr<PeerSession> peer,
                  std::vector<std::byte>       body,
                  const HandlerContext&        ctx)
{
    const std::string& ip = peer->Wire()->RemoteIPv4();
    if (!body.empty())
        spdlog::debug("OnSmDelSessionReq[{}]: ignored unexpected body "
                      "({} bytes)", ip, body.size());

    // Legacy gates the char sweep + DB clear on the sender being a
    // map server (HIBYTE(wID) == SVRGRP_MAPSVR) but closes the
    // session unconditionally (SSHandler.cpp:512-545).
    const std::uint16_t wid       = peer->Wid();
    const std::uint8_t  group     = static_cast<std::uint8_t>(wid >> 8);
    const std::uint8_t  server_id = static_cast<std::uint8_t>(wid & 0xFF);

    if (group == kSvrGrpMapSvr && ctx.chars)
    {
        std::size_t closed = 0;
        for (std::uint32_t id : ctx.chars->SnapshotIds())
        {
            auto c = ctx.chars->Find(id);
            if (!c) continue;
            bool has_con = false;
            {
                std::lock_guard g(c->lock);
                for (const auto& con : c->cons)
                    if (con.server_id == server_id) { has_con = true; break; }
            }
            if (!has_con) continue;
            co_await CloseChar(c, ctx);
            ++closed;
        }

        // Legacy DM_CLEARMAPCURRENTUSER_REQ(m_bGroupID, LOBYTE(wID),
        // SVRGRP_MAPSVR) → TClearMapCurrentUser (SSHandler.cpp:9956)
        // — repository-collapsed.
        if (ctx.service_ops)
        {
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
                [repo = ctx.service_ops, gid = ctx.group_id, server_id]
                { repo->ClearMapCurrentUser(gid, server_id,
                                            kSvrGrpMapSvr); });
        }

        spdlog::info("OnSmDelSessionReq[{}]: map wID=0x{:04X} teardown — "
                     "{} char(s) closed, current-user clear {}",
            ip, wid, closed,
            ctx.service_ops ? "queued" : "skipped (no DB)");
    }
    else
    {
        spdlog::info("OnSmDelSessionReq[{}]: non-map sender wID=0x{:04X} "
                     "— closing session only (legacy parity)", ip, wid);
    }

    // Legacy PostQueuedCompletionStatus(COMP_CLOSE) — force-close the
    // sender's socket. The read loop unwinds and unregisters the peer.
    peer->Wire()->Close();
    co_return;
}

} // namespace tworldsvr::handlers
