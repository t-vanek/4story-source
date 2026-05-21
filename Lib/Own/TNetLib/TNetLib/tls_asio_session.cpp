// PCH-free (no stdafx.h) — Linux build doesn't drag winsock.h.

#include "tls_asio_session.h"
#include "tnetlib_proto_log.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <utility>

namespace tnetlib {

namespace {

constexpr std::size_t kRecvChunkBytes = 4096;

// Mirror of asio_session.cpp's send-queue capacity knob. Kept separate
// so TLS and plain sessions can be tuned independently if needed; for
// now they share the default. Reads g_tls_send_queue_capacity at
// session construction time.
std::atomic<std::size_t> g_tls_send_queue_capacity{256};

// Route TLS diagnostics through the same sink as AsioSession.
using detail::LogProto;

// Pull CN out of an X509 subject. Returns empty string if absent.
std::string ExtractCommonName(X509* cert)
{
    if (cert == nullptr) return {};
    X509_NAME* subj = X509_get_subject_name(cert);
    if (subj == nullptr) return {};
    char buf[256] = {0};
    int len = X509_NAME_get_text_by_NID(subj, NID_commonName, buf, sizeof(buf));
    if (len <= 0) return {};
    return std::string(buf, static_cast<std::size_t>(len));
}

} // namespace

TlsAsioSession::TlsAsioSession(boost::asio::ip::tcp::socket socket,
                               boost::asio::ssl::context&   ctx)
    : m_stream(std::move(socket), ctx)
    , m_recv_buffer(kRecvChunkBytes)
    , m_send_chan(m_stream.get_executor(),
                  g_tls_send_queue_capacity.load(std::memory_order_relaxed))
{
    boost::system::error_code ec;
    auto ep = m_stream.next_layer().remote_endpoint(ec);
    if (!ec && ep.address().is_v4())
    {
        m_remote_ipv4 = ep.address().to_v4().to_string();
    }
}

TlsAsioSession::~TlsAsioSession()
{
    Close();
}

boost::asio::awaitable<bool>
TlsAsioSession::Handshake(TlsRole role)
{
    boost::system::error_code ec;
    const auto type = (role == TlsRole::Server)
        ? boost::asio::ssl::stream_base::server
        : boost::asio::ssl::stream_base::client;

    co_await m_stream.async_handshake(type,
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));

    if (ec)
    {
        LogProto("tls: handshake failed (ip=%s, role=%s, err=%s)",
                    m_remote_ipv4.c_str(),
                    role == TlsRole::Server ? "server" : "client",
                    ec.message().c_str());
        Close();
        co_return false;
    }

    // Capture peer CN for downstream identity checks. OpenSSL hands
    // back an owning X509* via SSL_get_peer_certificate (must free);
    // we use SSL_get1_peer_certificate on OpenSSL 3 which is the
    // same shape. Either is fine to leave un-checked: peer cert can
    // legitimately be absent for some server-only TLS configs.
    SSL* ssl = m_stream.native_handle();
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    X509* cert = SSL_get1_peer_certificate(ssl);
#else
    X509* cert = SSL_get_peer_certificate(ssl);
#endif
    if (cert != nullptr)
    {
        m_peer_cn = ExtractCommonName(cert);
        X509_free(cert);
    }

    co_return true;
}

void TlsAsioSession::StartDrainIfNeeded()
{
    if (m_drain_started.exchange(true, std::memory_order_acq_rel))
        return;

    boost::asio::co_spawn(
        m_stream.get_executor(),
        [self = shared_from_this()]() -> boost::asio::awaitable<void> {
            co_await self->DrainSendQueue();
        },
        boost::asio::detached);
}

boost::asio::awaitable<void>
TlsAsioSession::Run(BytesHandler on_bytes)
{
    StartDrainIfNeeded();
    boost::system::error_code ec;
    while (!m_closed.load(std::memory_order_relaxed))
    {
        const auto n = co_await m_stream.async_read_some(
            boost::asio::buffer(m_recv_buffer.data(), m_recv_buffer.size()),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));

        if (ec || n == 0)
            break;

        if (on_bytes)
            on_bytes(std::span<const std::byte>(m_recv_buffer.data(), n));
    }
    Close();
}

boost::asio::awaitable<void>
TlsAsioSession::Send(std::span<const std::byte> bytes)
{
    if (m_closed.load(std::memory_order_relaxed) || bytes.empty())
        co_return;

    co_await boost::asio::async_write(
        m_stream,
        boost::asio::buffer(bytes.data(), bytes.size()),
        boost::asio::use_awaitable);
}

