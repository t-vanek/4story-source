#include "world_client.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

#include <utility>

namespace tmapsvr {

AsioWorldClient::AsioWorldClient(boost::asio::io_context& io,
                                 std::string host,
                                 std::uint16_t port,
                                 InboundHandler on_packet,
                                 std::chrono::milliseconds backoff_initial,
                                 std::chrono::milliseconds backoff_max)
    : m_io(io)
    , m_send_strand(boost::asio::make_strand(io.get_executor()))
    , m_host(std::move(host))
    , m_port(port)
    , m_on_packet(std::move(on_packet))
    , m_backoff_initial(backoff_initial)
    , m_backoff_max(backoff_max)
{
}

bool AsioWorldClient::IsConnected() const
{
    return m_session && m_session->Socket().is_open();
}

boost::asio::awaitable<std::shared_ptr<tnetlib::AsioSession>>
AsioWorldClient::DialOnce()
{
    using boost::asio::ip::tcp;

    tcp::resolver resolver(m_io);
    boost::system::error_code ec;
    auto endpoints = co_await resolver.async_resolve(
        m_host, std::to_string(m_port),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec)
    {
        spdlog::warn("world_client: resolve {}:{} failed: {}",
            m_host, m_port, ec.message());
        co_return nullptr;
    }

    tcp::socket sock(m_io);
    co_await boost::asio::async_connect(sock, endpoints,
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec)
    {
        spdlog::warn("world_client: connect {}:{} failed: {}",
            m_host, m_port, ec.message());
        co_return nullptr;
    }

    // Server-to-server peer — no RC4, AsioSession's default XOR
    // header/body codec is enough.
    co_return std::make_shared<tnetlib::AsioSession>(
        std::move(sock), tnetlib::PeerType::Server);
}

boost::asio::awaitable<void>
AsioWorldClient::Run()
{
    auto backoff = m_backoff_initial;
    boost::asio::steady_timer timer(m_io);

    while (true)
    {
        auto sess = co_await DialOnce();
        if (!sess)
        {
            spdlog::info("world_client: backing off {}ms before retry",
                static_cast<long long>(backoff.count()));
            timer.expires_after(backoff);
            boost::system::error_code ec;
            co_await timer.async_wait(
                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) co_return; // executor stopping
            backoff = std::min(backoff * 2, m_backoff_max);
            continue;
        }

        spdlog::info("world_client: connected to {}:{}", m_host, m_port);
        m_session = sess;
        backoff   = m_backoff_initial;

        // Inbound dispatch — if the caller supplied a handler (F6+
        // wires handlers_world::DispatchWorld here via main()), invoke
        // it with the decoded body. Without a handler we log + drop,
        // which is what F5 did before this commit.
        co_await sess->RunPackets(
            [this](const tnetlib::DecodedPacket& pkt) {
                if (m_on_packet)
                {
                    m_on_packet(pkt.wId, pkt.body);
                }
                else
                {
                    spdlog::debug("world_client: rx wId=0x{:04X} seq={} body={} bytes "
                                  "(no inbound handler installed)",
                        pkt.wId, pkt.dwNumber, pkt.body.size());
                }
            });

        spdlog::info("world_client: disconnected from {}:{} — reconnecting",
            m_host, m_port);
        m_session.reset();

        // Brief pause before the next dial so a flapping peer doesn't
        // burn a CPU on tight reconnect spin.
        timer.expires_after(m_backoff_initial);
        boost::system::error_code ec;
        co_await timer.async_wait(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) co_return;
    }
}

boost::asio::awaitable<bool>
AsioWorldClient::SendPacket(std::uint16_t wId, std::vector<std::byte> body)
{
    // Hop to the send strand so concurrent SendPacket calls from
    // multiple handler coroutines (potentially on multiple threads)
    // serialize correctly. AsioSession::SendPacket is documented as
    // not-thread-safe; the strand makes it so. Dispatch returns
    // synchronously when we're already on the strand (single-thread
    // io.run()), so the cost is one queue hop in the multi-thread
    // case and zero in the common case.
    co_await boost::asio::dispatch(m_send_strand,
        boost::asio::use_awaitable);

    if (!IsConnected())
    {
        spdlog::warn("world_client: SendPacket wId=0x{:04X} dropped — "
                     "world peer not connected",
            wId);
        co_return false;
    }
    // Capture the session locally so a disconnect mid-send doesn't
    // null the member out from under us. m_session is a shared_ptr
    // exactly so we can do this.
    auto sess = m_session;
    co_await sess->SendPacket(
        wId, std::span<const std::byte>(body.data(), body.size()));
    co_return true;
}

} // namespace tmapsvr
