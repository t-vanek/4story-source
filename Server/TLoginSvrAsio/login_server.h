#pragma once

// LoginServer — orchestrates the accept loop, per-connection
// AsioSession lifecycle, and the per-message handler dispatch for
// the modernized TLoginSvrAsio binary. Header-only public surface;
// implementation lives in login_server.cpp.

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include "asio_session.h"
#include "MessageId.h"

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
    std::uint16_t port = 4815;

    // RC4 secret key for the inbound legacy-client wire. Empty
    // disables RC4 (server-server compatible mode; useful for tests
    // and the modernized-peer migration path). The shipped legacy
    // client expects a specific byte string — see
    // Session.cpp:16's g_strSecretKey.
    std::vector<std::byte> rc4_secret_key;
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
