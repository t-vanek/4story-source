#include "control_server.h"

#include "senders.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>
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

        // Server-to-server IP allowlist enforcement. Operators connect
        // through the same listener as peer-register traffic, so the
        // gate applies uniformly. CT_OPLOGIN's per-IP token bucket
        // (login_rate) runs separately on top.
        if (m_cfg.security != nullptr)
        {
            boost::system::error_code pec;
            const auto ep = sock.remote_endpoint(pec);
            const std::string ip =
                pec ? std::string{"?"} : ep.address().to_string();
            const auto check = m_cfg.security->CheckIp(ip);
            if (!check.allowed())
            {
                spdlog::warn("control_svr: rejected accept from {} ({})",
                    ip, fourstory::security::OutcomeName(check.outcome));
                boost::system::error_code ig;
                sock.close(ig);
                continue;
            }
        }

        std::shared_ptr<ControlSession> sess;

        if (m_cfg.ssl_ctx != nullptr)
        {
            // Hybrid first-byte detection for backward-compat with
            // legacy TController.exe (which speaks plain TCP).
            // TLS ClientHello always starts with 0x16 (Handshake
            // content type); the legacy operator protocol's first
            // byte is wSize's low byte and is never 0x16 in practice
            // — the legacy CT_OPLOGIN frame is at minimum 18 bytes,
            // so wSize >= 18 and wSize is small (< 0x1000), the low
            // byte is the size mod 256 which never lands on 0x16 for
            // any defined CT_* opcode. If it ever does for a future
            // opcode, that opcode's frame layout has to grow past
            // 0x1600 bytes before this confuses us.
            //
            // We MSG_PEEK one byte so the TLS handshake (or the
            // legacy framer) still sees the original byte stream
            // unchanged.
            std::array<unsigned char, 1> peek{};
            boost::system::error_code peek_ec;
            const auto peeked = co_await sock.async_receive(
                boost::asio::buffer(peek),
                tcp::socket::message_peek,
                boost::asio::redirect_error(
                    boost::asio::use_awaitable, peek_ec));
            if (peek_ec || peeked == 0)
            {
                spdlog::debug("control_svr: peek failed from {} — closing",
                    sock.remote_endpoint(peek_ec).address().to_string());
                boost::system::error_code ig;
                sock.close(ig);
                continue;
            }

            const bool looks_like_tls = (peek[0] == 0x16);
            if (looks_like_tls)
            {
                // Server-side TLS — wrap and handshake. Same path as
                // before, just gated on the peek.
                ControlSession::TlsStream stream(std::move(sock),
                                                  *m_cfg.ssl_ctx);
                boost::system::error_code tls_ec;
                co_await stream.async_handshake(
                    boost::asio::ssl::stream_base::server,
                    boost::asio::redirect_error(
                        boost::asio::use_awaitable, tls_ec));
                if (tls_ec)
                {
                    spdlog::warn("control_svr: TLS handshake failed: {}",
                        tls_ec.message());
                    boost::system::error_code ig;
                    stream.next_layer().shutdown(
                        tcp::socket::shutdown_both, ig);
                    stream.next_layer().close(ig);
                    continue;
                }
                sess = std::make_shared<ControlSession>(std::move(stream));
                spdlog::info("control_svr: TLS accept from {}",
                    sess->RemoteIPv4());
            }
            else
            {
                // Plain TCP path — legacy operator binary on a
                // TLS-enabled listener. Logged so operators see the
                // mix; future Phase B token validation can refuse
                // operator commands on plain sessions if required.
                sess = std::make_shared<ControlSession>(std::move(sock));
                spdlog::info("control_svr: plain accept from {} "
                             "(legacy operator path)",
                    sess->RemoteIPv4());
            }
        }
        else
        {
            // No TLS context configured — plain TCP for everyone.
            sess = std::make_shared<ControlSession>(std::move(sock));
            spdlog::info("control_svr: accept from {}", sess->RemoteIPv4());
        }

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
    ctx.peer_repo  = m_cfg.peer_repo;
    ctx.security   = m_cfg.security;
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

boost::asio::awaitable<void>
ControlServer::RegistryLeaseExpiryLoop(std::chrono::seconds max_age,
                                       std::chrono::seconds tick)
{
    if (!m_cfg.peers) co_return;
    boost::asio::steady_timer timer(m_io);
    while (m_acceptor.is_open())
    {
        timer.expires_after(tick);
        boost::system::error_code ec;
        co_await timer.async_wait(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec || !m_acceptor.is_open()) break;

        const auto expired = m_cfg.peers->ExpireStale(max_age);
        if (expired > 0)
            spdlog::warn("registry: expired {} stale lease(s) "
                         "(no heartbeat within {}s)",
                expired, static_cast<long long>(max_age.count()));
    }
}

} // namespace tcontrolsvr
