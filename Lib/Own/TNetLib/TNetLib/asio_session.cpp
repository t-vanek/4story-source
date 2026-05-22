// PCH-free (no stdafx.h) — compiles on Linux without TNetLib.h → winsock2.h.
// MSVC vcxproj marks this file with PrecompiledHeader=NotUsing.

#include "asio_session.h"
#include "tnetlib_crypto.h"
#include "tnetlib_proto_log.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <utility>

namespace tnetlib {

namespace {
// Reasonable starting buffer for a single async_read_some. Packets
// rarely exceed a few KB on this wire protocol; this just bounds
// per-recv memory.
constexpr std::size_t kRecvChunkBytes = 4096;

// Send-queue capacity per session. 256 in-flight packets per peer is
// well above the legacy wire protocol's burst patterns (handler
// replies are bounded by the per-request payload) and gives roughly
// 16 KB-worth of buffered PendingSend metadata per session at typical
// body sizes. Process-wide knob via SetSendQueueCapacity.
std::atomic<std::size_t> g_send_queue_capacity{256};

void DefaultErrorLogger(std::string_view msg) noexcept
{
    std::fprintf(stderr, "[tnetlib] %.*s\n",
                 static_cast<int>(msg.size()), msg.data());
}
} // namespace

namespace detail {
// Shared sink — declared in tnetlib_proto_log.h. Both AsioSession and
// TlsAsioSession route diagnostics through it.
std::atomic<AsioSession::ErrorLogger> g_proto_logger{&DefaultErrorLogger};

void LogProto(const char* fmt, ...) noexcept
{
    auto* fn = g_proto_logger.load(std::memory_order_relaxed);
    if (fn == nullptr) return;
    char buf[256];
    std::va_list ap;
    va_start(ap, fmt);
    const int n = std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    const std::size_t len = std::min(static_cast<std::size_t>(n),
                                     sizeof(buf) - 1);
    fn(std::string_view(buf, len));
}
} // namespace detail

namespace {
// Bring the shared LogProto into this TU's anonymous namespace so the
// rest of this file can call it unqualified, matching the original
// shape from before the TlsAsioSession split.
using detail::LogProto;
} // namespace

void AsioSession::SetErrorLogger(ErrorLogger logger) noexcept
{
    detail::g_proto_logger.store(logger, std::memory_order_relaxed);
}

void AsioSession::SetSendQueueCapacity(std::size_t n) noexcept
{
    // 0 would deadlock SendPacket on the first call; clamp to 1.
    g_send_queue_capacity.store(n == 0 ? 1 : n,
                                std::memory_order_relaxed);
}

// ===== AsioSession ==========================================================

AsioSession::AsioSession(boost::asio::ip::tcp::socket socket, PeerType type)
    : m_socket(std::move(socket))
    , m_type(type)
    , m_recv_buffer(kRecvChunkBytes)
    , m_send_chan(m_socket.get_executor(),
                  g_send_queue_capacity.load(std::memory_order_relaxed))
{
    // Capture peer IPv4 once at construction. remote_endpoint() throws
    // on a closed / unconnected socket; the auth path (IP banlist,
    // audit log) needs the address even if the socket is gone by the
    // time it's read.
    boost::system::error_code ec;
    auto ep = m_socket.remote_endpoint(ec);
    if (!ec && ep.address().is_v4())
    {
        m_remote_ipv4 = ep.address().to_v4().to_string();
    }
}

AsioSession::~AsioSession()
{
    Close();
}

void AsioSession::StartDrainIfNeeded()
{
    // exchange returns the prior value — we spawn iff we flipped false→true.
    if (m_drain_started.exchange(true, std::memory_order_acq_rel))
        return;

    // The drain coroutine holds the session alive through shared_from_this
    // so it can outlive whichever caller spawned us (typical pattern: Run
    // or RunPackets exits when the peer closes, but a few last queued
    // sends may still be in flight on the channel).
    boost::asio::co_spawn(
        m_socket.get_executor(),
        [self = shared_from_this()]() -> boost::asio::awaitable<void> {
            co_await self->DrainSendQueue();
        },
        boost::asio::detached);
}

boost::asio::awaitable<void>
AsioSession::Run(BytesHandler on_bytes)
{
    StartDrainIfNeeded();
    boost::system::error_code ec;
    while (m_socket.is_open())
    {
        const auto n = co_await m_socket.async_read_some(
            boost::asio::buffer(m_recv_buffer.data(), m_recv_buffer.size()),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));

        if (ec || n == 0)
        {
            // Peer closed (EOF) or hard error. Both terminate the loop;
            // the handler caller decides whether to log.
            break;
        }

        if (on_bytes)
        {
            on_bytes(std::span<const std::byte>(m_recv_buffer.data(), n));
        }
    }
    Close();
}

boost::asio::awaitable<void>
AsioSession::Send(std::span<const std::byte> bytes)
{
    if (!m_socket.is_open() || bytes.empty())
        co_return;

    co_await boost::asio::async_write(
        m_socket,
        boost::asio::buffer(bytes.data(), bytes.size()),
        boost::asio::use_awaitable);
}

