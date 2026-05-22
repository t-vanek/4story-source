#pragma once

// WorldServer — accept loop + per-session coroutine wiring. W1
// shipped the accept gate; W2 added the HandlerContext for the
// char registry; **W3a-2** rewraps each accepted WorldSession in a
// PeerSession so handlers can see map-server identity (wID +
// nation) once RW_RELAYSVR_REQ has run, and so PeerRegistry can
// drop the entry cleanly on disconnect.

#include "handlers/handlers.h"
#include "peer_session.h"
#include "world_session.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <cstdint>
#include <memory>

namespace tworldsvr {

struct WorldServerConfig
{
    std::uint16_t  port            = 0;
    std::uint32_t  max_connections = 256;
    HandlerContext ctx{};
};

class WorldServer
{
public:
    WorldServer(boost::asio::io_context& io, WorldServerConfig cfg);

    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const { return m_port; }

    // Test hook — drive a pre-built peer session through the
    // dispatch loop without going through the acceptor.
    boost::asio::awaitable<void> Drive(std::shared_ptr<PeerSession> peer);

    std::uint32_t LiveConnections() const { return m_live.load(); }

private:
    boost::asio::awaitable<void> HandleConnection(
        std::shared_ptr<PeerSession> peer);

    boost::asio::io_context&        m_io;
    boost::asio::ip::tcp::acceptor  m_acceptor;
    std::uint16_t                   m_port;
    WorldServerConfig               m_cfg;
    std::atomic<std::uint32_t>      m_live{0};
};

} // namespace tworldsvr
