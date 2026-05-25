#include "world_client.h"

#include "wire_codec.h"
#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <spdlog/spdlog.h>

#include <cstring>
#include <span>
#include <utility>

namespace tmapsvr {

namespace {

// 8-byte plain server-to-server frame, identical to TWorldSvr's
// WorldSession and TControlSvr's ControlSession: WORD wSize | WORD wID |
// DWORD dwChkSum, where wSize counts the header, dwChkSum is a 32-bit
// XOR-fold over the (plaintext) body. No sequence number, no header
// obfuscation, no RC4 — the cluster's SS convention.
#pragma pack(push, 1)
struct SsHeader
{
    std::uint16_t wSize;
    std::uint16_t wID;
    std::uint32_t dwChkSum;
};
#pragma pack(pop)
static_assert(sizeof(SsHeader) == 8, "SsHeader must be 8 bytes");

constexpr std::uint16_t kSsHeaderSize = 8;
constexpr std::uint16_t kSsMaxPacket  = 0xFFFF;

std::uint32_t FoldChecksum(const std::byte* p, std::size_t n)
{
    std::uint32_t acc = 0;
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4)
    {
        std::uint32_t w = 0;
        std::memcpy(&w, p + i, 4);
        acc ^= w;
    }
    for (; i < n; ++i)
        acc ^= static_cast<std::uint32_t>(p[i]);
    return acc;
}

} // namespace

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
    return m_session && m_session->is_open();
}

boost::asio::awaitable<std::shared_ptr<boost::asio::ip::tcp::socket>>
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

    auto sock = std::make_shared<tcp::socket>(m_io);
    co_await boost::asio::async_connect(*sock, endpoints,
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec)
    {
        spdlog::warn("world_client: connect {}:{} failed: {}",
            m_host, m_port, ec.message());
        co_return nullptr;
    }
    co_return sock;
}

boost::asio::awaitable<void>
AsioWorldClient::Run()
{
    using boost::asio::buffer;
    auto backoff = m_backoff_initial;
    boost::asio::steady_timer timer(m_io);

    while (true)
    {
        auto sock = co_await DialOnce();
        if (!sock)
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
        m_session = sock;
        backoff   = m_backoff_initial;

        // Identify this map to TWorld before reading. Until the
        // RW_RELAYSVR_REQ lands, TWorld holds the session anonymous
        // (wID=0) and won't route MW traffic back to us.
        co_await SendRegister();

        // Inbound read loop — 8-byte header + body, verify checksum,
        // hand the decoded (wId, body) to the installed handler.
        boost::system::error_code ec;
        while (sock->is_open())
        {
            SsHeader hdr{};
            co_await boost::asio::async_read(*sock,
                buffer(&hdr, sizeof(hdr)),
                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) break;
            if (hdr.wSize < kSsHeaderSize || hdr.wSize > kSsMaxPacket)
            {
                spdlog::warn("world_client: framing error wSize={} — closing",
                    hdr.wSize);
                break;
            }
            const std::size_t body_size = hdr.wSize - kSsHeaderSize;
            std::vector<std::byte> body(body_size);
            if (body_size > 0)
            {
                co_await boost::asio::async_read(*sock,
                    buffer(body.data(), body_size),
                    boost::asio::redirect_error(
                        boost::asio::use_awaitable, ec));
                if (ec) break;
            }
            const std::uint32_t expected =
                FoldChecksum(body.data(), body_size);
            if (expected != hdr.dwChkSum)
            {
                spdlog::warn("world_client: checksum mismatch wID=0x{:04X} "
                             "got=0x{:08X} expected=0x{:08X} — closing",
                    hdr.wID, hdr.dwChkSum, expected);
                break;
            }
            if (m_on_packet)
                m_on_packet(hdr.wID,
                    std::span<const std::byte>(body.data(), body.size()));
        }

        spdlog::info("world_client: disconnected from {}:{} — reconnecting",
            m_host, m_port);
        {
            boost::system::error_code ig;
            m_session->close(ig);
        }
        m_session.reset();

        // Brief pause before the next dial so a flapping peer doesn't
        // burn a CPU on tight reconnect spin.
        timer.expires_after(m_backoff_initial);
        co_await timer.async_wait(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) co_return;
    }
}

boost::asio::awaitable<void>
AsioWorldClient::SendRegister()
{
    if (m_relay_wid == 0)
    {
        spdlog::warn("world_client: relay wid not set — link stays "
                     "anonymous (TWorld won't route MW back)");
        co_return;
    }
    std::vector<std::byte> body;
    tmapsvr::wire::WritePOD<std::uint16_t>(body, m_relay_wid);
    const bool ok = co_await SendPacket(
        static_cast<std::uint16_t>(
            tnetlib::protocol::MessageId::RW_RELAYSVR_REQ),
        std::move(body));
    if (ok)
        spdlog::info("world_client: sent RW_RELAYSVR_REQ wid=0x{:04X} "
                     "(server_id={}, group_id={})",
            m_relay_wid, m_relay_wid & 0xFF, (m_relay_wid >> 8) & 0xFF);
    else
        spdlog::warn("world_client: RW_RELAYSVR_REQ wid=0x{:04X} not sent "
                     "(link dropped before register)", m_relay_wid);
}

boost::asio::awaitable<bool>
AsioWorldClient::SendPacket(std::uint16_t wId, std::vector<std::byte> body)
{
    // Hop to the send strand so concurrent SendPacket calls from
    // multiple handler coroutines serialize correctly on the socket.
    co_await boost::asio::dispatch(m_send_strand,
        boost::asio::use_awaitable);

    if (!IsConnected())
    {
        spdlog::warn("world_client: SendPacket wId=0x{:04X} dropped — "
                     "world peer not connected", wId);
        co_return false;
    }
    const std::size_t total = kSsHeaderSize + body.size();
    if (total > kSsMaxPacket)
    {
        spdlog::error("world_client: outbound packet too big ({})", total);
        co_return false;
    }

    std::vector<std::byte> frame(total);
    SsHeader hdr{};
    hdr.wSize    = static_cast<std::uint16_t>(total);
    hdr.wID      = wId;
    hdr.dwChkSum = FoldChecksum(body.data(), body.size());
    std::memcpy(frame.data(), &hdr, sizeof(hdr));
    if (!body.empty())
        std::memcpy(frame.data() + sizeof(hdr), body.data(), body.size());

    // Capture the session locally so a disconnect mid-send doesn't null
    // the member out from under us.
    auto sock = m_session;
    boost::system::error_code ec;
    co_await boost::asio::async_write(*sock,
        boost::asio::buffer(frame.data(), frame.size()),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec)
    {
        spdlog::warn("world_client: SendPacket wId=0x{:04X} write failed: {}",
            wId, ec.message());
        co_return false;
    }
    co_return true;
}

} // namespace tmapsvr
