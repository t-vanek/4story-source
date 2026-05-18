#pragma once

// AdminShell — line-based TCP admin command interface. Inbound version
// of the legacy `CDebugSocket` (which connected outbound to a terminal
// server). Operators telnet to the configured port and run simple
// commands; the shell parses each \r\n-terminated line, dispatches,
// and replies on the same socket.
//
// Bind defaults to 127.0.0.1 so a misconfigured deploy doesn't expose
// the shell to the open internet. There is no authentication —
// operators are responsible for firewalling the port. Production deploys
// that need remote access should tunnel via SSH instead.
//
// Available commands:
//   help                — list commands
//   status              — live session count + uptime
//   kick <user_id>      — close the AsioSession holding `user_id`
//   ban-ip <ip>         — push an IP into the in-memory ban set
//   unban-ip <ip>       — remove from ban set
//   log-level <level>   — switch spdlog level (trace/debug/info/warn/...)
//   quit                — close the admin connection

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace tloginsvr::services { class IConnectionRegistry; }

namespace tloginsvr {

class AdminShell : public std::enable_shared_from_this<AdminShell>
{
public:
    // Bind on `bind_address`:`port`. Constructor opens the acceptor;
    // exceptions on bind failure surface to the caller (main treats
    // them as fatal). `connection_registry` non-owning, used for
    // kick + status.
    AdminShell(boost::asio::io_context& io,
               const std::string& bind_address,
               std::uint16_t port,
               services::IConnectionRegistry* connection_registry,
               std::chrono::steady_clock::time_point started_at);

    // Spawns the accept loop on the io_context. Idempotent.
    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const { return m_port; }

private:
    boost::asio::awaitable<void> HandleSession(
        std::shared_ptr<boost::asio::ip::tcp::socket> sock);

    std::string Dispatch(const std::string& line);

    boost::asio::ip::tcp::acceptor   m_acceptor;
    std::uint16_t                    m_port;
    services::IConnectionRegistry*   m_registry;
    std::chrono::steady_clock::time_point m_started_at;
};

} // namespace tloginsvr