boost::asio::awaitable<void>
TlsAsioSession::RunPackets(PacketHandler on_packet)
{
    StartDrainIfNeeded();
    boost::system::error_code ec;

    while (!m_closed.load(std::memory_order_relaxed))
    {
        m_packet_buffer.resize(kPacketHeaderSize);

        co_await boost::asio::async_read(
            m_stream,
            boost::asio::buffer(m_packet_buffer.data(), kPacketHeaderSize),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) break;

        auto* hdr = reinterpret_cast<PacketHeader*>(m_packet_buffer.data());
        const std::uint16_t wSize = hdr->wSize;
        if (wSize < kPacketHeaderSize || wSize >= kMaxPacketSize)
            break;

        const std::size_t body_len = wSize - kPacketHeaderSize;

        if (body_len > 0)
        {
            m_packet_buffer.resize(wSize);
            hdr = reinterpret_cast<PacketHeader*>(m_packet_buffer.data());

            co_await boost::asio::async_read(
                m_stream,
                boost::asio::buffer(m_packet_buffer.data() + kPacketHeaderSize,
                                    body_len),
                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) break;
        }

        // No RC4 on TLS sessions — transport provides confidentiality.
        ++m_recv_sequence;
        const std::int64_t key = KeyForSequence(m_recv_sequence);
        DecryptHeader(hdr, key);

        if (hdr->dwNumber != m_recv_sequence)
        {
            LogProto("tls/recv: sequence mismatch (ip=%s, got=%u, want=%u, wId=0x%04X)",
                        m_remote_ipv4.c_str(),
                        hdr->dwNumber, m_recv_sequence,
                        static_cast<unsigned>(hdr->wId));
            break;
        }

        std::byte* body_ptr = m_packet_buffer.data() + kPacketHeaderSize;
        if (!DecryptBody(body_ptr, body_len, key, hdr->llChecksum))
        {
            LogProto("tls/recv: checksum mismatch (ip=%s, wId=0x%04X, dwNumber=%u, body=%u)",
                        m_remote_ipv4.c_str(),
                        static_cast<unsigned>(hdr->wId),
                        hdr->dwNumber,
                        static_cast<unsigned>(body_len));
            break;
        }

        if (on_packet)
        {
            DecodedPacket pkt;
            pkt.wId      = hdr->wId;
            pkt.dwNumber = hdr->dwNumber;
            pkt.body     = std::span<const std::byte>(body_ptr, body_len);
            on_packet(pkt);
        }
    }
    Close();
}

boost::asio::awaitable<void>
TlsAsioSession::SendPacket(std::uint16_t wId,
                           std::span<const std::byte> body)
{
    if (m_closed.load(std::memory_order_relaxed))
        co_return;

    if (body.size() >= kMaxPacketSize - kPacketHeaderSize)
    {
        LogProto("tls/send: oversized body (ip=%s, wId=0x%04X, body=%zu, max=%zu)",
                    m_remote_ipv4.c_str(),
                    static_cast<unsigned>(wId),
                    body.size(),
                    static_cast<std::size_t>(kMaxPacketSize - kPacketHeaderSize));
        co_return;
    }

    PendingSend pending;
    pending.wId = wId;
    pending.body.assign(body.begin(), body.end());

    boost::system::error_code ec;
    co_await m_send_chan.async_send(
        boost::system::error_code{},
        std::move(pending),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));

    if (ec)
    {
        LogProto("tls/send: enqueue dropped (ip=%s, wId=0x%04X, err=%s)",
                    m_remote_ipv4.c_str(),
                    static_cast<unsigned>(wId),
                    ec.message().c_str());
    }
}

boost::asio::awaitable<void>
TlsAsioSession::DoSendPacket(std::uint16_t wId,
                             std::span<const std::byte> body)
{
    if (m_closed.load(std::memory_order_relaxed))
        co_return;

    const std::size_t frame_size = kPacketHeaderSize + body.size();
    m_send_buffer.resize(frame_size);

    auto* hdr = reinterpret_cast<PacketHeader*>(m_send_buffer.data());
    hdr->wSize    = static_cast<std::uint16_t>(frame_size);
    hdr->wId      = wId;
    hdr->dwNumber = ++m_send_sequence;

    std::byte* body_dst = m_send_buffer.data() + kPacketHeaderSize;
    if (!body.empty())
        std::memcpy(body_dst, body.data(), body.size());

    const std::int64_t key = KeyForSequence(m_send_sequence);
    hdr->llChecksum = EncryptBody(body_dst, body.size(), key);
    EncryptHeader(hdr, key);

    // No RC4 layer on TLS — see RunPackets above.
    boost::system::error_code ec;
    co_await boost::asio::async_write(
        m_stream,
        boost::asio::buffer(m_send_buffer.data(), frame_size),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));

    if (ec)
    {
        LogProto("tls/send: write failed (ip=%s, wId=0x%04X, err=%s)",
                    m_remote_ipv4.c_str(),
                    static_cast<unsigned>(wId),
                    ec.message().c_str());
    }
}

boost::asio::awaitable<void>
TlsAsioSession::DrainSendQueue()
{
    boost::system::error_code ec;
    while (true)
    {
        PendingSend pending = co_await m_send_chan.async_receive(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));

        if (ec) break;

        co_await DoSendPacket(pending.wId,
            std::span<const std::byte>(pending.body.data(),
                                       pending.body.size()));

        if (m_closed.load(std::memory_order_relaxed))
            break;
    }
}

void TlsAsioSession::Close()
{
    // Idempotent. Channel close wakes both producers (suspended on
    // a full queue) and the drain (awaiting next item).
    m_send_chan.close();

    if (m_closed.exchange(true, std::memory_order_acq_rel))
        return;

    auto& sock = m_stream.next_layer();
    if (!sock.is_open())
        return;

    // Best-effort TLS close_notify. We don't co_await it here because
    // Close() is sync — many callers run from destructors or other
    // non-coroutine contexts. If the peer cares about a graceful
    // close, they'll see it on the next async read or have their own
    // shutdown protocol.
    boost::system::error_code ec;
    m_stream.shutdown(ec);
    sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    sock.close(ec);
}

} // namespace tnetlib
