#include "map_server.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <exception>
#include <utility>

namespace tmapsvr {

MapServer::MapServer(boost::asio::io_context& io, MapServerConfig config)
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
MapServer::Run()
{
    using boost::asio::ip::tcp;

    while (m_acceptor.is_open())
    {
        boost::system::error_code ec;
        tcp::socket sock = co_await m_acceptor.async_accept(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) break;

        // max_connections gate — drop the new accept rather than queueing
        // it so the client gets an immediate RST and can retry against
        // another channel instead of waiting on a half-open socket.
        const auto current = m_active_connections.load(std::memory_order_relaxed);
        if (current >= m_cfg.max_connections)
        {
            boost::system::error_code peer_ec;
            const auto peer = sock.remote_endpoint(peer_ec);
            spdlog::warn("map_server: max_connections cap reached ({} >= {}); "
                         "dropping new accept from {}",
                current, m_cfg.max_connections,
                peer_ec ? std::string{"<unknown>"} : peer.address().to_string());
            boost::system::error_code ignored;
            sock.close(ignored);
            continue;
        }

        const auto peer = m_cfg.rc4_secret_key.empty()
            ? tnetlib::PeerType::Server
            : tnetlib::PeerType::Client;
        auto sess = std::make_shared<tnetlib::AsioSession>(
            std::move(sock), peer);

        // Legacy convention (matches CSession::Decrypt RC4 path /
        // CSession::Encrypt XOR-only path): inbound RC4 on, outbound
        // XOR-only. Toggle off entirely for plain-wire test runs.
        if (!m_cfg.rc4_secret_key.empty())
            sess->EnableInboundRC4(m_cfg.rc4_secret_key);

        Register(sess);
        m_active_connections.fetch_add(1, std::memory_order_relaxed);

        // Absorb any exception that escapes the per-connection coroutine
        // so co_spawn's detached-rethrow doesn't terminate the whole
        // io_context.
        boost::asio::co_spawn(
            m_io,
            HandleConnection(sess),
            [this, sess](std::exception_ptr ep) {
                if (ep)
                {
                    try { std::rethrow_exception(ep); }
                    catch (const std::exception& ex) {
                        spdlog::error("map_server: connection coroutine threw: {}",
                            ex.what());
                    }
                }
                m_active_connections.fetch_sub(1, std::memory_order_relaxed);
                Unregister(sess.get());
            });
    }
}

void MapServer::Register(std::shared_ptr<tnetlib::AsioSession> session)
{
    std::lock_guard<std::mutex> lk(m_sessions_mtx);
    m_sessions.erase(
        std::remove_if(m_sessions.begin(), m_sessions.end(),
            [](const std::weak_ptr<tnetlib::AsioSession>& w) { return w.expired(); }),
        m_sessions.end());
    m_sessions.emplace_back(std::move(session));
}

void MapServer::Unregister(tnetlib::AsioSession* raw)
{
    std::lock_guard<std::mutex> lk(m_sessions_mtx);
    m_sessions.erase(
        std::remove_if(m_sessions.begin(), m_sessions.end(),
            [raw](const std::weak_ptr<tnetlib::AsioSession>& w)
            {
                auto sp = w.lock();
                return !sp || sp.get() == raw;
            }),
        m_sessions.end());
}

boost::asio::awaitable<void>
MapServer::HandleConnection(std::shared_ptr<tnetlib::AsioSession> session)
{
    // F3 stub dispatch: log every received packet ID and drop it.
    // F4 will replace this lambda with the real switch over
    // MessageId values (CS_CONNECT_REQ, CS_VERIFYSESSION_REQ, …).
    co_await session->RunPackets(
        [](const tnetlib::DecodedPacket& pkt) {
            spdlog::debug("map_server: rx wId=0x{:04X} seq={} body={} bytes "
                          "(unhandled — F3 stub)",
                pkt.wId, pkt.dwNumber, pkt.body.size());
        });
}

} // namespace tmapsvr
