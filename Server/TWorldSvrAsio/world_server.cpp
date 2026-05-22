#include "world_server.h"

#include "peer_session.h"
#include "services/peer_registry.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

namespace tworldsvr {

WorldServer::WorldServer(boost::asio::io_context& io, WorldServerConfig cfg)
    : m_io(io)
    , m_acceptor(io)
    , m_port(cfg.port)
    , m_cfg(std::move(cfg))
{
    using boost::asio::ip::tcp;
    tcp::endpoint ep(tcp::v4(), m_port);
    m_acceptor.open(ep.protocol());
    m_acceptor.set_option(tcp::acceptor::reuse_address(true));
    m_acceptor.bind(ep);
    m_acceptor.listen();
    m_port = m_acceptor.local_endpoint().port();
}

boost::asio::awaitable<void>
WorldServer::Run()
{
    using boost::asio::ip::tcp;
    while (m_acceptor.is_open())
    {
        boost::system::error_code ec;
        tcp::socket sock = co_await m_acceptor.async_accept(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) break;

        if (m_cfg.max_connections > 0 &&
            m_live.load() >= m_cfg.max_connections)
        {
            boost::system::error_code ig;
            const auto ep = sock.remote_endpoint(ig);
            spdlog::warn("world_server: at capacity ({}); refusing {}",
                m_cfg.max_connections,
                ig ? std::string{"?"} : ep.address().to_string());
            sock.close(ig);
            continue;
        }

        auto wire = std::make_shared<WorldSession>(std::move(sock));
        auto peer = std::make_shared<PeerSession>(wire);
        spdlog::info("world_server: accept from {}", wire->RemoteIPv4());
        boost::asio::co_spawn(m_io,
            HandleConnection(std::move(peer)),
            boost::asio::detached);
    }
}

boost::asio::awaitable<void>
WorldServer::Drive(std::shared_ptr<PeerSession> peer)
{
    co_await HandleConnection(std::move(peer));
}

boost::asio::awaitable<void>
WorldServer::HandleConnection(std::shared_ptr<PeerSession> peer)
{
    m_live.fetch_add(1);
    const std::string ip = peer->Wire()->RemoteIPv4();
    try
    {
        const auto ctx = m_cfg.ctx;
        co_await peer->Wire()->Run(
            [peer, ctx](std::shared_ptr<WorldSession> /*s*/,
                        DecodedPacket pkt) -> boost::asio::awaitable<void>
            {
                co_await handlers::Dispatch(peer, pkt.wId,
                                            std::move(pkt.body), ctx);
            });
    }
    catch (const std::exception& ex)
    {
        spdlog::warn("world_server[{}]: session terminated by exception: {}",
            ip, ex.what());
    }

    // Drop the peer from PeerRegistry on exit. Safe to call with
    // wID=0 (Unregister returns nullptr without touching state) so
    // sessions that never made it past RW_RELAYSVR_REQ are cheap to
    // tear down.
    if (m_cfg.ctx.peers && peer->Wid() != 0)
    {
        if (m_cfg.ctx.peers->Unregister(peer->Wid()))
            spdlog::info("world_server: wID={} unregistered from peer registry",
                peer->Wid());
    }

    m_live.fetch_sub(1);
    spdlog::info("world_server: peer {} disconnected", ip);
}

} // namespace tworldsvr
