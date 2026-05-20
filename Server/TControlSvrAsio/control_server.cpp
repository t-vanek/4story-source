#include "control_server.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

namespace tcontrolsvr {

ControlServer::ControlServer(boost::asio::io_context& io,
                             ControlServerConfig cfg)
    : m_io(io)
    , m_acceptor(io)
    , m_port(cfg.port)
    , m_cfg(std::move(cfg))
    , m_auto_start(m_cfg.auto_start)
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
ControlServer::Run()
{
    using boost::asio::ip::tcp;
    while (m_acceptor.is_open())
    {
        boost::system::error_code ec;
        tcp::socket sock = co_await m_acceptor.async_accept(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) break;
        auto sess = std::make_shared<ControlSession>(std::move(sock));
        spdlog::info("control_svr: accept from {}", sess->RemoteIPv4());
        boost::asio::co_spawn(m_io,
            HandleConnection(std::move(sess)),
            boost::asio::detached);
    }
}

boost::asio::awaitable<void>
ControlServer::Drive(std::shared_ptr<ControlSession> sess)
{
    co_await HandleConnection(std::move(sess));
}

boost::asio::awaitable<void>
ControlServer::HandleConnection(std::shared_ptr<ControlSession> sess)
{
    auto op = std::make_shared<OperatorSession>(sess);

    HandlerContext ctx{};
    ctx.auth       = m_cfg.auth;
    ctx.inventory  = m_cfg.inventory;
    ctx.operators  = &m_operators;
    ctx.auto_start = &m_auto_start;

    co_await sess->Run(
        [this, op, ctx](std::shared_ptr<ControlSession> /*s*/,
                        DecodedPacket pkt) -> boost::asio::awaitable<void>
        {
            co_await handlers::Dispatch(op, pkt.wId,
                                        std::move(pkt.body), ctx);
        });

    if (op->LoggedIn())
    {
        spdlog::info("control_svr: operator '{}' (seq={}) disconnected",
            op->UserId(), op->ManagerSeq());
    }
    m_operators.Unregister(op.get());
}

} // namespace tcontrolsvr
