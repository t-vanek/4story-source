#include "patch_session.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <spdlog/spdlog.h>

#include <cstring>
#include <utility>

namespace tpatchsvr {

namespace {
// Legacy CPacket::Encrypt with no crypto enabled folds the body bytes
// into a single DWORD checksum, then stamps it back into the header.
// We compute the same fold so legacy CTControlSvr accepts our acks.
//
// Algorithm (from Packet.cpp): sum body bytes as little-endian DWORDs
// running XOR. For a body that isn't a multiple of 4 bytes, the tail
// is folded one byte at a time into the low byte of the accumulator.
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
    {
        acc ^= static_cast<std::uint32_t>(p[i]);
    }
    return acc;
}
} // namespace

std::uint32_t ComputeChecksum(const std::byte* body, std::size_t len)
{
    return FoldChecksum(body, len);
}

PatchSession::PatchSession(boost::asio::ip::tcp::socket sock)
    : m_socket(std::move(sock))
{
    boost::system::error_code ec;
    auto ep = m_socket.remote_endpoint(ec);
    if (!ec && ep.address().is_v4())
        m_remote_ipv4 = ep.address().to_v4().to_string();
}

boost::asio::awaitable<void>
PatchSession::Run(PacketHandler on_packet)
{
    using namespace boost::asio;
    while (m_socket.is_open())
    {
        PacketHeader hdr{};
        boost::system::error_code ec;
        co_await async_read(m_socket, buffer(&hdr, sizeof(hdr)),
            redirect_error(use_awaitable, ec));
        if (ec) break;
        if (hdr.wSize < kPacketHeaderSize || hdr.wSize > kMaxPacketSize)
        {
            spdlog::warn("patch_session: framing error (wSize={}) — closing",
                hdr.wSize);
            break;
        }
        const std::size_t body_size = hdr.wSize - kPacketHeaderSize;
        std::vector<std::byte> body(body_size);
        if (body_size > 0)
        {
            co_await async_read(m_socket, buffer(body.data(), body_size),
                redirect_error(use_awaitable, ec));
            if (ec) break;
        }
        // Verify checksum. Mismatch = forged / corrupt packet — drop
        // the session same as legacy `PACKET_INVALID`.
        const std::uint32_t expected = ComputeChecksum(body.data(), body_size);
        if (expected != hdr.dwChkSum)
        {
            spdlog::warn("patch_session: checksum mismatch wID=0x{:04X} "
                         "(got 0x{:08X} expected 0x{:08X}) — closing",
                hdr.wID, hdr.dwChkSum, expected);
            break;
        }
        DecodedPacket pkt{ hdr.wID, std::move(body) };
        co_await on_packet(shared_from_this(), std::move(pkt));
    }
}

boost::asio::awaitable<void>
PatchSession::SendPacket(std::uint16_t wId, std::vector<std::byte> body)
{
    if (!m_socket.is_open()) co_return;
    const std::size_t total = kPacketHeaderSize + body.size();
    if (total > kMaxPacketSize)
    {
        spdlog::error("patch_session: outbound packet too big ({})", total);
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
    co_await boost::asio::async_write(m_socket,
        boost::asio::buffer(m_send_scratch.data(), total),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec)
        spdlog::debug("patch_session: send wID=0x{:04X} failed: {}",
            wId, ec.message());
}

void PatchSession::Close()
{
    boost::system::error_code ec;
    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    m_socket.close(ec);
}

} // namespace tpatchsvr
