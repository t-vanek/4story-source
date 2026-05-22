#pragma once

// ControlServer — accept loop + per-session coroutine wiring. F1
// only handles inbound operator sockets (OperatorSession). F2 adds
// the outbound peer dialer and the SERVICEMONITOR-driven flip from
// "looks like an operator" to "this is a PeerSession".

#include "control_session.h"
#include "operator_session.h"
#include "peer_session.h"
#include "handlers/handlers.h"
#include "services/admin_audit_logger.h"
#include "services/alerter.h"
#include "services/chat_ban_repository.h"
#include "services/event_registry.h"
#include "services/event_repository.h"
#include "services/operator_registry.h"
#include "services/patch_metadata_service.h"
#include "services/peer_registry.h"
#include "services/peer_repository.h"
#include "services/service_controller.h"
#include "services/user_protected_service.h"

#include "fourstory/ops/rate_limiter.h"
#include "fourstory/security/peer_security_gate.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/thread_pool.hpp>

#include <cstdint>
#include <memory>

namespace tcontrolsvr {

class PeerDialer;

struct ControlServerConfig
{
    std::uint16_t          port = 0;
    IOperatorAuthService*  auth        = nullptr;
    IServiceInventory*     inventory   = nullptr;
    IServiceController*    controller  = nullptr;
    PeerDialer*            dialer      = nullptr;
    PeerRegistry*          peers       = nullptr;
    IAdminAuditLogger*     audit       = nullptr;
    IUserProtectedService* user_ban    = nullptr;
    ChatBanRepository*     chat_bans   = nullptr;
    EventRegistry*         events      = nullptr;
    IEventRepository*      event_repo  = nullptr;
    IPatchMetadataService* patch_meta  = nullptr;
    IAlerter*              alerter     = nullptr;
    IPeerRepository*       peer_repo   = nullptr;
    fourstory::security::PeerSecurityGate* security = nullptr;
    fourstory::ops::LoginRateLimiter* login_rate = nullptr;
    // Worker pool for synchronous SOCI calls. nullptr → fall back
    // to in-line execution on the io_context thread.
    boost::asio::thread_pool* db_pool = nullptr;
    std::uint8_t           auto_start  = 0;

    // Optional server-side TLS context. When non-null, the accept
    // loop drives a server-side TLS handshake before constructing
    // the ControlSession. Caller owns the lifetime (typically a
    // std::optional<ssl::context> on the main.cpp stack, built once
    // via fourstory::security::PeerTlsContextBuilder::BuildServerContext).
    //
    // Phase A.1 limitation: when set, the legacy operator binary
    // (TController.exe) cannot connect to this listener because it
    // speaks plain TCP. A hybrid first-byte-detection accept loop
    // is planned for Phase A.2 to restore operator compatibility.
    boost::asio::ssl::context* ssl_ctx = nullptr;
};

class ControlServer
{
public:
    ControlServer(boost::asio::io_context& io, ControlServerConfig cfg);

    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const { return m_port; }

    // Test hook — drive a single session through the dispatch loop
    // without binding a socket. The session is constructed by the
    // caller, then handed off here.
    boost::asio::awaitable<void> Drive(std::shared_ptr<ControlSession> sess);

    std::size_t LiveOperators() const { return m_operators.Size(); }

    OperatorRegistry&       Operators()       { return m_operators; }
    const OperatorRegistry& Operators() const { return m_operators; }

    // 1Hz peer keep-alive watchdog (legacy TimerThread). Walks the
    // PeerRegistry, marks peers offline if last_recv_tick is older
    // than `offline_after`, and broadcasts an empty SERVICEDATA_ACK
    // to operators on the transition. Public so main can spawn it.
    boost::asio::awaitable<void> PeerKeepaliveLoop(
        std::chrono::milliseconds offline_after = std::chrono::seconds(60),
        std::chrono::milliseconds tick = std::chrono::seconds(1));

    // Lease expiry sweep for modern peer self-registration. Peers send
    // CT_PEER_HEARTBEAT_REQ every ~30s (kHeartbeatIntervalSec in
    // handlers_registry.cpp); this loop drops registry entries whose
    // last heartbeat is older than `max_age`. Default 90s = three
    // missed heartbeats — long enough that one dropped UDP packet on
    // a flaky link doesn't reap a healthy peer.
    boost::asio::awaitable<void> RegistryLeaseExpiryLoop(
        std::chrono::seconds max_age = std::chrono::seconds(90),
        std::chrono::seconds tick    = std::chrono::seconds(15));

private:
    boost::asio::awaitable<void> HandleConnection(
        std::shared_ptr<ControlSession> sess);

    boost::asio::io_context&        m_io;
    boost::asio::ip::tcp::acceptor  m_acceptor;
    std::uint16_t                   m_port;
    ControlServerConfig             m_cfg;
    OperatorRegistry                m_operators;
    std::uint8_t                    m_auto_start = 0;
};

} // namespace tcontrolsvr
