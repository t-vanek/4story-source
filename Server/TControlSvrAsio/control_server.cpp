#include "control_server.h"

#include "senders.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

namespace tcontrolsvr {

ControlServer::ControlServer(boost::asio::io_context& io,
                             ControlServerConfig cfg)
    : m_io(io)
    , m_acceptor(io)
    , m_port(cfg.port)
    , m_cfg(std::move(cfg))
    , m_auto_start(m_cfg.auto_start)
{
    using boost::asio::ip::tcp;
    tcp::endpoint ep(tcp::v4(), m_port);
    m_acceptor.open(ep.protocol());
    m_acceptor.set_option(tcp::acceptor::reuse_address(true));
    m_acceptor.bind(ep);
    m_acceptor.listen();
    m_port = m_acceptor.local_endpoint().port();
}

boost::asio::awaitable<void>
ControlServer::Run()
{
    using boost::asio::ip::tcp;
    while (m_acceptor.is_open())
    {
        boost::system::error_code ec;
        tcp::socket sock = co_await m_acceptor.async_accept(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) break;
        auto sess = std::make_shared<ControlSession>(std::move(sock));
        spdlog::info("control_svr: accept from {}", sess->RemoteIPv4());
        boost::asio::co_spawn(m_io,
            HandleConnection(std::move(sess)),
            boost::asio::detached);
    }
}

boost::asio::awaitable<void>
ControlServer::Drive(std::shared_ptr<ControlSession> sess)
{
    co_await HandleConnection(std::move(sess));
}

boost::asio::awaitable<void>
ControlServer::HandleConnection(std::shared_ptr<ControlSession> sess)
{
    auto op = std::make_shared<OperatorSession>(sess);

    HandlerContext ctx{};
    ctx.auth       = m_cfg.auth;
    ctx.inventory  = m_cfg.inventory;
    ctx.operators  = &m_operators;
    ctx.peers      = m_cfg.peers;
    ctx.controller = m_cfg.controller;
    ctx.dialer     = m_cfg.dialer;
    ctx.audit      = m_cfg.audit;
    ctx.user_ban   = m_cfg.user_ban;
    ctx.chat_bans  = m_cfg.chat_bans;
    ctx.events     = m_cfg.events;
    ctx.event_repo = m_cfg.event_repo;
    ctx.patch_meta = m_cfg.patch_meta;
    ctx.alerter    = m_cfg.alerter;
    ctx.login_rate = m_cfg.login_rate;
    ctx.db_pool    = m_cfg.db_pool;
    ctx.io         = &m_io;
    ctx.auto_start = &m_auto_start;

    co_await sess->Run(
        [this, op, ctx](std::shared_ptr<ControlSession> /*s*/,
                        DecodedPacket pkt) -> boost::asio::awaitable<void>
        {
            co_await handlers::Dispatch(op, pkt.wId,
                                        std::move(pkt.body), ctx);
        });

    if (op->LoggedIn())
    {
        spdlog::info("control_svr: operator '{}' (seq={}) disconnected",
            op->UserId(), op->ManagerSeq());
    }
    m_operators.Unregister(op.get());
}

boost::asio::awaitable<void>
ControlServer::PeerKeepaliveLoop(std::chrono::milliseconds offline_after,
                                 std::chrono::milliseconds tick)
{
    if (!m_cfg.peers)
        co_return;
    boost::asio::steady_timer timer(m_io);
    while (m_acceptor.is_open())
    {
        timer.expires_after(tick);
        boost::system::error_code ec;
        co_await timer.async_wait(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec || !m_acceptor.is_open()) break;

        // For each registered service, if there's a live connection
        // and we haven't heard from it in offline_after, mark it
        // offline and broadcast an empty SERVICEDATA row to every
        // logged-in operator. Mirrors legacy OnCT_TIMER_REQ logic.
        const auto now_ms = static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        const auto threshold = static_cast<std::uint32_t>(offline_after.count());

        for (const auto& svc : m_cfg.peers->Services())
        {
            auto* st   = m_cfg.peers->Status(svc.service_id);
            auto  conn = m_cfg.peers->Connection(svc.service_id);
            if (!st) continue;
            const bool has_conn = conn && conn->Wire() && conn->Wire()->IsOpen();
            if (!has_conn)
            {
                // No connection — broadcast zero-filled SERVICEDATA so
                // the operator GUI shows "stopped" tiles. Legacy
                // emits this every tick when m_pConn is null.
                for (const auto& op : m_operators.SnapshotLoggedIn())
                {
                    if (!op || !op->Wire() || !op->Wire()->IsOpen()) continue;
                    co_await senders::SendServiceDataAck(op->Wire(),
                        svc.service_id, 0, 0, st->max_users, 0,
                        st->peak_time_unix, st->stop_count,
                        st->latest_stop_unix, 0);
                }
                continue;
            }
            // Have connection — if last_recv_tick is stale, drop it.
            if (st->last_recv_tick &&
                now_ms - st->last_recv_tick > threshold)
            {
                spdlog::warn("peer_keepalive: svc_id={:08x} stale "
                             "(>{}ms) — closing", svc.service_id, threshold);
                st->last_recv_tick = 0;
                conn->Wire()->Close();  // peer_loop tears down registry entry
                for (const auto& op : m_operators.SnapshotLoggedIn())
                {
                    if (!op || !op->Wire() || !op->Wire()->IsOpen()) continue;
                    co_await senders::SendServiceDataAck(op->Wire(),
                        svc.service_id, 0, 0, st->max_users, threshold,
                        st->peak_time_unix, st->stop_count,
                        st->latest_stop_unix, 0);
                }
                // Legacy: fire SMS alert on the 60s-timeout branch.
                if (m_cfg.alerter)
                    m_cfg.alerter->Notify(svc.type_id, svc.service_id, 3);
            }
        }
    }
}

} // namespace tcontrolsvr
