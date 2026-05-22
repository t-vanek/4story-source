#pragma once

// WorldServer — accept loop + per-session coroutine wiring. W1
// accepts plain-TCP SS connections, gates on max_connections, and
// drives each peer through the dispatch stub. Per-peer state
// (svr_id, role, channels) and the outbound peer dialer arrive in
// W2+ once the per-feature handlers exist.

#include "handlers/handlers.h"
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
    std::uint16_t port            = 0;
    std::uint32_t max_connections = 256;
    HandlerContext ctx{};
};

class WorldServer
{
public:
    WorldServer(boost::asio::io_context& io, WorldServerConfig cfg);

    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const { return m_port; }

    // Test hook — drive a single pre-built session through the
    // dispatch loop without going through the acceptor. Useful for
    // tests that frame packets in-memory and want to skip TCP.
    boost::asio::awaitable<void> Drive(std::shared_ptr<WorldSession> sess);

    std::uint32_t LiveConnections() const { return m_live.load(); }

private:
    boost::asio::awaitable<void> HandleConnection(
        std::shared_ptr<WorldSession> sess);

    boost::asio::io_context&        m_io;
    boost::asio::ip::tcp::acceptor  m_acceptor;
    std::uint16_t                   m_port;
    WorldServerConfig               m_cfg;
    std::atomic<std::uint32_t>      m_live{0};
};

} // namespace tworldsvr
