#pragma once

// AdminShell — minimal TCP text protocol for operator commands.
//
// Wire: newline-delimited text. Each connection is a session that
// optionally authenticates with `auth <secret>` first (if [admin]
// secret is configured), then sends one command per line and reads
// one or more lines back.
//
// Commands (T6 set):
//   help                       List available commands.
//   status                     Session count, peer states, summary
//                              metrics.
//   kick <char_id> [reason]    Close the session for char_id via
//                              session_reg. Logs the reason.
//   broadcast <msg>            Stubbed — will push to all channels
//                              via presence once the CT_ANNOUNCE
//                              outbound encoder lands.
//   peer-status                Connection state of world_client +
//                              log_peer.
//   quit                       Close the admin connection.
//
// Bind: 127.0.0.1:port by default. Operators who need remote access
// should tunnel via SSH rather than expose the shell to the open
// internet. The optional shared secret adds a second factor; bind
// + secret is the production posture.

#include "handlers.h"   // HandlerContext provides all the services

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <string>

namespace tmapsvr::ops {

struct AdminShellConfig
{
    std::string    bind   = "127.0.0.1";
    std::uint16_t  port   = 0;         // 0 = disabled
    std::string    secret;             // empty = no auth required
};

class AdminShell
{
public:
    AdminShell(boost::asio::io_context& io,
               AdminShellConfig         cfg,
               const HandlerContext&    services);

    // Run the accept loop. Returns when the executor stops or the
    // acceptor is closed.
    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const { return m_port; }

    // Stop accepting new admin connections (graceful shutdown).
    void StopAccepting();

private:
    boost::asio::awaitable<void> HandleConnection(
        boost::asio::ip::tcp::socket sock);

    boost::asio::awaitable<bool> DispatchLine(
        boost::asio::ip::tcp::socket& sock,
        const std::string&            line);

    boost::asio::io_context&       m_io;
    AdminShellConfig               m_cfg;
    const HandlerContext&          m_services;
    boost::asio::ip::tcp::acceptor m_acceptor;
    std::uint16_t                  m_port;
};

} // namespace tmapsvr::ops
