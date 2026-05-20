// LoginServer implementation — accept loop, per-connection session
// lifecycle, per-packet dispatch.
//
// Architecture (mirrors legacy CTLoginSvrModule with a portable
// reactor underneath):
//   Run()                    — AsioListener accept loop. Per accept:
//                              gate on max_connections, attach RC4
//                              codec when configured, hand the socket
//                              to a fresh AsioSession, co_spawn the
//                              per-connection coroutine with a
//                              top-level exception trap.
//   HandleConnection()       — pre-auth idle watchdog + RunPackets
//                              read loop + post-disconnect cleanup
//                              chain (TCURRENTUSER delete / TLOG
//                              timestamp via ISessionTerminator).
//   Dispatch()               — switch on MessageId; one case per
//                              handler. CT_* messages additionally
//                              gate on peer IP == control_server_ip.
//
// Exception safety: handler-side throws (SOCI pool exhaustion, DB
// outages, constraint violations) are caught per-packet and per-
// connection so a single bad query can't take down the io_context.
//
// PCH-free; depends only on TNetLib's portable surface (AsioSession,
// AsioListener, MessageId) plus Boost.Asio.
//
// Legacy parity: Server/TLoginSvr/CTLoginSvrModule.cpp (IOCP accept
// loop), Server/TLoginSvr/CSHandler.cpp (dispatch switch).

#include "login_server.h"
#include "handlers.h"

#include "fourstory/db/session_pool.h"   // fourstory::db::AcquireTimeout

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <exception>
#include <utility>

