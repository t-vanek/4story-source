#pragma once

// WorldSession — TCP framing primitive for the inbound SS link from
// TMap / TLogin peers. Legacy wire format is the same 8-byte CPacket
// header as Patch/Control (WORD wSize | WORD wID | DWORD dwChkSum),
// no RC4 — the SS link is on the operator LAN, not the public net.
// (Confirmed against Server/TControlSvrAsio/control_session.h.)
//
// The session is intentionally protocol-only: handlers are wired by
// the caller through Run()'s callback. Per-peer state (svr_id, role,
// channels, subscribed maps) lives on a wrapper added in W2 once the
// W↔M wire contract is settled (see PATCH_README §6 gating clause).
//
// This is parallel to TControlSvrAsio's ControlSession but kept
// separate because (a) the namespaces differ and (b) the W3+ peer
// wrappers will diverge from the operator/peer split TControl uses.

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tworldsvr {

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

// Body checksum: running 32-bit XOR fold, same primitive as the four
// shipped Asio daemons. Mirrors the legacy CPacket::Encrypt branch
// with crypto disabled (Lib/Own/TNetLib/TNetLib/Packet.cpp).
std::uint32_t ComputeChecksum(const std::byte* body, std::size_t len);

struct DecodedPacket
{
    std::uint16_t           wId;
    std::vector<std::byte>  body;
};

class WorldSession : public std::enable_shared_from_this<WorldSession>
{
public:
    using Socket        = boost::asio::ip::tcp::socket;
    using PacketHandler = std::function<
        boost::asio::awaitable<void>(std::shared_ptr<WorldSession>, DecodedPacket)>;

    explicit WorldSession(Socket sock);

    // Read loop. Returns when the peer closes the socket or framing
    // breaks (wSize out of range, checksum mismatch, short read).
    boost::asio::awaitable<void> Run(PacketHandler on_packet);

    // Build + send one frame. Header (size + checksum) is computed
    // here so callers only ship body bytes.
    boost::asio::awaitable<void> SendPacket(std::uint16_t wId,
                                            std::vector<std::byte> body);

    void Close();
    bool IsOpen() const { return m_socket.is_open(); }

    const std::string& RemoteIPv4() const { return m_remote_ipv4; }

    std::chrono::steady_clock::time_point ConnectedAt() const
    {
        return m_connected_at;
    }

    // Last inbound packet timestamp — used by the peer-keepalive
    // timer (legacy m_dwRecvTick > 60s = offline).
    std::chrono::steady_clock::time_point LastRecvAt() const
    {
        return std::chrono::steady_clock::time_point{
            std::chrono::steady_clock::duration{m_last_recv_at.load()}};
    }

private:
    void TouchRecv();

    Socket                                m_socket;
    std::string                           m_remote_ipv4;
    std::vector<std::byte>                m_send_scratch;
    std::chrono::steady_clock::time_point m_connected_at{
        std::chrono::steady_clock::now()};
    std::atomic<std::chrono::steady_clock::rep> m_last_recv_at{
        std::chrono::steady_clock::now().time_since_epoch().count()};
};

} // namespace tworldsvr
