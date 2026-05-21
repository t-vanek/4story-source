#include "fourstory/cluster/peer_client.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstring>
#include <utility>

#ifdef _WIN32
#include <process.h>
#define FOURSTORY_GETPID() _getpid()
#else
#include <unistd.h>
#define FOURSTORY_GETPID() getpid()
#endif

namespace fourstory::cluster {

namespace {

// Message IDs — must match Lib/Own/TProtocol/include/MessageId.h.
// Embedded here (rather than #include MessageId.h) so this library
// stays standalone: TControlSvrAsio re-uses TProtocol but a peer
// server should be able to link only fourstory_common.
constexpr std::uint16_t kPeerRegisterReq    = 0x9F00;
constexpr std::uint16_t kPeerRegisterAck    = 0x9F01;
constexpr std::uint16_t kPeerHeartbeatReq   = 0x9F02;
constexpr std::uint16_t kPeerHeartbeatAck   = 0x9F03;
constexpr std::uint16_t kPeerDeregisterReq  = 0x9F04;

constexpr std::uint16_t kPacketHeaderSize = 8;

#pragma pack(push, 1)
struct PacketHeader
{
    std::uint16_t wSize;
    std::uint16_t wID;
    std::uint32_t dwChkSum;
};
#pragma pack(pop)
static_assert(sizeof(PacketHeader) == kPacketHeaderSize);

// Body checksum — running 32-bit XOR fold. Bit-for-bit identical to
// TControlSvrAsio::ComputeChecksum / TPatchSvrAsio::ComputeChecksum.
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

template <class T>
void WritePOD(std::vector<std::byte>& out, T v)
{
    const auto* p = reinterpret_cast<const std::byte*>(&v);
    out.insert(out.end(), p, p + sizeof(T));
}
void WriteString(std::vector<std::byte>& out, const std::string& s)
{
    const auto len = static_cast<std::int32_t>(s.size());
    WritePOD<std::int32_t>(out, len);
    if (!s.empty())
    {
        const auto* p = reinterpret_cast<const std::byte*>(s.data());
        out.insert(out.end(), p, p + s.size());
    }
}

class BodyReader
{
public:
    BodyReader(const std::byte* data, std::size_t size)
        : m_data(data), m_size(size) {}
    template <class T> bool Read(T& v)
    {
        if (m_off + sizeof(T) > m_size) return false;
        std::memcpy(&v, m_data + m_off, sizeof(T));
        m_off += sizeof(T);
        return true;
    }
private:
    const std::byte* m_data;
    std::size_t      m_size;
    std::size_t      m_off = 0;
};

} // namespace

PeerClient::PeerClient(boost::asio::io_context& io, PeerClientOptions opts)
    : m_io(io), m_opts(std::move(opts)), m_socket(io)
{
    // Auto-fill platform identifiers when the caller leaves them
    // at default — saves every peer server from duplicating the
    // getpid() / now() ceremony.
    if (m_opts.pid == 0)
        m_opts.pid = static_cast<std::uint32_t>(FOURSTORY_GETPID());
    if (m_opts.start_unix == 0)
        m_opts.start_unix =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
}

void PeerClient::Stop()
{
    m_stop.store(true);
    boost::system::error_code ec;
    m_socket.cancel(ec);
    m_socket.close(ec);
}

boost::asio::awaitable<bool>
PeerClient::SendOneFrame(std::uint16_t wId, std::vector<std::byte> body)
{
    const std::size_t total = kPacketHeaderSize + body.size();
    std::vector<std::byte> frame(total);
    PacketHeader hdr{};
    hdr.wSize    = static_cast<std::uint16_t>(total);
    hdr.wID      = wId;
    hdr.dwChkSum = FoldChecksum(body.data(), body.size());
    std::memcpy(frame.data(), &hdr, sizeof(hdr));
    if (!body.empty())
        std::memcpy(frame.data() + sizeof(hdr), body.data(), body.size());
    boost::system::error_code ec;
    co_await boost::asio::async_write(m_socket,
        boost::asio::buffer(frame.data(), total),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    co_return !ec;
}

boost::asio::awaitable<bool>
PeerClient::RecvOneFrame(std::uint16_t expected_wId,
                         std::vector<std::byte>& out_body)
{
    PacketHeader hdr{};
    boost::system::error_code ec;
    co_await boost::asio::async_read(m_socket,
        boost::asio::buffer(&hdr, sizeof(hdr)),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec) co_return false;
    if (hdr.wSize < sizeof(hdr)) co_return false;
    const std::size_t body_size = hdr.wSize - sizeof(hdr);
    out_body.assign(body_size, std::byte{});
    if (body_size > 0)
    {
        co_await boost::asio::async_read(m_socket,
            boost::asio::buffer(out_body.data(), body_size),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) co_return false;
        const auto expected_chk = FoldChecksum(out_body.data(), body_size);
        if (expected_chk != hdr.dwChkSum) co_return false;
    }
    co_return hdr.wID == expected_wId;
}

boost::asio::awaitable<bool> PeerClient::ConnectAndRegister()
{
    using boost::asio::ip::tcp;
    boost::system::error_code ec;

    // Fresh socket each attempt — the previous one may have been
    // closed by Stop() or by a heartbeat failure.
    m_socket = tcp::socket(m_io);
    tcp::resolver resolver(m_io);
    auto endpoints = co_await resolver.async_resolve(
        m_opts.control_host, std::to_string(m_opts.control_port),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec)
    {
        spdlog::warn("peer_client: resolve {}:{} failed — {}",
            m_opts.control_host, m_opts.control_port, ec.message());
        co_return false;
    }
    co_await boost::asio::async_connect(m_socket, endpoints,
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec)
    {
        spdlog::warn("peer_client: connect {}:{} failed — {}",
            m_opts.control_host, m_opts.control_port, ec.message());
        co_return false;
    }

    // Build CT_PEER_REGISTER_REQ body. Layout MUST match
    // handlers_registry.cpp::OnPeerRegisterReq.
    std::vector<std::byte> body;
    body.reserve(64);
    WritePOD<std::uint32_t>(body, m_opts.service_id);
    WriteString(body, m_opts.reported_name);
    WriteString(body, m_opts.reported_addr);
    WritePOD<std::uint16_t>(body, m_opts.reported_port);
    WriteString(body, m_opts.version);
    WritePOD<std::uint32_t>(body, m_opts.pid);
    WritePOD<std::uint64_t>(body,
        static_cast<std::uint64_t>(m_opts.start_unix));

    if (!co_await SendOneFrame(kPeerRegisterReq, std::move(body)))
    {
        spdlog::warn("peer_client: register-send failed");
        co_return false;
    }
    std::vector<std::byte> reply;
    if (!co_await RecvOneFrame(kPeerRegisterAck, reply) || reply.size() < 17)
    {
        spdlog::warn("peer_client: register-ack read failed / malformed");
        co_return false;
    }
    BodyReader r(reply.data(), reply.size());
    std::uint8_t  accepted = 0;
    std::uint32_t reason   = 0;
    std::uint64_t lease    = 0;
    std::uint32_t hb_int   = 30;
    r.Read(accepted); r.Read(reason); r.Read(lease); r.Read(hb_int);
    if (accepted != 1)
    {
        spdlog::warn("peer_client: register rejected — reason={} "
                     "service_id={:#x}", reason, m_opts.service_id);
        co_return false;
    }
    m_lease_epoch.store(lease);
    m_heartbeat_interval_sec.store(hb_int ? hb_int : std::uint32_t{30});
    m_registered.store(true);
    spdlog::info("peer_client: registered service_id={:#x} lease={} "
                 "heartbeat_interval={}s",
        m_opts.service_id, lease, hb_int);
    co_return true;
}

boost::asio::awaitable<void> PeerClient::HeartbeatLoop()
{
    boost::asio::steady_timer timer(m_io);
    while (!m_stop.load() && m_registered.load())
    {
        const auto interval = std::chrono::seconds(
            m_heartbeat_interval_sec.load());
        timer.expires_after(interval);
        boost::system::error_code ec;
        co_await timer.async_wait(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec || m_stop.load()) co_return;

        std::uint32_t cur = 0, max = 0;
        if (m_user_counts)
        {
            const auto pr = m_user_counts();
            cur = pr.first; max = pr.second;
        }
        std::vector<std::byte> body;
        body.reserve(20);
        WritePOD<std::uint32_t>(body, m_opts.service_id);
        WritePOD<std::uint64_t>(body, m_lease_epoch.load());
        WritePOD<std::uint32_t>(body, cur);
        WritePOD<std::uint32_t>(body, max);

        if (!co_await SendOneFrame(kPeerHeartbeatReq, std::move(body)))
        {
            spdlog::warn("peer_client: heartbeat send failed — reconnecting");
            m_registered.store(false);
            co_return;
        }
        std::vector<std::byte> reply;
        if (!co_await RecvOneFrame(kPeerHeartbeatAck, reply) || reply.size() < 9)
        {
            spdlog::warn("peer_client: heartbeat-ack read failed");
            m_registered.store(false);
            co_return;
        }
        BodyReader r(reply.data(), reply.size());
        std::uint8_t  accepted = 0;
        std::uint64_t echoed   = 0;
        r.Read(accepted); r.Read(echoed);
        if (accepted != 1)
        {
            spdlog::info("peer_client: heartbeat rejected by control "
                         "(stale lease) — will re-register");
            m_registered.store(false);
            co_return;
        }
    }
}

boost::asio::awaitable<void> PeerClient::Run()
{
    auto backoff = m_opts.initial_backoff;
    boost::asio::steady_timer timer(m_io);

    while (!m_stop.load())
    {
        const bool ok = co_await ConnectAndRegister();
        if (ok)
        {
            backoff = m_opts.initial_backoff;   // reset on success
            co_await HeartbeatLoop();
        }
        if (m_stop.load()) break;

        // Either ConnectAndRegister failed or HeartbeatLoop fell out.
        // Close the socket (idempotent) and sleep before retrying.
        boost::system::error_code ec;
        m_socket.close(ec);

        spdlog::info("peer_client: retrying in {}s",
            static_cast<long long>(backoff.count()));
        timer.expires_after(backoff);
        co_await timer.async_wait(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (m_stop.load()) break;

        // Exponential backoff capped at max_backoff.
        backoff = std::min(backoff * 2, m_opts.max_backoff);
    }

    // Graceful exit: if a lease is still held, send DEREGISTER so
    // TControl doesn't have to wait for the expiry sweep.
    if (m_registered.load())
    {
        std::vector<std::byte> body;
        WritePOD<std::uint32_t>(body, m_opts.service_id);
        WritePOD<std::uint64_t>(body, m_lease_epoch.load());
        co_await SendOneFrame(kPeerDeregisterReq, std::move(body));
        m_registered.store(false);
    }
    boost::system::error_code ec;
    m_socket.close(ec);
}

} // namespace fourstory::cluster