boost::asio::awaitable<void>
AsioSession::RunPackets(PacketHandler on_packet)
{
    StartDrainIfNeeded();
    boost::system::error_code ec;

    // Per-iteration: read 16-byte header, parse plaintext wSize, read
    // body, decrypt header+body, dispatch.
    while (m_socket.is_open())
    {
        m_packet_buffer.resize(kPacketHeaderSize);

        // Step 1 — read header (16 bytes). async_read with the
        // transfer_exactly completion condition guarantees the full
        // header arrives or we error out.
        co_await boost::asio::async_read(
            m_socket,
            boost::asio::buffer(m_packet_buffer.data(), kPacketHeaderSize),
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) break;

        // Step 2 — peek wSize (plaintext on the wire). Validate
        // before allocating body buffer.
        auto* hdr = reinterpret_cast<PacketHeader*>(m_packet_buffer.data());
        const std::uint16_t wSize = hdr->wSize;
        if (wSize < kPacketHeaderSize || wSize >= kMaxPacketSize)
            break; // matches legacy CheckMessage PACKET_INVALID branch

        const std::size_t body_len = wSize - kPacketHeaderSize;

        // Step 3 — read body bytes.
        if (body_len > 0)
        {
            m_packet_buffer.resize(wSize);
            // hdr pointer may be invalidated by resize; re-grab.
            hdr = reinterpret_cast<PacketHeader*>(m_packet_buffer.data());

            co_await boost::asio::async_read(
                m_socket,
                boost::asio::buffer(m_packet_buffer.data() + kPacketHeaderSize, body_len),
                boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            if (ec) break;
        }

        // Step 3.5 — RC4-over-entire-packet, if enabled. Legacy
        // convention: wSize is plaintext on the wire (used for framing
        // pre-RC4), so we save it before RC4 scrambles it and restore
        // it after. Matches CSession::Decrypt at Session.cpp:84-91.
        if (!m_rc4_inbound_key.empty())
        {
            const auto saved_wsize = hdr->wSize;
            if (!tnetlib_crypto::RC4MD5Transform(
                    reinterpret_cast<unsigned char*>(m_packet_buffer.data()),
                    m_packet_buffer.size(),
                    reinterpret_cast<const unsigned char*>(m_rc4_inbound_key.data()),
                    m_rc4_inbound_key.size()))
            {
                LogProto("recv: RC4 transform failed (ip=%s, wSize=%u)",
                         m_remote_ipv4.c_str(),
                         static_cast<unsigned>(m_packet_buffer.size()));
                break;
            }
            hdr->wSize = saved_wsize;
        }

        // Step 4 — decrypt header. Key is derived from the EXPECTED
        // sequence (legacy code increments first, then uses; mirror
        // that so we and the sender agree on the key.)
        ++m_recv_sequence;
        const std::int64_t key = KeyForSequence(m_recv_sequence);
        DecryptHeader(hdr, key);

        // Step 5 — sequence-number sanity. Legacy convention is that
        // each side counts every packet (incl. failures) — see the
        // step-A audit notes. Mismatch terminates the session.
        if (hdr->dwNumber != m_recv_sequence)
        {
            LogProto("recv: sequence mismatch (ip=%s, got=%u, want=%u, wId=0x%04X)",
                     m_remote_ipv4.c_str(),
                     hdr->dwNumber, m_recv_sequence,
                     static_cast<unsigned>(hdr->wId));
            break;
        }

        // Step 6 — decrypt body + verify checksum.
        std::byte* body_ptr = m_packet_buffer.data() + kPacketHeaderSize;
        if (!DecryptBody(body_ptr, body_len, key, hdr->llChecksum))
        {
            LogProto("recv: checksum mismatch (ip=%s, wId=0x%04X, dwNumber=%u, body=%u)",
                     m_remote_ipv4.c_str(),
                     static_cast<unsigned>(hdr->wId),
                     hdr->dwNumber,
                     static_cast<unsigned>(body_len));
            break;
        }

        // Step 7 — dispatch.
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
AsioSession::SendPacket(std::uint16_t wId, std::span<const std::byte> body)
{
    if (!m_socket.is_open())
        co_return;

    // Reject sends that would overflow the 16-bit wSize. The codec's
    // contract is wSize < kMaxPacketSize (see packet_codec.h). Written
    // as a subtraction so the addition can never wrap on size_t.
    if (body.size() >= kMaxPacketSize - kPacketHeaderSize)
    {
        LogProto("send: oversized body (ip=%s, wId=0x%04X, body=%zu, max=%zu)",
                 m_remote_ipv4.c_str(),
                 static_cast<unsigned>(wId),
                 body.size(),
                 static_cast<std::size_t>(kMaxPacketSize - kPacketHeaderSize));
        co_return;
    }

    // Copy the body into the channel envelope. The drain coroutine owns
    // the lifetime from here, so the caller's `body` span need not stay
    // valid past this co_return.
    PendingSend pending;
    pending.wId  = wId;
    pending.body.assign(body.begin(), body.end());

    boost::system::error_code ec;
    co_await m_send_chan.async_send(
        boost::system::error_code{},
        std::move(pending),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));

    if (ec)
    {
        // Channel was closed (Close() called) or the operation was
        // cancelled. Quietly drop — the caller's intent was to send
        // on a session that is now going away.
        LogProto("send: enqueue dropped (ip=%s, wId=0x%04X, err=%s)",
                 m_remote_ipv4.c_str(),
                 static_cast<unsigned>(wId),
                 ec.message().c_str());
    }
}

boost::asio::awaitable<void>
AsioSession::DoSendPacket(std::uint16_t wId, std::span<const std::byte> body)
{
    if (!m_socket.is_open())
        co_return;

    // Build the frame in m_send_buffer so the async_write sees one
    // contiguous chunk (no scatter/gather needed). m_send_buffer is
    // owned exclusively by the drain coroutine.
    const std::size_t frame_size = kPacketHeaderSize + body.size();
    m_send_buffer.resize(frame_size);

    auto* hdr = reinterpret_cast<PacketHeader*>(m_send_buffer.data());
    hdr->wSize    = static_cast<std::uint16_t>(frame_size);
    hdr->wId      = wId;
    hdr->dwNumber = ++m_send_sequence;
    // llChecksum gets filled in by EncryptBody below.

    std::byte* body_dst = m_send_buffer.data() + kPacketHeaderSize;
    if (!body.empty())
        std::memcpy(body_dst, body.data(), body.size());

    const std::int64_t key = KeyForSequence(m_send_sequence);
    hdr->llChecksum = EncryptBody(body_dst, body.size(), key);
    EncryptHeader(hdr, key);

    // RC4-over-entire-packet on outbound, if enabled. Same wSize
    // save/restore as the inbound side — wSize must stay plaintext
    // for the peer's framer to read pre-RC4.
    if (!m_rc4_outbound_key.empty())
    {
        const auto saved_wsize = hdr->wSize;
        if (!tnetlib_crypto::RC4MD5Transform(
                reinterpret_cast<unsigned char*>(m_send_buffer.data()),
                m_send_buffer.size(),
                reinterpret_cast<const unsigned char*>(m_rc4_outbound_key.data()),
                m_rc4_outbound_key.size()))
        {
            LogProto("send: RC4 transform failed (ip=%s, wId=0x%04X)",
                     m_remote_ipv4.c_str(),
                     static_cast<unsigned>(wId));
            co_return;
        }
        hdr->wSize = saved_wsize;
    }

    boost::system::error_code ec;
    co_await boost::asio::async_write(
        m_socket,
        boost::asio::buffer(m_send_buffer.data(), frame_size),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));

    if (ec)
    {
        // The socket-level errors here (broken pipe, connection reset)
        // are the normal "peer went away" path; surfacing them once
        // helps diagnose unexpected drops.
        LogProto("send: write failed (ip=%s, wId=0x%04X, err=%s)",
                 m_remote_ipv4.c_str(),
                 static_cast<unsigned>(wId),
                 ec.message().c_str());
    }
}

boost::asio::awaitable<void>
AsioSession::DrainSendQueue()
{
    boost::system::error_code ec;
    while (true)
    {
        PendingSend pending = co_await m_send_chan.async_receive(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));

        if (ec)
        {
            // Channel closed (Close()) or cancelled. We're done.
            break;
        }

        // Hand off the buffered body to the actual serialize+write.
        // DoSendPacket is responsible for socket-state checks.
        co_await DoSendPacket(pending.wId,
            std::span<const std::byte>(pending.body.data(),
                                       pending.body.size()));

        if (!m_socket.is_open())
            break;
    }
}

