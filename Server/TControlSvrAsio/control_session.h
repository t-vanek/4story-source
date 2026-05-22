#pragma once

// ControlSession — TCP framing primitive shared by OperatorSession
// (inbound from GM operators / TController.exe) and PeerSession
// (outbound to LoginSvr/MapSvr/...). Legacy wire format is the same
// 8-byte CPacket header as Patch (WORD wSize | WORD wID | DWORD
// dwChkSum), no RC4 — TControlSvr is on the operator LAN, not the
// public internet.
//
// The session is intentionally protocol-only: handlers are wired by
// the caller through Run()'s callback. State that's specific to a
// role (operator authority / peer service ID) lives on the wrappers
// in operator_session.h and peer_session.h.

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace tcontrolsvr {

constexpr std::uint16_t kPacketHeaderSize = 8;   // WORD+WORD+DWORD
constexpr std::uint16_t kMaxPacketSize    = 0xFFFF;

#pragma pack(push, 1)
struct PacketHeader
{
    std::uint16_t wSize;
    std::uint16_t wID;
    std::uint32_t dwChkSum;
};
#pragma pack(pop)
static_assert(sizeof(PacketHeader) == kPacketHeaderSize,
              "PacketHeader must be 8 bytes");

// Body checksum identical to TPatchSvrAsio (running 32-bit XOR fold).
// Matches the legacy CPacket::Encrypt branch with crypto disabled —
// see Lib/Own/TNetLib/TNetLib/Packet.cpp.
std::uint32_t ComputeChecksum(const std::byte* body, std::size_t len);

struct DecodedPacket
{
    std::uint16_t           wId;
    std::vector<std::byte>  body;
};

class ControlSession : public std::enable_shared_from_this<ControlSession>
{
public:
    using PacketHandler = std::function<
        boost::asio::awaitable<void>(std::shared_ptr<ControlSession>, DecodedPacket)>;

    using PlainSocket = boost::asio::ip::tcp::socket;
    using TlsStream   = boost::asio::ssl::stream<PlainSocket>;

    // Plain-TCP ctor — legacy operator and pre-Phase-A peer paths.
    explicit ControlSession(PlainSocket sock);

    // TLS ctor — caller already wrapped the accepted socket in an
    // ssl::stream and (typically) co_awaited async_handshake(server)
    // before constructing the session. Inbound bytes from here on
    // arrive plaintext on this side, ciphertext on the wire.
    explicit ControlSession(TlsStream stream);

    // Read loop. Returns when peer closes or framing breaks.
    boost::asio::awaitable<void> Run(PacketHandler on_packet);

    // Build + send one frame. Header (size + checksum) is computed
    // here so callers only ship body bytes.
    boost::asio::awaitable<void> SendPacket(std::uint16_t wId,
                                            std::vector<std::byte> body);

    void Close();
    bool IsOpen() const;

    const std::string& RemoteIPv4() const { return m_remote_ipv4; }

    std::chrono::steady_clock::time_point ConnectedAt() const
    {
        return m_connected_at;
    }

    // Last inbound packet timestamp — used by the peer-keepalive
    // timer (legacy m_dwRecvTick > 60s == offline).
    std::chrono::steady_clock::time_point LastRecvAt() const
    {
        return std::chrono::steady_clock::time_point{
            std::chrono::steady_clock::duration{m_last_recv_at.load()}};
    }

private:
    void TouchRecv();

    // Underlying tcp::socket regardless of whether we're plain or
    // TLS — used for is_open(), shutdown/close, and the cached IP
    // lookup at construction time.
    PlainSocket& UnderlyingTcp();
    const PlainSocket& UnderlyingTcp() const;

    using SocketVariant = std::variant<PlainSocket, TlsStream>;
    SocketVariant                         m_socket;
    std::string                           m_remote_ipv4;
    std::vector<std::byte>                m_send_scratch;
    std::chrono::steady_clock::time_point m_connected_at{
        std::chrono::steady_clock::now()};
    std::atomic<std::chrono::steady_clock::rep> m_last_recv_at{
        std::chrono::steady_clock::now().time_since_epoch().count()};
};

} // namespace tcontrolsvr
