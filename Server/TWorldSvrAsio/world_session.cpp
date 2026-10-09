#include "world_session.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <spdlog/spdlog.h>

#include <cstring>
#include <utility>

namespace tworldsvr {

namespace {
// 32-bit XOR fold over the body. Identical to the FoldChecksum used
// by TPatchSvrAsio / TControlSvrAsio so the byte-on-the-wire shape
// matches legacy peers exactly.
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

struct WorldSession::SendState {
    using Permit = boost::asio::experimental::channel<void(boost::system::error_code, int)>;
    explicit SendState(boost::asio::any_io_executor executor) : permit(executor, 1) {
        permit.try_send(boost::system::error_code{}, 0);
    }
    Permit permit;
    std::size_t pending_frames = 0;
    std::size_t pending_bytes = 0;
    static constexpr std::size_t max_frames = 256;
    static constexpr std::size_t max_bytes = 4 * 1024 * 1024;
};

WorldSession::WorldSession(Socket sock)
    : m_socket(std::move(sock))
    , m_send_state(std::make_shared<SendState>(m_socket.get_executor()))
{
    boost::system::error_code ec;
    auto ep = m_socket.remote_endpoint(ec);
    if (!ec && ep.address().is_v4())
        m_remote_ipv4 = ep.address().to_v4().to_string();
}

void WorldSession::TouchRecv()
{
    m_last_recv_at.store(
        std::chrono::steady_clock::now().time_since_epoch().count());
}

boost::asio::awaitable<void>
WorldSession::Run(PacketHandler on_packet)
{
    using namespace boost::asio;
    // Read EOF, malformed framing and escaping dispatch exceptions all retire
    // the same writer, including callers waiting for their turn to send.
    struct CloseOnExit {
        std::shared_ptr<WorldSession> owner;
        ~CloseOnExit() { owner->Close(); }
    } close{shared_from_this()};

    while (m_socket.is_open())
    {
        PacketHeader hdr{};
        boost::system::error_code ec;
        co_await async_read(m_socket, buffer(&hdr, sizeof(hdr)),
            redirect_error(use_awaitable, ec));
        if (ec) break;
        if (hdr.wSize < kPacketHeaderSize || hdr.wSize > kMaxPacketSize)
        {
            spdlog::warn("world_session[{}]: framing error wSize={} — closing",
                m_remote_ipv4, hdr.wSize);
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
        const std::uint32_t expected = ComputeChecksum(body.data(), body_size);
        if (expected != hdr.dwChkSum)
        {
            spdlog::warn("world_session[{}]: checksum mismatch wID=0x{:04X} "
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
WorldSession::SendPacket(std::uint16_t wId, std::vector<std::byte> body)
{
    const auto self = shared_from_this();
    if (!IsOpen()) co_return;
    if (body.size() > kMaxPacketSize - kPacketHeaderSize) {
        spdlog::error("world_session[{}]: outbound body too big ({})", m_remote_ipv4, body.size());
        co_return;
    }
    const std::size_t total = kPacketHeaderSize + body.size();
    const auto state = m_send_state;
    if (state->pending_frames >= SendState::max_frames || total > SendState::max_bytes - state->pending_bytes) {
        spdlog::warn("world_session[{}]: outbound backlog exceeded; closing peer", m_remote_ipv4);
        Close(); co_return;
    }
    ++state->pending_frames; state->pending_bytes += total;
    struct Reservation {
        std::shared_ptr<SendState> state;
        std::size_t bytes;
        ~Reservation() { --state->pending_frames; state->pending_bytes -= bytes; }
    } reservation{state, total};
    boost::system::error_code ec;
    co_await state->permit.async_receive(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec || !IsOpen()) co_return;
    // A strand only serializes handlers, not composed writes across suspension.
    // Keep the permit and this frame alive until async_write completes.
    struct Release {
        std::shared_ptr<SendState> state;
        ~Release() { state->permit.try_send(boost::system::error_code{}, 0); }
    } release{state};
    std::vector<std::byte> frame(total);
    PacketHeader hdr{};
    hdr.wSize    = static_cast<std::uint16_t>(total);
    hdr.wID      = wId;
    hdr.dwChkSum = ComputeChecksum(body.data(), body.size());
    std::memcpy(frame.data(), &hdr, sizeof(hdr));
    if (!body.empty())
        std::memcpy(frame.data() + sizeof(hdr),
                    body.data(), body.size());
    co_await boost::asio::async_write(m_socket,
        boost::asio::buffer(frame.data(), total),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec) {
        spdlog::debug("world_session[{}]: send wID=0x{:04X} failed: {}",
            m_remote_ipv4, wId, ec.message());
        Close();
    }
}

void WorldSession::Close()
{
    m_send_state->permit.close();
    boost::system::error_code ec;
    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    m_socket.close(ec);
}

} // namespace tworldsvr