namespace tloginsvr {

LoginServer::LoginServer(boost::asio::io_context& io, LoginServerConfig config)
    : m_io(io)
    , m_listener(io.get_executor(), config.port)
    , m_rc4_secret_key(std::move(config.rc4_secret_key))
    , m_auth_service(config.auth_service)
    , m_db_pool(config.db_pool)
    , m_connection_registry(config.connection_registry)
    , m_map_server_locator(config.map_server_locator)
    , m_session_terminator(config.session_terminator)
    , m_char_service(config.char_service)
    , m_audit_logger(config.audit_logger)
    , m_rate_limiter(config.login_rate_limiter)
    , m_event_registry(config.event_registry)
    , m_smtp_client(config.smtp_client)
    , m_pre_auth_timeout(config.pre_auth_timeout)
    , m_accepted_versions(std::move(config.accepted_versions))
    , m_test_handlers_enabled(config.test_handlers_enabled)
    , m_on_quit_request(std::move(config.on_quit_request))
    , m_nation(config.nation)
    , m_control_server_ip(std::move(config.control_server_ip))
    , m_max_connections(config.max_connections)
{
}

LoginServer::LoginServer(boost::asio::io_context& io, std::uint16_t port)
    : LoginServer(io, LoginServerConfig{ .port = port })
{
}

std::uint16_t LoginServer::Port() const
{
    return m_listener.Port();
}

boost::asio::awaitable<void>
LoginServer::Run()
{
    co_await m_listener.Run([this](boost::asio::ip::tcp::socket socket) {
        // Pre-auth flood gate: bound the total in-flight TCP sessions
        // so a misbehaving / hostile peer can't hold thousands of half-
        // open sockets in memory waiting for the per-session pre-auth
        // watchdog to fire. Counter is decremented in HandleConnection
        // after the cleanup chain completes.
        if (m_max_connections > 0)
        {
            const auto current = m_active_connections.load(std::memory_order_relaxed);
            if (current >= m_max_connections)
            {
                boost::system::error_code peer_ec;
                const auto peer = socket.remote_endpoint(peer_ec);
                spdlog::warn("max_connections cap reached ({} >= {}); dropping new accept from {}",
                    current, m_max_connections,
                    peer_ec ? std::string{"<unknown>"} : peer.address().to_string());
                boost::system::error_code ignored;
                socket.close(ignored);
                return;
            }
        }
        m_active_connections.fetch_add(1, std::memory_order_relaxed);

        // PeerType::Client if we're running with the legacy-client
        // secret configured (Phase 2.5 wire compat); PeerType::Server
        // for the modernized peer test mode. The peer type label
        // doesn't change behavior here — we toggle RC4 explicitly
        // via EnableInboundRC4 — but keeping it semantically right
        // means future code reading sess->Type() gets the truthful
        // answer about "what kind of peer is on the other side".
        const auto peer = m_rc4_secret_key.empty()
            ? tnetlib::PeerType::Server
            : tnetlib::PeerType::Client;
        auto sess = std::make_shared<tnetlib::AsioSession>(
            std::move(socket), peer);

        if (!m_rc4_secret_key.empty())
        {
            // Server-side: legacy convention is RC4 inbound only,
            // XOR-only outbound (matches CSession::Decrypt RC4 path
            // vs CSession::Encrypt XOR-only path).
            sess->EnableInboundRC4(m_rc4_secret_key);
        }

        // Top-level exception handler for the per-connection coroutine.
        // Any exception that escapes HandleConnection (e.g. a SOCI
        // throw that bubbled past the per-dispatch try/catch via a
        // path we missed, or an OOM in std::vector::reserve) gets
        // logged and absorbed here instead of being delivered to
        // co_spawn's "rethrow on detached" trap — which would call
        // std::terminate() and take the whole io_context down.
        boost::asio::co_spawn(
            m_io,
            HandleConnection(sess),
            [](std::exception_ptr ep) {
                if (!ep) return;
                try { std::rethrow_exception(ep); }
                catch (const std::exception& ex) {
                    spdlog::error("HandleConnection coroutine threw: {}", ex.what());
                }
                catch (...) {
                    spdlog::error("HandleConnection coroutine threw a non-std exception");
                }
            });
    });
}

namespace {

// RAII counter decrement — ensures m_active_connections gets
// decremented even if HandleConnection unwinds via exception.
struct ConnectionCounterGuard
{
    std::atomic<std::uint32_t>* counter;
    bool armed;
    ~ConnectionCounterGuard() {
        if (armed && counter) counter->fetch_sub(1, std::memory_order_relaxed);
    }
};

} // namespace

boost::asio::awaitable<void>
LoginServer::HandleConnection(std::shared_ptr<tnetlib::AsioSession> sess)
{
    // Tracks the slot we reserved in Run()'s listener callback. The
    // guard fires unconditionally on scope exit — including the
    // exceptional path — so max_connections accounting can't leak
    // even if a handler or the cleanup chain throws.
    ConnectionCounterGuard slot_guard{
        &m_active_connections, m_max_connections > 0 };

    // Pre-auth idle watchdog. Spawn a deadline that closes the
    // socket if the session hasn't completed CS_LOGIN_REQ within
    // m_pre_auth_timeout. Cancel when RunPackets returns (real
    // disconnect path) — the timer's `wait` will get error_code
    // operation_aborted which is fine, we just bail.
    auto watchdog = std::make_shared<boost::asio::steady_timer>(m_io);
    if (m_pre_auth_timeout.count() > 0)
    {
        watchdog->expires_after(m_pre_auth_timeout);
        const auto registry = m_connection_registry;
        const auto timeout = m_pre_auth_timeout;
        boost::asio::co_spawn(
            m_io,
            [sess, watchdog, registry, timeout]() -> boost::asio::awaitable<void> {
                boost::system::error_code ec;
                co_await watchdog->async_wait(
                    boost::asio::redirect_error(boost::asio::use_awaitable, ec));
                if (ec) co_return; // timer cancelled — connection healthy
                // Timer elapsed. If the session is registered, auth
                // already finished and we don't need to act. Otherwise
                // it's stuck pre-auth and gets dropped.
                if (registry && registry->Lookup(sess).has_value())
                    co_return;
                spdlog::warn("pre-auth idle timeout ({}s) — closing session", timeout.count());
                sess->Close();
            },
            boost::asio::detached);
    }

    // RunPackets fires the handler synchronously per packet, but the
    // handler we want to invoke is awaitable (it issues SendPacket).
    // Spawn a per-packet coroutine instead of trying to do it inline,
    // so the codec read loop isn't blocked by send completions.
    //
    // The completion handler swallows handler-side exceptions: SOCI
    // calls inside the dispatch chain can throw on DB outages, pool
    // timeouts, or constraint violations. Without this we'd hit
    // `boost::asio::detached`'s "rethrow if unhandled" policy and
    // std::terminate() the whole io_context — one bad query would
    // take down every active connection.
    co_await sess->RunPackets(
        [this, sess](const tnetlib::DecodedPacket& packet) {
            boost::asio::co_spawn(
                m_io,
                Dispatch(sess, packet),
                [sess](std::exception_ptr ep) {
                    if (!ep) return;
                    try { std::rethrow_exception(ep); }
                    catch (const std::exception& ex) {
                        spdlog::error("dispatch coroutine threw (peer={}): {}",
                            sess->RemoteIPv4(), ex.what());
                    }
                    catch (...) {
                        spdlog::error("dispatch coroutine threw a non-std exception (peer={})",
                            sess->RemoteIPv4());
                    }
                    // Close the socket so the client sees a disconnect
                    // rather than hanging. Logging is intentional —
                    // ops needs to know when handlers fail.
                    sess->Close();
                });
        });

    // Cancel the pre-auth watchdog (no-op if it already fired or was
    // never armed). Otherwise it might keep a shared_ptr to sess
    // alive past the connection close. cancel() in Asio 1.91 returns
    // the number of pending handlers and doesn't throw — no need for
    // an error_code overload.
    if (watchdog) watchdog->cancel();

    // Connection closing — drive the per-session cleanup chain:
    // 1. Look up the entry stamped at LOGIN-success time (if any).
    //    Unauthenticated sessions have no entry → skip everything.
    // 2. If a terminator is wired, call Terminate with the entry's
    //    user_id + session_key. The MapHandoff flag flips the reason
    //    so the impl can preserve TCURRENTUSER for Map's dwKEY
    //    validation (legacy CSHandler.cpp:1428 behavior).
    // 3. Always unregister from the connection registry.
    //
    // Wrapped in try/catch so a DB blip during Terminate() (e.g. the
    // pool just hit its acquire_timeout) doesn't escape into the
    // outer co_spawn frame as an unhandled exception.
    try
    {
        if (m_connection_registry)
        {
            const auto entry = m_connection_registry->Lookup(sess);
            if (entry && m_session_terminator)
            {
                const auto reason = entry->handoff_to_map
                    ? services::TerminationReason::MapHandoff
                    : services::TerminationReason::Disconnect;
                m_session_terminator->Terminate(
                    entry->user_id, entry->session_key, reason,
                    entry->last_char_id);
            }
            m_connection_registry->Unregister(sess);
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("session cleanup failed (peer={}): {}",
            sess->RemoteIPv4(), ex.what());
    }

    // m_active_connections decrement is handled by slot_guard's
    // destructor (RAII) — covers both the happy path and the
    // exception-unwind path uniformly.
}

boost::asio::awaitable<void>
LoginServer::Dispatch(std::shared_ptr<tnetlib::AsioSession> sess,
                      const tnetlib::DecodedPacket& packet)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToMessageId;

    // Copy the body once — `packet.body` is only valid for the
    // duration of the RunPackets callback that produced this Dispatch
    // call, and we've already returned from it via the co_spawn.
    std::vector<std::byte> body(packet.body.begin(), packet.body.end());
    const auto id = ToMessageId(packet.wId);

    // Wrap the whole dispatch in a try/catch so handler-side throws
    // (SOCI pool exhaustion, DB outages, constraint violations) get
    // logged + closed cleanly per-connection rather than escaping into
    // co_spawn's detached completion path. The outer Run() completion
    // handler is a safety net, but catching here lets us identify the
    // message id that triggered the failure for ops triage.
    try
    {
        switch (id)
        {
    case MessageId::CS_LOGIN_REQ:
        co_await handlers::OnLoginReq(sess, body, m_auth_service,
            m_connection_registry, m_audit_logger, m_rate_limiter,
            std::span<const std::uint16_t>(
                m_accepted_versions.data(), m_accepted_versions.size()),
            m_smtp_client, m_nation, m_db_pool);
        break;
    case MessageId::CS_GROUPLIST_REQ:
        co_await handlers::OnGroupListReq(sess, body, m_map_server_locator,
            m_connection_registry, m_db_pool);
        break;
    case MessageId::CS_CHANNELLIST_REQ:
        co_await handlers::OnChannelListReq(sess, body, m_map_server_locator,
            m_connection_registry, m_db_pool);
        break;
    case MessageId::CS_CHARLIST_REQ:
        co_await handlers::OnCharListReq(sess, body, m_char_service,
            m_connection_registry, m_db_pool);
        break;
    case MessageId::CS_CREATECHAR_REQ:
        co_await handlers::OnCreateCharReq(sess, body, m_char_service,
            m_connection_registry, m_audit_logger, m_nation, m_db_pool);
        break;
    case MessageId::CS_DELCHAR_REQ:
        co_await handlers::OnDelCharReq(sess, body, m_char_service,
            m_connection_registry, m_auth_service, m_audit_logger, m_db_pool);
        break;
    case MessageId::CS_START_REQ:
        co_await handlers::OnStartReq(sess, body, m_map_server_locator,
            m_connection_registry, m_audit_logger, m_db_pool);
        break;
    case MessageId::CS_AGREEMENT_REQ:
        co_await handlers::OnAgreementReq(sess, body, m_auth_service,
            m_connection_registry, m_db_pool);
        break;
    case MessageId::CS_HOTSEND_REQ:
        co_await handlers::OnHotsendReq(sess, body);
        break;
    case MessageId::CS_VETERAN_REQ:
        co_await handlers::OnVeteranReq(*sess, body, m_char_service, m_db_pool);
        break;
    case MessageId::CS_TERMINATE_REQ:
        co_await handlers::OnTerminateReq(*sess, body);
        break;
    case MessageId::CS_SECURITYCONFIRM_ACK:
        co_await handlers::OnSecurityConfirmAck(sess, body,
            m_auth_service, m_connection_registry, m_audit_logger, m_db_pool);
        break;
    case MessageId::CS_TESTLOGIN_REQ:
        if (m_test_handlers_enabled)
        {
            co_await handlers::OnTestLoginReq(sess, body, m_auth_service,
                m_connection_registry, m_audit_logger, m_db_pool);
        }
        else
        {
            spdlog::warn("CS_TESTLOGIN_REQ received but test_handlers_enabled=false — dropping");
        }
        break;
    case MessageId::CS_TESTVERSION_REQ:
        if (m_test_handlers_enabled)
            co_await handlers::OnTestVersionReq(*sess, body);
        else
            spdlog::warn("CS_TESTVERSION_REQ received but test_handlers_enabled=false — dropping");
        break;
    // Control protocol — TControlSvr / GM tooling. Legacy gates
    // these on `m_bSessionType == SESSION_SERVER`, set in Accept()
    // by comparing the peer's IP against the resolved Control Server
    // address. We mirror that: only the configured control_server_ip
    // is allowed to drive event/state mutations. Empty config = gate
    // open (single-process dev + in-process tests).
    case MessageId::CT_SERVICEMONITOR_ACK:
    case MessageId::CT_SERVICEDATACLEAR_ACK:
    case MessageId::CT_CTRLSVR_REQ:
    case MessageId::CT_EVENTUPDATE_REQ:
    case MessageId::CT_EVENTMSG_REQ:
        if (!m_control_server_ip.empty() && sess->RemoteIPv4() != m_control_server_ip)
        {
            spdlog::warn("ct: dropping {} from non-control peer {} (expected {})",
                tnetlib::protocol::NameOf(id),
                sess->RemoteIPv4(),
                m_control_server_ip);
            break;
        }
        switch (id)
        {
        case MessageId::CT_SERVICEMONITOR_ACK:
            co_await handlers::OnControlServiceMonitor(*sess, body, m_connection_registry);
            break;
        case MessageId::CT_SERVICEDATACLEAR_ACK:
            co_await handlers::OnControlServiceDataClear(*sess, body);
            break;
        case MessageId::CT_CTRLSVR_REQ:
            co_await handlers::OnControlCtrlSvr(*sess, body);
            break;
        case MessageId::CT_EVENTUPDATE_REQ:
            co_await handlers::OnControlEventUpdate(*sess, body, m_event_registry);
            break;
        case MessageId::CT_EVENTMSG_REQ:
            co_await handlers::OnControlEventMsg(*sess, body);
            break;
        default: break;  // unreachable — outer case covers exactly these five
        }
        break;
    case MessageId::SM_QUITSERVICE_REQ:
        co_await handlers::OnQuitServiceReq(*sess, body, m_on_quit_request);
        break;
    default:
    {
        const auto name = tnetlib::protocol::NameOf(id);
        spdlog::warn("unhandled packet id=0x{:04X} ({}) body={} bytes",
            packet.wId,
            name.empty() ? std::string_view{"unknown"} : name,
            body.size());
        break;
    }
    }
    }
    catch (const fourstory::db::AcquireTimeout& ex)
    {
        // Pool saturated — log at warn (this is an operational issue
        // but the rest of the server should keep serving). The client
        // sees a disconnect, which they'll retry; better than the
        // server hanging until the io_context dies.
        spdlog::warn("dispatch {}: pool exhausted, dropping session (peer={}): {}",
            tnetlib::protocol::NameOf(id), sess->RemoteIPv4(), ex.what());
        sess->Close();
    }
    catch (const std::exception& ex)
    {
        spdlog::error("dispatch {}: handler threw, closing session (peer={}): {}",
            tnetlib::protocol::NameOf(id), sess->RemoteIPv4(), ex.what());
        sess->Close();
    }
    catch (...)
    {
        spdlog::error("dispatch {}: handler threw a non-std exception, closing session (peer={})",
            tnetlib::protocol::NameOf(id), sess->RemoteIPv4());
        sess->Close();
    }
}

} // namespace tloginsvr
