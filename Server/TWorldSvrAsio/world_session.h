#pragma once

// Original server CPacket transport: WORD size, WORD opcode, DWORD number,
// INT64 checksum (Packet.h:36–43). SESSION_SERVER / !UseCrypt transmits plain
// bytes and ignores number/checksum on receive (Session.cpp:454–474).
// Fresh output zeros both fields. Copied client packets can retain a number;
// these unused fields are neither actor authority nor a replay fence.
// Peer roles/admission and body authorization belong to the existing handlers.

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <bit>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace tworldsvr {

constexpr std::uint16_t kPacketHeaderSize = 16;
constexpr std::uint16_t kMaxPacketSize    = 0xFFFE; // source total < 0xFFFF

#pragma pack(push, 1)
struct PacketHeader
{
    std::uint16_t wSize;
    std::uint16_t wID;
    std::uint32_t dwNumber;
    std::int64_t llChecksum;
};
#pragma pack(pop)
static_assert(sizeof(PacketHeader) == kPacketHeaderSize,
              "Original CPacket header must be 16 bytes");
static_assert(std::endian::native == std::endian::little,
              "Original CPacket layout requires little-endian serialization");

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
    // breaks (wSize out of range or short read). Source plaintext receive
    // ignores the number/checksum metadata; client crypto is never applied.
    boost::asio::awaitable<void> Run(PacketHandler on_packet);

    // Build + send one frame. Size/opcode and zero metadata are written
    // here so callers only ship body bytes. One complete write at a time, with
    // per-peer backpressure: at most 256 outstanding frames and 4 MiB including
    // headers. Overflow/write failure closes the peer; no write replay.
    // Run/SendPacket/Close are confined to the runtime's single I/O thread.
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
    struct SendState;
    std::shared_ptr<SendState>             m_send_state;
    std::chrono::steady_clock::time_point m_connected_at{
        std::chrono::steady_clock::now()};
    std::atomic<std::chrono::steady_clock::rep> m_last_recv_at{
        std::chrono::steady_clock::now().time_since_epoch().count()};
};

} // namespace tworldsvr