void AsioSession::Close()
{
    // Always tear down the send channel — pending awaiters (producers
    // suspended on a full queue, the drain awaiting the next item)
    // will wake with operation_aborted/cancelled and unwind. close()
    // is idempotent for the channel itself.
    m_send_chan.close();

    if (!m_socket.is_open())
        return;
    boost::system::error_code ec;
    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    m_socket.close(ec);
}

void AsioSession::EnableInboundRC4(std::vector<std::byte> secret_key)
{
    m_rc4_inbound_key = std::move(secret_key);
}

void AsioSession::EnableOutboundRC4(std::vector<std::byte> secret_key)
{
    m_rc4_outbound_key = std::move(secret_key);
}

// ===== AsioListener =========================================================

AsioListener::AsioListener(boost::asio::any_io_executor exec, std::uint16_t port)
    : m_acceptor(exec,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
    , m_port(port)
{
    m_acceptor.set_option(boost::asio::socket_base::reuse_address(true));
    // If port was 0 (ephemeral), reflect the actual bound port back.
    if (m_port == 0)
    {
        m_port = m_acceptor.local_endpoint().port();
    }
}

boost::asio::awaitable<void>
AsioListener::Run(AcceptHandler on_accept)
{
    boost::system::error_code ec;
    while (m_acceptor.is_open())
    {
        auto socket = co_await m_acceptor.async_accept(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec)
        {
            // operation_aborted == acceptor was closed by Stop; clean exit.
            break;
        }
        if (on_accept)
        {
            on_accept(std::move(socket));
        }
    }
}

} // namespace tnetlib
