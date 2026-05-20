#pragma once

#include "patch_session.h"
#include "services/patch_repository.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

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
    PatchRepository*   repo = nullptr;        // non-owning
    std::string        ftp_url;
    std::string        pre_ftp_url;
    std::string        login_host;
    std::uint16_t      login_port = 0;
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
    // safety net in case TControlSvr never connects.
    std::size_t SweepStaleClients(std::chrono::seconds max_age);

private:
    boost::asio::awaitable<void> HandleConnection(
        std::shared_ptr<PatchSession> session);

    boost::asio::awaitable<void> StaleClientSweepLoop();

    void Register(std::shared_ptr<PatchSession> session);
    void Unregister(PatchSession* raw);

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
