#include "patch_server.h"
#include "handlers.h"

#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>

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

    // Background safety-net sweep — legacy fires only on
    // CT_SERVICEMONITOR_ACK, but a deploy without a TControlSvr peer
    // would never sweep. Run a 60s tick independently.
    boost::asio::co_spawn(m_io, StaleClientSweepLoop(),
        boost::asio::detached);

    while (m_acceptor.is_open())
    {
        boost::system::error_code ec;
        tcp::socket sock = co_await m_acceptor.async_accept(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) break;
        auto sess = std::make_shared<PatchSession>(std::move(sock));
        Register(sess);
        boost::asio::co_spawn(m_io,
            HandleConnection(sess), boost::asio::detached);
    }
}

void PatchServer::Register(std::shared_ptr<PatchSession> session)
{
    std::lock_guard<std::mutex> lk(m_sessions_mtx);
    // Drop already-expired entries opportunistically so the vector
    // doesn't grow without bound under high connect/disconnect churn.
    m_sessions.erase(
        std::remove_if(m_sessions.begin(), m_sessions.end(),
            [](const std::weak_ptr<PatchSession>& w) { return w.expired(); }),
        m_sessions.end());
    m_sessions.emplace_back(std::move(session));
}

void PatchServer::Unregister(PatchSession* raw)
{
    std::lock_guard<std::mutex> lk(m_sessions_mtx);
    m_sessions.erase(
        std::remove_if(m_sessions.begin(), m_sessions.end(),
            [raw](const std::weak_ptr<PatchSession>& w)
            {
                auto sp = w.lock();
                return !sp || sp.get() == raw;
            }),
        m_sessions.end());
}

std::size_t PatchServer::SweepStaleClients(std::chrono::milliseconds max_age)
{
    // Snapshot the registry under the lock, then release before
    // calling Close() so the close path can take its own locks /
    // touch the session list without re-entering us.
    std::vector<std::shared_ptr<PatchSession>> victims;
    const auto now = std::chrono::steady_clock::now();
    {
        std::lock_guard<std::mutex> lk(m_sessions_mtx);
        victims.reserve(m_sessions.size());
        for (const auto& w : m_sessions)
        {
            auto sp = w.lock();
            if (!sp || !sp->IsOpen()) continue;
            if (sp->IsServerPeer()) continue;  // matches legacy SESSION_SERVER exemption
            if (now - sp->ConnectedAt() > max_age)
                victims.push_back(std::move(sp));
        }
    }
    for (const auto& v : victims)
    {
        spdlog::info("patch_server: stale client sweep closing {} "
                     "(age > {}ms)",
            v->RemoteIPv4(),
            static_cast<long long>(max_age.count()));
        v->Close();
    }
    return victims.size();
}

boost::asio::awaitable<void>
PatchServer::StaleClientSweepLoop()
{
    using namespace std::chrono_literals;
    boost::asio::steady_timer timer(m_io);
    while (m_acceptor.is_open())
    {
        timer.expires_after(60s);
        boost::system::error_code ec;
        co_await timer.async_wait(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) break;
        if (!m_acceptor.is_open()) break;
        SweepStaleClients(60s);
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
            ctx.server        = this;
            ctx.db_pool       = m_cfg.db_pool;

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

    Unregister(session.get());
    m_live_sessions.fetch_sub(1);
}

} // namespace tpatchsvr
