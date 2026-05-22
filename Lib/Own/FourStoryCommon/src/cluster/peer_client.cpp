#include "fourstory/cluster/peer_client.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <variant>

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
constexpr std::uint16_t kPeerDiscoverReq    = 0x9F05;
constexpr std::uint16_t kPeerDiscoverAck    = 0x9F06;

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
    bool ReadString(std::string& out)
    {
        out.clear();
        std::int32_t len = 0;
        if (!Read(len)) return false;
        if (len < 0) return false;
        if (m_off + static_cast<std::size_t>(len) > m_size) return false;
        if (len > 0)
        {
            out.assign(reinterpret_cast<const char*>(m_data + m_off),
                       static_cast<std::size_t>(len));
            m_off += static_cast<std::size_t>(len);
        }
        return true;
    }
private:
    const std::byte* m_data;
    std::size_t      m_size;
    std::size_t      m_off = 0;
};

} // namespace

PeerClient::PeerClient(boost::asio::io_context& io, PeerClientOptions opts)
    : m_io(io)
    , m_opts(std::move(opts))
    , m_socket(MakeInitialSocket(m_io, m_opts.ssl_ctx))
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

// Static helper used by the ctor's initializer list. The variant's
// default constructor would pick the PlainSocket alternative; that's
// fine when TLS is off, but we want the TLS arm constructed up-front
// when ssl_ctx is non-null so a SendOneFrame called before the first
// ConnectAndRegister attempt hits the correct codepath.
PeerClient::SocketVariant
PeerClient::MakeInitialSocket(boost::asio::io_context& io,
                              boost::asio::ssl::context* ctx)
{
    if (ctx != nullptr)
        return SocketVariant(std::in_place_type<TlsStream>, io, *ctx);
    return SocketVariant(std::in_place_type<PlainSocket>, io);
}

PeerClient::PlainSocket& PeerClient::UnderlyingTcp()
{
    if (auto* tls = std::get_if<TlsStream>(&m_socket))
        return tls->next_layer();
    return std::get<PlainSocket>(m_socket);
}

