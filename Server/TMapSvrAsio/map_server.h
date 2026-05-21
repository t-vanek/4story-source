#pragma once

// TMapSvrAsio top-level server. Owns the TCP acceptor, spins up an
// AsioSession per inbound connection, and dispatches each decoded
// packet to handlers.cpp (added in F4).
//
// Modeled on tloginsvr::LoginServer + tpatchsvr::PatchServer. The
// session-registry pattern (weak_ptr vector + sweep) is copied
// directly from PatchServer so the F17 control / sweep paths drop
// into the same shape later.

#include "asio_session.h"
#include "handlers.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace tmapsvr {

struct MapServerConfig
{
    std::uint16_t           port = 5815;          // DEF_MAPPORT
    // RC4 key shared with the legacy client; empty disables crypto
    // (dev / netcat smoke tests run plain wire). Same convention as
    // tloginsvr::LoginServerConfig::rc4_secret_key.
    std::vector<std::byte>  rc4_secret_key;
    std::uint32_t           max_connections = 8000;

    // Per-packet handler context — owns no state, just non-owning
    // pointers to services (validator, …) main built. Copied into
    // MapServer at construction; the caller's struct lifetime must
    // dominate the io_context's run().
    HandlerContext          handlers;
};

class MapServer
{
public:
    MapServer(boost::asio::io_context& io, MapServerConfig config);

    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const { return m_port; }

    // Session-registry hooks. Public so F17 / tests can drive the
    // sweep without standing up the full accept loop.
    void Register(std::shared_ptr<tnetlib::AsioSession> session);
    void Unregister(tnetlib::AsioSession* raw);

    std::uint32_t LiveSessions() const { return m_active_connections.load(); }

private:
    boost::asio::awaitable<void> HandleConnection(
        std::shared_ptr<tnetlib::AsioSession> session);

    boost::asio::io_context&        m_io;
    boost::asio::ip::tcp::acceptor  m_acceptor;
    std::uint16_t                   m_port;
    MapServerConfig                 m_cfg;
    std::atomic<std::uint32_t>      m_active_connections{0};

    std::mutex                                            m_sessions_mtx;
    std::vector<std::weak_ptr<tnetlib::AsioSession>>      m_sessions;
};

} // namespace tmapsvr
