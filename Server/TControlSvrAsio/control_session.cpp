#include "control_session.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <spdlog/spdlog.h>

#include <cstring>
#include <utility>

namespace tcontrolsvr {

namespace {
// Identical to TPatchSvrAsio's FoldChecksum — sum body bytes as
// little-endian DWORDs running XOR; the unaligned tail is folded one
// byte at a time into the low byte of the accumulator. See
// Lib/Own/TNetLib/TNetLib/Packet.cpp for the legacy primitive.
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

std::uint32_t ComputeChecksum(const std::byte* body, std::size_t len)
{
    return FoldChecksum(body, len);
}

namespace {
// Extract the X509 Common Name field from an SSL session's peer
// certificate. Returns empty when the peer didn't present a cert or
// the cert's subject has no CN. Caller must have already driven
// async_handshake to completion — calling on an in-progress handshake
// is undefined.
std::string ExtractPeerCommonName(SSL* ssl)
{
    if (ssl == nullptr) return {};
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    X509* cert = SSL_get1_peer_certificate(ssl);
#else
    X509* cert = SSL_get_peer_certificate(ssl);
#endif
    if (cert == nullptr) return {};
    std::string out;
    X509_NAME* subj = X509_get_subject_name(cert);
    if (subj != nullptr)
    {
        char buf[256] = {0};
        const int len = X509_NAME_get_text_by_NID(subj, NID_commonName,
                                                   buf, sizeof(buf));
        if (len > 0)
            out.assign(buf, static_cast<std::size_t>(len));
    }
    X509_free(cert);
    return out;
}
} // namespace

ControlSession::ControlSession(PlainSocket sock)
    : m_socket(std::in_place_type<PlainSocket>, std::move(sock))
{
    boost::system::error_code ec;
    auto ep = UnderlyingTcp().remote_endpoint(ec);
    if (!ec && ep.address().is_v4())
        m_remote_ipv4 = ep.address().to_v4().to_string();
}

ControlSession::ControlSession(TlsStream stream)
    : m_socket(std::in_place_type<TlsStream>, std::move(stream))
{
    boost::system::error_code ec;
    auto ep = UnderlyingTcp().remote_endpoint(ec);
    if (!ec && ep.address().is_v4())
        m_remote_ipv4 = ep.address().to_v4().to_string();

    // Capture the peer CN now — the ssl::stream is fully handshaked
    // by the time the caller hands it to this ctor (ControlServer
    // drives async_handshake then constructs us), so SSL_get_peer_cert
    // will return the cert the peer presented during the handshake.
    m_peer_cn = ExtractPeerCommonName(
        std::get<TlsStream>(m_socket).native_handle());
}

ControlSession::PlainSocket& ControlSession::UnderlyingTcp()
{
    if (auto* tls = std::get_if<TlsStream>(&m_socket))
        return tls->next_layer();
    return std::get<PlainSocket>(m_socket);
}

const ControlSession::PlainSocket& ControlSession::UnderlyingTcp() const
{
    if (auto* tls = std::get_if<TlsStream>(&m_socket))
        return tls->next_layer();
    return std::get<PlainSocket>(m_socket);
}

bool ControlSession::IsOpen() const
{
    return UnderlyingTcp().is_open();
}

void ControlSession::TouchRecv()
{
    m_last_recv_at.store(
        std::chrono::steady_clock::now().time_since_epoch().count());
}

boost::asio::awaitable<void>
ControlSession::Run(PacketHandler on_packet)
{
    using namespace boost::asio;

    // Dispatch a single async_read through whichever variant arm is
    // active. boost::asio::async_read accepts any AsyncReadStream;
    // the variant only exists so connect/close target the raw layer.
    auto read_into = [&](void* dst, std::size_t n,
                          boost::system::error_code& ec)
        -> boost::asio::awaitable<bool> {
        if (auto* tls = std::get_if<TlsStream>(&m_socket))
        {
            co_await async_read(*tls, buffer(dst, n),
                redirect_error(use_awaitable, ec));
        }
        else
        {
            co_await async_read(std::get<PlainSocket>(m_socket),
                buffer(dst, n),
                redirect_error(use_awaitable, ec));
        }
        co_return !ec;
    };

    while (UnderlyingTcp().is_open())
    {
        PacketHeader hdr{};
        boost::system::error_code ec;
        if (!co_await read_into(&hdr, sizeof(hdr), ec)) break;
        if (hdr.wSize < kPacketHeaderSize || hdr.wSize > kMaxPacketSize)
        {
            spdlog::warn("control_session[{}]: framing error wSize={} — closing",
                m_remote_ipv4, hdr.wSize);
            break;
        }
        const std::size_t body_size = hdr.wSize - kPacketHeaderSize;
        std::vector<std::byte> body(body_size);
        if (body_size > 0)
        {
            if (!co_await read_into(body.data(), body_size, ec)) break;
        }
        const std::uint32_t expected = ComputeChecksum(body.data(), body_size);
        if (expected != hdr.dwChkSum)
        {
            spdlog::warn("control_session[{}]: checksum mismatch wID=0x{:04X} "
                         "got=0x{:08X} expected=0x{:08X} — closing",
                m_remote_ipv4, hdr.wID, hdr.dwChkSum, expected);
            break;
        }
        TouchRecv();
        DecodedPacket pkt{hdr.wID, std::move(body)};
        co_await on_packet(shared_from_this(), std::move(pkt));
    }
}

boost::asio::awaitable<void>
ControlSession::SendPacket(std::uint16_t wId, std::vector<std::byte> body)
{
    if (!IsOpen()) co_return;
    const std::size_t total = kPacketHeaderSize + body.size();
    if (total > kMaxPacketSize)
    {
        spdlog::error("control_session[{}]: outbound packet too big ({})",
            m_remote_ipv4, total);
        co_return;
    }
    m_send_scratch.resize(total);
    PacketHeader hdr{};
    hdr.wSize    = static_cast<std::uint16_t>(total);
    hdr.wID      = wId;
    hdr.dwChkSum = ComputeChecksum(body.data(), body.size());
    std::memcpy(m_send_scratch.data(), &hdr, sizeof(hdr));
    if (!body.empty())
        std::memcpy(m_send_scratch.data() + sizeof(hdr),
                    body.data(), body.size());
    boost::system::error_code ec;
    // Dispatch the write through the active variant arm.
    if (auto* tls = std::get_if<TlsStream>(&m_socket))
    {
        co_await boost::asio::async_write(*tls,
            boost::asio::buffer(m_send_scratch.data(), total),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    }
    else
    {
        co_await boost::asio::async_write(
            std::get<PlainSocket>(m_socket),
            boost::asio::buffer(m_send_scratch.data(), total),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    }
    if (ec)
        spdlog::debug("control_session[{}]: send wID=0x{:04X} failed: {}",
            m_remote_ipv4, wId, ec.message());
}

void ControlSession::Close()
{
    boost::system::error_code ec;
    auto& sock = UnderlyingTcp();
    sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    sock.close(ec);
}

} // namespace tcontrolsvr