void PeerClient::Stop()
{
    m_stop.store(true);
    boost::system::error_code ec;
    auto& sock = UnderlyingTcp();
    sock.cancel(ec);
    sock.close(ec);
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
    // Dispatch the write through whichever variant arm is active.
    // Boost.Asio's async_write picks up either AsyncStream just fine;
    // the variant only exists so the lifetime + connect/close paths
    // can target the underlying tcp::socket explicitly.
    if (auto* tls = std::get_if<TlsStream>(&m_socket))
    {
        co_await boost::asio::async_write(*tls,
            boost::asio::buffer(frame.data(), total),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    }
    else
    {
        co_await boost::asio::async_write(std::get<PlainSocket>(m_socket),
            boost::asio::buffer(frame.data(), total),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    }
    co_return !ec;
}

boost::asio::awaitable<bool>
PeerClient::RecvOneFrame(std::uint16_t expected_wId,
                         std::vector<std::byte>& out_body)
{
    PacketHeader hdr{};
    boost::system::error_code ec;

    auto read_into = [&](void* dst, std::size_t n) -> boost::asio::awaitable<bool> {
        if (auto* tls = std::get_if<TlsStream>(&m_socket))
        {
            co_await boost::asio::async_read(*tls,
                boost::asio::buffer(dst, n),
                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        }
        else
        {
            co_await boost::asio::async_read(std::get<PlainSocket>(m_socket),
                boost::asio::buffer(dst, n),
                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        }
        co_return !ec;
    };

    if (!co_await read_into(&hdr, sizeof(hdr))) co_return false;
    if (hdr.wSize < sizeof(hdr)) co_return false;
    const std::size_t body_size = hdr.wSize - sizeof(hdr);
    out_body.assign(body_size, std::byte{});
    if (body_size > 0)
    {
        if (!co_await read_into(out_body.data(), body_size)) co_return false;
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
    // closed by Stop() or by a heartbeat failure. Re-emplace into
    // the variant so the active arm matches m_opts.ssl_ctx.
    m_socket = MakeInitialSocket(m_io, m_opts.ssl_ctx);

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
    co_await boost::asio::async_connect(UnderlyingTcp(), endpoints,
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec)
    {
        spdlog::warn("peer_client: connect {}:{} failed — {}",
            m_opts.control_host, m_opts.control_port, ec.message());
        co_return false;
    }

    // Mutual TLS handshake if configured. Done before the registry
    // protocol so the very first byte on the wire is already inside
    // the encrypted channel.
    if (auto* tls = std::get_if<TlsStream>(&m_socket))
    {
        co_await tls->async_handshake(
            boost::asio::ssl::stream_base::client,
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec)
        {
            spdlog::warn("peer_client: TLS handshake to {}:{} failed — {}",
                m_opts.control_host, m_opts.control_port, ec.message());
            boost::system::error_code close_ec;
            UnderlyingTcp().close(close_ec);
            co_return false;
        }
        spdlog::info("peer_client: TLS handshake to {}:{} succeeded",
            m_opts.control_host, m_opts.control_port);
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
        // Close the underlying socket (idempotent); the variant arm
        // will be re-emplaced on the next ConnectAndRegister.
        boost::system::error_code ec;
        UnderlyingTcp().close(ec);

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
    UnderlyingTcp().close(ec);
}

// Discovery — run a transient request/reply against TControlSvr on a
// fresh socket. Kept separate from the registration socket (Run's
// state machine) so an in-flight Discover call can't race the
// heartbeat read loop, and so a Discover failure mid-call doesn't
// take down the lease.
//
// Transport mirrors ConnectAndRegister: TCP (or TLS when ssl_ctx is
// set), 8-byte header + body framing, same checksum. No registration
// is performed on this socket — the discover endpoint is a read-only
// lookup and intentionally not lease-gated.
boost::asio::awaitable<std::vector<PeerClient::DiscoveredPeer>>
PeerClient::Discover(std::uint8_t group_id, std::uint8_t type_id)
{
    using boost::asio::ip::tcp;
    std::vector<DiscoveredPeer> empty;

    // Build the transport on a stack-local variant so this call is
    // independent of m_socket (which Run() is actively reading from).
    SocketVariant transport = MakeInitialSocket(m_io, m_opts.ssl_ctx);
    auto& tcp_sock = std::holds_alternative<TlsStream>(transport)
        ? std::get<TlsStream>(transport).next_layer()
        : std::get<PlainSocket>(transport);

    boost::system::error_code ec;
    tcp::resolver resolver(m_io);
    auto endpoints = co_await resolver.async_resolve(
        m_opts.control_host, std::to_string(m_opts.control_port),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec) { spdlog::warn("peer_client/discover: resolve failed — {}",
        ec.message()); co_return empty; }

    co_await boost::asio::async_connect(tcp_sock, endpoints,
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec) { spdlog::warn("peer_client/discover: connect failed — {}",
        ec.message()); co_return empty; }

    if (auto* tls = std::get_if<TlsStream>(&transport))
    {
        co_await tls->async_handshake(
            boost::asio::ssl::stream_base::client,
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) { spdlog::warn("peer_client/discover: TLS failed — {}",
            ec.message()); co_return empty; }
    }

    // Build CT_PEER_DISCOVER_REQ body: BYTE group, BYTE type.
    std::vector<std::byte> body;
    body.reserve(2);
    WritePOD<std::uint8_t>(body, group_id);
    WritePOD<std::uint8_t>(body, type_id);

    // Send through whichever variant arm is active.
    const auto send_frame = [&](std::vector<std::byte> b)
        -> boost::asio::awaitable<bool>
    {
        const std::size_t total = kPacketHeaderSize + b.size();
        std::vector<std::byte> frame(total);
        PacketHeader hdr{};
        hdr.wSize    = static_cast<std::uint16_t>(total);
        hdr.wID      = kPeerDiscoverReq;
        hdr.dwChkSum = FoldChecksum(b.data(), b.size());
        std::memcpy(frame.data(), &hdr, sizeof(hdr));
        if (!b.empty())
            std::memcpy(frame.data() + sizeof(hdr), b.data(), b.size());
        boost::system::error_code wec;
        if (auto* tls = std::get_if<TlsStream>(&transport))
        {
            co_await boost::asio::async_write(*tls,
                boost::asio::buffer(frame.data(), total),
                boost::asio::redirect_error(boost::asio::use_awaitable, wec));
        }
        else
        {
            co_await boost::asio::async_write(std::get<PlainSocket>(transport),
                boost::asio::buffer(frame.data(), total),
                boost::asio::redirect_error(boost::asio::use_awaitable, wec));
        }
        co_return !wec;
    };

    if (!co_await send_frame(std::move(body)))
    {
        spdlog::warn("peer_client/discover: send failed");
        co_return empty;
    }

    // Read reply header + body.
    PacketHeader hdr{};
    const auto read_into = [&](void* dst, std::size_t n)
        -> boost::asio::awaitable<bool>
    {
        boost::system::error_code rec;
        if (auto* tls = std::get_if<TlsStream>(&transport))
        {
            co_await boost::asio::async_read(*tls,
                boost::asio::buffer(dst, n),
                boost::asio::redirect_error(boost::asio::use_awaitable, rec));
        }
        else
        {
            co_await boost::asio::async_read(std::get<PlainSocket>(transport),
                boost::asio::buffer(dst, n),
                boost::asio::redirect_error(boost::asio::use_awaitable, rec));
        }
        co_return !rec;
    };

    if (!co_await read_into(&hdr, sizeof(hdr)))
    {
        spdlog::warn("peer_client/discover: header read failed");
        co_return empty;
    }
    if (hdr.wID != kPeerDiscoverAck || hdr.wSize < sizeof(hdr))
    {
        spdlog::warn("peer_client/discover: unexpected reply wID={:#x} "
                     "size={}", hdr.wID, hdr.wSize);
        co_return empty;
    }
    const std::size_t body_size = hdr.wSize - sizeof(hdr);
    std::vector<std::byte> reply(body_size);
    if (body_size > 0 && !co_await read_into(reply.data(), body_size))
    {
        spdlog::warn("peer_client/discover: body read failed");
        co_return empty;
    }
    if (body_size > 0 && FoldChecksum(reply.data(), body_size) != hdr.dwChkSum)
    {
        spdlog::warn("peer_client/discover: checksum mismatch");
        co_return empty;
    }

    BodyReader r(reply.data(), reply.size());
    std::uint32_t reason = 0;
    std::uint16_t count  = 0;
    if (!r.Read(reason) || !r.Read(count))
    {
        spdlog::warn("peer_client/discover: malformed reply preamble");
        co_return empty;
    }
    if (reason != 0)
    {
        spdlog::warn("peer_client/discover: control rejected — reason={}",
            reason);
        co_return empty;
    }

    std::vector<DiscoveredPeer> out;
    out.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i)
    {
        DiscoveredPeer p{};
        if (!r.Read(p.service_id)        ||
            !r.ReadString(p.reported_name) ||
            !r.ReadString(p.reported_addr) ||
            !r.Read(p.reported_port))
        {
            spdlog::warn("peer_client/discover: malformed entry at i={}", i);
            co_return empty;
        }
        out.push_back(std::move(p));
    }
    co_return out;
}

} // namespace fourstory::cluster
