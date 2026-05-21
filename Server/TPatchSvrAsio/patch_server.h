#pragma once

#include "patch_session.h"
#include "services/patch_repository.h"

#include "fourstory/security/peer_security_gate.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

namespace boost::asio { class thread_pool; }

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace tpatchsvr {

struct PatchServerConfig
{
    std::uint16_t      port = 3715;
    IPatchRepository*  repo = nullptr;        // non-owning
    std::string        ftp_url;
    std::string        pre_ftp_url;
    std::string        login_host;
    std::uint16_t      login_port = 0;
    // Optional worker pool for off-loop SOCI calls.
    // MarkPreVersionComplete (txn write path) is the primary
    // beneficiary; reads are best-effort.
    boost::asio::thread_pool* db_pool = nullptr;

    // Server-to-server security gate. nullptr → no gating (legacy
    // posture); non-null → CheckIp() runs on every accept().
    fourstory::security::PeerSecurityGate* security = nullptr;
};

class PatchServer
{
public:
    PatchServer(boost::asio::io_context& io, PatchServerConfig config);

    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const { return m_port; }

    // Sweep client-typed sessions whose connected_at is older than
    // `max_age`. Closes each match. Returns the number closed.
    // Mirrors legacy `OnCT_SERVICEMONITOR_ACK`'s stale-client purge
    // (P-6 in the audit). Called from the service-monitor handler;
    // also driven by a periodic timer as a belt-and-suspenders
    // safety net in case TControlSvr never connects. Parameter type
    // is `milliseconds` (not `seconds`) so tests can drive the sweep
    // on a sub-second scale; production callers pass `60s` which
    // implicitly converts.
    std::size_t SweepStaleClients(std::chrono::milliseconds max_age);

    // Session-registry hooks. Public so tests can drive the sweep
    // without standing up the full accept loop; Run/HandleConnection
    // are the only normal callers.
    void Register(std::shared_ptr<PatchSession> session);
    void Unregister(PatchSession* raw);

private:
    boost::asio::awaitable<void> HandleConnection(
        std::shared_ptr<PatchSession> session);

    boost::asio::awaitable<void> StaleClientSweepLoop();

    boost::asio::io_context&        m_io;
    boost::asio::ip::tcp::acceptor  m_acceptor;
    std::uint16_t                   m_port;
    PatchServerConfig               m_cfg;
    std::atomic<std::size_t>        m_live_sessions{0};

    // Weak handles to currently open sessions, so the sweep can
    // close clients that never completed the patch handshake.
    // weak_ptr lets the sweep run lock-free w.r.t. session
    // destruction.
    std::mutex                                          m_sessions_mtx;
    std::vector<std::weak_ptr<PatchSession>>            m_sessions;
};

} // namespace tpatchsvr
