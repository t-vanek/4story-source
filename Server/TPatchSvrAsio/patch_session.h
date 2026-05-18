#pragma once

// PatchSession — per-peer TCP session for TPatchSvrAsio. Implements
// the legacy TPatchSvr wire format (8-byte header, no crypto):
//
//   struct PACKETHEADER { WORD wSize; WORD wID; DWORD dwChkSUM; }
//
// followed by body bytes. wSize counts the entire frame (header + body).
// dwChkSUM is computed by the legacy `CPacket::Encrypt` over the body
// — we replicate the same checksum so legacy clients (TControlSvr
// pushing CT_* messages) accept our acks.

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tpatchsvr {

constexpr std::uint16_t kPacketHeaderSize = 8;  // WORD+WORD+DWORD
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

// Compute the legacy checksum over `body` (matches Packet.cpp encrypt
// branch when crypto is off — sum of bytes folded into a DWORD).
std::uint32_t ComputeChecksum(const std::byte* body, std::size_t len);

// Inbound decoded packet — header + body view. body points into the
// session's recv buffer; callers must copy before yielding.
struct DecodedPacket
{
    std::uint16_t wId;
    std::vector<std::byte> body;
};

class PatchSession : public std::enable_shared_from_this<PatchSession>
{
public:
    using PacketHandler = std::function<
        boost::asio::awaitable<void>(std::shared_ptr<PatchSession>, DecodedPacket)>;

    explicit PatchSession(boost::asio::ip::tcp::socket sock);

    // Drive the read loop. Each fully-received packet fires the
    // handler (caller-supplied dispatch). Returns when the peer
    // closes or a framing error occurs.
    boost::asio::awaitable<void> Run(PacketHandler on_packet);

    // Build + send a packet with the given id and body. Header is
    // generated here (size + checksum).
    boost::asio::awaitable<void> SendPacket(std::uint16_t wId,
                                            std::vector<std::byte> body);

    // Close the socket. Idempotent.
    void Close();

    const std::string& RemoteIPv4() const { return m_remote_ipv4; }

private:
    boost::asio::ip::tcp::socket m_socket;
    std::string                  m_remote_ipv4;
    std::vector<std::byte>       m_send_scratch;
};

} // namespace tpatchsvr
