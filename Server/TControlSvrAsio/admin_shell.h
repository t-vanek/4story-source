#pragma once

// AdminShell — line-based TCP admin interface, the single operator
// entry point for the whole 4Story server cluster. Replaces the
// per-server localhost shells we used to ship in TLogin/TControl
// (those drifted in command set; centralizing keeps the audit trail
// single-sourced and the operator runbook short).
//
// Each \r\n-terminated command runs against this binary's live
// state (PeerRegistry + IServiceController + senders::Send*Ack). The
// cross-server commands (`kick`, `announce`, `service start/stop`)
// reach peer game servers (TLogin, TMap, TLog, TPatch, TWorld) via
// the existing CT_* peer protocol — no new wire surface needed.
//
// Bind defaults to 127.0.0.1; operators who need remote access tunnel
// over SSH. There is intentionally no authentication on the shell
// itself — the GM authority gate sits on the operator wire path
// (CT_OPLOGIN_REQ + AuthorityGate); the admin shell is meant for the
// box operator who already has shell access.

#include "control_session.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace tcontrolsvr {

class PeerRegistry;
class IServiceController;
class IAdminAuditLogger;

// Returns the count of live operator sessions for the `status` line.
// Wired in main.cpp to ControlServer::LiveOperators(); tests pass a
// stub.
using OperatorCountFn = std::function<std::size_t()>;

class AdminShell : public std::enable_shared_from_this<AdminShell>
{
public:
    AdminShell(boost::asio::io_context& io,
               const std::string& bind_address,
               std::uint16_t port,
               OperatorCountFn operator_count,
               PeerRegistry& peers,
               IServiceController& controller,
               IAdminAuditLogger* audit,
               std::chrono::steady_clock::time_point started_at);

    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const { return m_port; }

    // Test seam: drive a single command line against the shell's
    // dispatch coroutine without standing up the TCP listener.
    // Returns the reply text (without the trailing "\n> " prompt).
    // Used by test_admin_shell.cpp.
    boost::asio::awaitable<std::string>
        DispatchForTest(const std::string& line);

private:
    boost::asio::awaitable<void> HandleSession(
        std::shared_ptr<boost::asio::ip::tcp::socket> sock);

    boost::asio::awaitable<std::string> Dispatch(const std::string& line);

    // Sub-handlers — each returns the reply string. Async ones (status,
    // start, stop) co_await IServiceController; the rest are sync.
    std::string                          CmdStatus()  const;
    std::string                          CmdPeers()   const;
    std::string                          CmdRegistry() const;
    std::string                          CmdKick(std::uint32_t uid,
                                                  const std::string& user);
    std::string                          CmdAnnounce(const std::string& msg);
    boost::asio::awaitable<std::string>  CmdServiceStatus(std::uint32_t sid);
    boost::asio::awaitable<std::string>  CmdServiceStart(std::uint32_t sid);
    boost::asio::awaitable<std::string>  CmdServiceStop(std::uint32_t sid);
    std::string                          CmdLogLevel(const std::string& lvl);

    boost::asio::ip::tcp::acceptor   m_acceptor;
    std::uint16_t                    m_port;
    OperatorCountFn                  m_operator_count;
    PeerRegistry&                    m_peers;
    IServiceController&              m_controller;
    IAdminAuditLogger*               m_audit;
    std::chrono::steady_clock::time_point m_started_at;
};

} // namespace tcontrolsvr
