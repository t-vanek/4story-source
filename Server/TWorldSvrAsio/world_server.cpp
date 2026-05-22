#include "world_server.h"

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

        // Hard cap before the session shared_ptr exists. Refusing
        // early keeps the pre-auth surface narrow and matches
        // TLoginSvrAsio's gate.
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

        auto sess = std::make_shared<WorldSession>(std::move(sock));
        spdlog::info("world_server: accept from {}", sess->RemoteIPv4());
        boost::asio::co_spawn(m_io,
            HandleConnection(std::move(sess)),
            boost::asio::detached);
    }
}

boost::asio::awaitable<void>
WorldServer::Drive(std::shared_ptr<WorldSession> sess)
{
    co_await HandleConnection(std::move(sess));
}

boost::asio::awaitable<void>
WorldServer::HandleConnection(std::shared_ptr<WorldSession> sess)
{
    m_live.fetch_add(1);
    const std::string ip = sess->RemoteIPv4();
    try
    {
        const auto ctx = m_cfg.ctx;
        co_await sess->Run(
            [ctx](std::shared_ptr<WorldSession> s, DecodedPacket pkt)
                -> boost::asio::awaitable<void>
            {
                co_await handlers::Dispatch(std::move(s), pkt.wId,
                                            std::move(pkt.body), ctx);
            });
    }
    catch (const std::exception& ex)
    {
        spdlog::warn("world_server[{}]: session terminated by exception: {}",
            ip, ex.what());
    }
    m_live.fetch_sub(1);
    spdlog::info("world_server: peer {} disconnected", ip);
}

} // namespace tworldsvr
