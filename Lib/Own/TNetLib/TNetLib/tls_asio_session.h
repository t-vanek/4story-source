#pragma once

// TlsAsioSession — mutual-TLS variant of AsioSession for peer-to-peer
// links. Wraps boost::asio::ssl::stream<tcp::socket> instead of the
// plain socket; everything above the transport (packet codec, send
// queue, error logger) is identical to AsioSession.
//
// Why a parallel class instead of templating AsioSession:
//   * AsioSession's API exposes Socket() -> tcp::socket& which would
//     change shape across instantiations. Adapting every caller is
//     a separate cleanup that doesn't need to gate the TLS rollout.
//   * Peer sessions and client sessions have different lifecycles
//     (handshake, cert verification, scope of who can connect). A
//     dedicated class keeps that surface visible.
//
// Per the PEER_PROTOCOL_PLAN Phase A design:
//   * Mutual TLS — server verifies client cert, client verifies
//     server cert. CN expected to match PeerAuthRow.peer_name.
//   * SSL_CTX is provided by the caller (typically built once per
//     process at boot); TlsAsioSession does not own the cert chain.
//   * The legacy 4Story wire codec (XOR + checksum + sequence
//     numbers) still runs on top of the TLS stream — defense in
//     depth, and it keeps the on-stream byte layout identical to
//     the plain-TCP peer path, so the rollout is a transport swap
//     with no protocol change.
//   * RC4-over-entire-packet is NOT used on TLS sessions; the
//     transport already provides confidentiality.

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/experimental/channel.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "asio_session.h"   // DecodedPacket, PeerType, ErrorLogger reuse
#include "packet_codec.h"

namespace tnetlib {

// Side of the TLS handshake. Driven by who initiated the TCP
// connection: the accept-side calls Handshake(Server), the
// connect-side calls Handshake(Client).
enum class TlsRole
{
    Server,
    Client
};

class TlsAsioSession : public std::enable_shared_from_this<TlsAsioSession>
{
public:
    using BytesHandler  = AsioSession::BytesHandler;
    using PacketHandler = AsioSession::PacketHandler;
    using SslStream     = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

    // Construct from an already-accepted/connected tcp::socket and a
    // pre-configured SSL_CTX. The session takes ownership of the socket;
    // the SSL_CTX is borrowed (lifetime managed by the caller).
    TlsAsioSession(boost::asio::ip::tcp::socket socket,
                   boost::asio::ssl::context&   ctx);
    ~TlsAsioSession();

    TlsAsioSession(const TlsAsioSession&)            = delete;
    TlsAsioSession& operator=(const TlsAsioSession&) = delete;

    // Drive the TLS handshake. Must be co_awaited before any Run /
    // RunPackets / SendPacket call. Returns true on success; on
    // failure the session is closed and the caller should drop it.
    boost::asio::awaitable<bool> Handshake(TlsRole role);

    // Same shapes as AsioSession.
    boost::asio::awaitable<void> Run(BytesHandler on_bytes);
    boost::asio::awaitable<void> RunPackets(PacketHandler on_packet);
    boost::asio::awaitable<void> Send(std::span<const std::byte> bytes);
    boost::asio::awaitable<void> SendPacket(std::uint16_t wId,
                                            std::span<const std::byte> body);

    // Graceful TLS close + socket close. Idempotent.
    void Close();

    // Cached peer CN extracted from the peer cert at Handshake() time.
    // Empty if Handshake hasn't run or peer presented no cert.
    const std::string& PeerCommonName() const { return m_peer_cn; }

    // Cached peer IPv4 (dotted-decimal), captured at construction time.
    // Empty if the socket wasn't connected.
    const std::string& RemoteIPv4() const { return m_remote_ipv4; }

    SslStream&       Stream()       { return m_stream; }
    const SslStream& Stream() const { return m_stream; }

private:
    struct PendingSend
    {
        std::uint16_t           wId;
        std::vector<std::byte>  body;
    };
    using SendChannel = boost::asio::experimental::channel<
        void(boost::system::error_code, PendingSend)>;

    boost::asio::awaitable<void> DoSendPacket(std::uint16_t wId,
                                              std::span<const std::byte> body);
    boost::asio::awaitable<void> DrainSendQueue();
    void StartDrainIfNeeded();

    SslStream                    m_stream;       // owns the socket via next_layer()
    std::vector<std::byte>       m_recv_buffer;
    std::vector<std::byte>       m_packet_buffer;
    std::vector<std::byte>       m_send_buffer;
    std::uint32_t                m_recv_sequence = 0;
    std::uint32_t                m_send_sequence = 0;
    std::string                  m_remote_ipv4;
    std::string                  m_peer_cn;
    SendChannel                  m_send_chan;
    std::atomic<bool>            m_drain_started{false};
    std::atomic<bool>            m_closed{false};
};

} // namespace tnetlib
