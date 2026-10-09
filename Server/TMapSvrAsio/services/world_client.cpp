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
                                 std::chrono::milliseconds backoff_max,
                                 std::chrono::milliseconds registration_timeout)
    : m_io(io)
    , m_write_permit(std::make_shared<WritePermit>(io, 1))
    , m_registration_timeout(registration_timeout)
    , m_host(std::move(host))
    , m_port(port)
    , m_on_packet(std::move(on_packet))
    , m_backoff_initial(backoff_initial)
    , m_backoff_max(backoff_max)
{
    m_write_permit->try_send(boost::system::error_code{}, 0);
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

        m_registered = false;
        try { co_await ReadConnection(sock); }
        catch (...) { spdlog::error("world_client: connection handler failed; draining clients"); }
        // Close the admission gate before client teardown can suspend. No queued
        // sender or old World callback may target a replacement connection.
        m_registered = false;
        boost::system::error_code ec;
        sock->close(ec);
        m_session.reset();
        spdlog::info("world_client: disconnected from {}:{}; draining before reconnect", m_host, m_port);
        if (m_on_disconnect && !co_await m_on_disconnect()) co_return;

        // Brief pause before the next dial so a flapping peer doesn't
        // burn a CPU on tight reconnect spin.
        timer.expires_after(m_backoff_initial);
        co_await timer.async_wait(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) co_return;
    }
}

boost::asio::awaitable<void>
AsioWorldClient::ReadConnection(std::shared_ptr<boost::asio::ip::tcp::socket> sock)
{
    using boost::asio::buffer;
    auto deadline = std::make_shared<boost::asio::steady_timer>(m_io);
    if (m_relay_wid != 0) {
        deadline->expires_after(m_registration_timeout);
        deadline->async_wait([sock, deadline](auto ec) {
            if (!ec) {
                spdlog::warn("world_client: registration deadline expired; closing");
                boost::system::error_code ignored; sock->close(ignored);
            }
        });
    }
    // Always cancel even if an inbound handler throws.
    struct Cancel { std::shared_ptr<boost::asio::steady_timer> timer; ~Cancel() { timer->cancel(); } } cancel{deadline};
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
        if (m_relay_wid != 0 && !m_registered) {
            wire::Reader reader(body.data(), body.size());
            std::uint8_t nation{};
            std::uint16_t count{};
            bool valid = hdr.wID == static_cast<std::uint16_t>(
                tnetlib::protocol::MessageId::RW_RELAYSVR_ACK) && reader.Read(nation) && reader.Read(count);
            for (std::uint32_t i=0; valid && i<count; ++i) {
                std::uint32_t user{}; valid=reader.Read(user);
            }
            valid = valid && reader.Read(count);
            for (std::uint32_t i=0; valid && i<count; ++i) {
                std::string key, value; valid=reader.ReadString(key) && reader.ReadString(value);
            }
            if (!valid || !reader.Eof()) {
                spdlog::warn("world_client: invalid registration acknowledgment; closing");
                break;
            }
            m_registered = true;
            deadline->cancel();
            spdlog::info("world_client: registration acknowledged wid=0x{:04X}", m_relay_wid);
            continue;
        }
        if (m_on_packet)
            co_await m_on_packet(hdr.wID,
                std::span<const std::byte>(body.data(), body.size()));
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
            "(server_id={}, server_type={})",
            m_relay_wid, m_relay_wid & 0xFF, (m_relay_wid >> 8) & 0xFF);
    else
        spdlog::warn("world_client: RW_RELAYSVR_REQ wid=0x{:04X} not sent "
                     "(link dropped before register)", m_relay_wid);
}

boost::asio::awaitable<bool>
AsioWorldClient::SendPacket(std::uint16_t wId, std::vector<std::byte> body)
{
    auto sock = m_session;
    if (!sock || !sock->is_open()) co_return false;
    // A strand alone does not serialize composed async_write operations across
    // suspension. Hold this permit until the whole frame is written.
    co_await m_write_permit->async_receive(boost::asio::use_awaitable);
    struct Release {
        std::shared_ptr<WritePermit> permit;
        ~Release() { permit->try_send(boost::system::error_code{}, 0); }
    } release{m_write_permit};
    if (sock != m_session || !sock->is_open()) co_return false;
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
    boost::system::error_code ec;
    co_await boost::asio::async_write(*sock,
        boost::asio::buffer(frame.data(), frame.size()),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec)
    {
        spdlog::warn("world_client: SendPacket wId=0x{:04X} write failed: {}",
            wId, ec.message());
        if (sock == m_session) m_registered = false;
        boost::system::error_code ignored; sock->close(ignored);
        co_return false;
    }
    co_return sock == m_session;
}

} // namespace tmapsvr
