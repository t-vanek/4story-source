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

#include <cstdint>
#include <memory>

namespace tloginsvr {

class LoginServer
{
public:
    LoginServer(boost::asio::io_context& io, std::uint16_t port);

    // Coroutine entry — runs the accept loop until the executor stops.
    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const;

private:
    boost::asio::io_context& m_io;
    tnetlib::AsioListener    m_listener;

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
