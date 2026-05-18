#pragma once

// LoginServer — orchestrates the accept loop, per-connection
// AsioSession lifecycle, and the per-message handler dispatch for
// the modernized TLoginSvrAsio binary. Header-only public surface;
// implementation lives in login_server.cpp.

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <chrono>
#include <functional>

#include "asio_session.h"
#include "MessageId.h"
#include "fourstory/audit/audit_logger.h"
#include "services/auth_service.h"
#include "services/char_service.h"
#include "services/connection_registry.h"
#include "services/event_registry.h"
#include "services/map_server_locator.h"
#include "fourstory/ops/rate_limiter.h"
#include "services/session_terminator.h"
#include "fourstory/smtp/smtp_client.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace tloginsvr {

// Per-server configuration. Constructed by main from CLI flags / env
// vars. Tests use the default (no RC4 secret) to exercise the
// codec pipeline against another modernized peer.
struct LoginServerConfig
{
    // Default matches the legacy TSERVER row for bType=TLOGIN_GSP=2.
    std::uint16_t port = 4816;

    // RC4 secret key for the inbound legacy-client wire. Empty
    // disables RC4 (server-server compatible mode; useful for tests
    // and the modernized-peer migration path). The shipped legacy
    // client expects a specific byte string — see
    // Session.cpp:16's g_strSecretKey.
    std::vector<std::byte> rc4_secret_key;

    // Authentication backend. Non-owning pointer (caller owns the
    // service lifetime; typically constructed in main with the same
    // lifetime as the io_context). Null disables auth — handler
    // falls back to the version-check-only stub from Phase 3.
    services::IAuthService* auth_service = nullptr;

    // Connection registry for duplicate-session enforcement. Non-
    // owning. Null disables registry tracking — login handler still
    // works, just no kick-on-duplicate behavior (single-session
    // dev/test convenience).
    services::IConnectionRegistry* connection_registry = nullptr;

    // Map-server endpoint lookup. Non-owning. Null falls back to
    // the Phase-3 stub (SR_NOSERVER for every CS_START_REQ).
    services::IMapServerLocator* map_server_locator = nullptr;

    // Per-session cleanup on close (TCURRENTUSER delete + TLog
    // timestamp). Non-owning. Null = no cleanup hook fires.
    services::ISessionTerminator* session_terminator = nullptr;

    // Character lifecycle (CS_CHARLIST/CREATECHAR/DELCHAR backing).
    // Non-owning. Null falls back to Phase-3 stubs (empty list /
    // CR_INTERNAL / DR_INTERNAL).
    services::ICharService* char_service = nullptr;

    // Accepted client wire versions (CS_LOGIN_REQ.wVersion). Defaults
    // to the single legacy value 0x2918 (ProtocolBase.h::TVERSION).
    // Operators can extend the list in TOML to support older client
    // builds during a rolling upgrade or QA windows. Empty list →
    // reject every login with LR_VERSION (defensive: a forgotten
    // config shouldn't open the server to whatever wire blob shows
    // up).
    std::vector<std::uint16_t> accepted_versions = { 0x2918 };

    // Debug stress-test handlers. CS_TESTLOGIN_REQ +
    // CS_TESTVERSION_REQ. Disabled by default — production deploys
    // should never expose these (they bypass auth). Tooling enables
    // them via TOML.
    bool test_handlers_enabled = false;

    // Called when SM_QUITSERVICE_REQ arrives. Main typically wires
    // this to `io.stop()` so the wire-protocol shutdown matches the
    // SIGINT path. Null = SM_QUITSERVICE_REQ is logged and ignored.
    std::function<void()> on_quit_request;

    // Operational audit sink (login attempts, char create/delete,
    // game-start handoff). Non-owning. Null disables the audit
    // emission entirely — handlers carry on, just no audit records.
    fourstory::audit::IAuditLogger* audit_logger = nullptr;

    // Per-IP login rate limiter. Non-owning. Null disables (any
    // login attempt is allowed through to the auth service).
    fourstory::ops::LoginRateLimiter* login_rate_limiter = nullptr;

    // SMTP client for 2FA emails. Non-owning. Null disables the
    // outbound mail path — the LR_SECURITY challenge still runs, the
    // code is generated + stored in TSECURECODE, but no email lands
    // (useful for tests + dev mode where operators read the code from
    // the spdlog "audit" stream).
    fourstory::smtp::ISmtpClient* smtp_client = nullptr;

    // GM-event registry. Non-owning. CT_EVENTUPDATE_REQ persists
    // entries here; future GroupList ack consumers can read back.
    // Null silences the persistence path (handler still logs).
    services::IEventRegistry* event_registry = nullptr;

    // Pre-auth idle timeout. Connections that don't complete
    // CS_LOGIN_REQ within this window are dropped. 0 disables.
    // Defaults to 60s — generous for a real client, hostile to
    // half-open SYN-floods.
    std::chrono::seconds pre_auth_timeout{ 60 };
};

class LoginServer
{
public:
    LoginServer(boost::asio::io_context& io, LoginServerConfig config);

    // Legacy convenience overload — no RC4.
    LoginServer(boost::asio::io_context& io, std::uint16_t port);

    // Coroutine entry — runs the accept loop until the executor stops.
    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const;

private:
    boost::asio::io_context& m_io;
    tnetlib::AsioListener    m_listener;
    std::vector<std::byte>   m_rc4_secret_key; // copied from config; empty = no RC4
    services::IAuthService*        m_auth_service = nullptr;         // non-owning; null = stub mode
    services::IConnectionRegistry* m_connection_registry = nullptr;  // non-owning; null = no duplicate-kick
    services::IMapServerLocator*   m_map_server_locator = nullptr;   // non-owning; null = stub SR_NOSERVER
    services::ISessionTerminator*  m_session_terminator = nullptr;   // non-owning; null = no close-time cleanup
    services::ICharService*        m_char_service = nullptr;         // non-owning; null = stub responses
    fourstory::audit::IAuditLogger*        m_audit_logger = nullptr;         // non-owning; null = no audit emission
    fourstory::ops::LoginRateLimiter*    m_rate_limiter = nullptr;         // non-owning; null = no rate limiting
    services::IEventRegistry*      m_event_registry = nullptr;       // non-owning; null = no persistence
    fourstory::smtp::ISmtpClient*         m_smtp_client = nullptr;          // non-owning; null = no 2FA mail
    std::chrono::seconds           m_pre_auth_timeout{ 60 };
    std::vector<std::uint16_t>     m_accepted_versions;              // empty = reject all
    bool                           m_test_handlers_enabled = false;  // CS_TESTLOGIN_REQ / CS_TESTVERSION_REQ gate
    std::function<void()>          m_on_quit_request;                // SM_QUITSERVICE_REQ → io.stop() etc. May be null.

    // Per-connection coroutine: hand off the socket to a fresh
    // AsioSession, drive RunPackets, dispatch each decoded packet.
    boost::asio::awaitable<void> HandleConnection(
        std::shared_ptr<tnetlib::AsioSession> sess);

    // Dispatch one decoded packet to its handler. Returns awaitable<void>
    // so handlers can `co_await` SendPacket calls.
    boost::asio::awaitable<void> Dispatch(
        std::shared_ptr<tnetlib::AsioSession> sess,
        const tnetlib::DecodedPacket& packet);
};

} // namespace tloginsvr
