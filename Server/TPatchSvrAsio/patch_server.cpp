#include "patch_server.h"
#include "handlers.h"

#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

namespace tpatchsvr {

PatchServer::PatchServer(boost::asio::io_context& io, PatchServerConfig config)
    : m_io(io)
    , m_acceptor(io)
    , m_port(config.port)
    , m_cfg(std::move(config))
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
PatchServer::Run()
{
    using boost::asio::ip::tcp;
    while (m_acceptor.is_open())
    {
        boost::system::error_code ec;
        tcp::socket sock = co_await m_acceptor.async_accept(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) break;
        auto sess = std::make_shared<PatchSession>(std::move(sock));
        boost::asio::co_spawn(m_io,
            HandleConnection(sess), boost::asio::detached);
    }
}

boost::asio::awaitable<void>
PatchServer::HandleConnection(std::shared_ptr<PatchSession> session)
{
    m_live_sessions.fetch_add(1);
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToMessageId;

    co_await session->Run(
        [this](std::shared_ptr<PatchSession> sess, DecodedPacket pkt)
            -> boost::asio::awaitable<void>
        {
            handlers::ServerContext ctx{};
            ctx.repo          = m_cfg.repo;
            ctx.ftp_url       = m_cfg.ftp_url;
            ctx.pre_ftp_url   = m_cfg.pre_ftp_url;
            ctx.login_host    = m_cfg.login_host;
            ctx.login_port    = m_cfg.login_port;
            ctx.session_count = m_live_sessions.load();

            const auto id = ToMessageId(pkt.wId);
            switch (id)
            {
            case MessageId::CT_SERVICEMONITOR_ACK:
                co_await handlers::OnServiceMonitor(sess, std::move(pkt.body), ctx);
                break;
            case MessageId::CT_SERVICEDATACLEAR_ACK:
                co_await handlers::OnServiceDataClear(sess, std::move(pkt.body));
                break;
            case MessageId::CT_PATCH_REQ:
                co_await handlers::OnPatch(sess, std::move(pkt.body), ctx);
                break;
            case MessageId::CT_NEWPATCH_REQ:
                co_await handlers::OnNewPatch(sess, std::move(pkt.body), ctx);
                break;
            case MessageId::CT_CHANGEIF_REQ:
                co_await handlers::OnChangeInterface(sess, std::move(pkt.body), ctx);
                break;
            case MessageId::CT_PREPATCH_REQ:
                co_await handlers::OnPrePatch(sess, std::move(pkt.body), ctx);
                break;
            case MessageId::CT_PATCHSTART_REQ:
                co_await handlers::OnPatchStart(sess, std::move(pkt.body));
                break;
            case MessageId::CT_CTRLSVR_REQ:
                co_await handlers::OnCtrlSvr(sess, std::move(pkt.body));
                break;
            case MessageId::CT_PREPATCHCOMPLETE_REQ:
                co_await handlers::OnPrePatchComplete(sess, std::move(pkt.body), ctx);
                break;
            default:
                spdlog::warn("unhandled patch packet id=0x{:04X} body={} bytes",
                    pkt.wId, pkt.body.size());
                break;
            }
        });

    m_live_sessions.fetch_sub(1);
}

} // namespace tpatchsvr
