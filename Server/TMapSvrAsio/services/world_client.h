#pragma once

// AsioWorldClient — persistent outbound TCP link to TWorldSvr.
//
// The legacy CTMapSvrModule held a single `CSession m_world` that was
// connected once at boot and reused for every map↔world packet (mostly
// MW_/DM_/SS_ family). This is the modern equivalent: a coroutine that
// connects, reads inbound packets, and on disconnect schedules a
// reconnect with exponential backoff (1s → 30s). The mode is
// server-to-server so the wire codec runs plain (no RC4 layer; just
// the XOR header + body codec that AsioSession applies by default).
//
// Thread safety (T3): SendPacket dispatches to an internal strand so
// concurrent calls from multiple handler coroutines / threads are
// serialized correctly even when the io_context runs across multiple
// threads. The strand wraps the io_context's executor; a multi-thread
// io.run() pool is now safe to use without the send-side race that
// the F5 commit documented as a TODO.

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace tmapsvr {

class IWorldClient
{
public:
    virtual ~IWorldClient() = default;

    // Fire-and-forget packet send. Returns true if the bytes were
    // handed off to the AsioSession (does NOT wait for ACK on the
    // wire); false when the session is currently disconnected (caller
    // can choose to retry / buffer / drop).
    virtual boost::asio::awaitable<bool>
        SendPacket(std::uint16_t wId, std::vector<std::byte> body) = 0;

    // Lifecycle gate — handlers use this to decide whether to even
    // attempt SendPacket (no buffering yet in F5).
    virtual bool IsConnected() const = 0;
};

class AsioWorldClient final : public IWorldClient
{
public:
    // Per-packet callback fired for each inbound frame from the world
    // peer. Called synchronously from the read loop; the callback is
    // expected to copy the body and co_spawn its own dispatch
    // coroutine (the body span is only valid during the call). main()
    // builds the lambda that captures the HandlerContext and routes
    // to handlers_world::DispatchWorld.
    using InboundHandler =
        std::function<void(std::uint16_t wId,
                           std::span<const std::byte> body)>;

    AsioWorldClient(boost::asio::io_context& io,
                    std::string host,
                    std::uint16_t port,
                    InboundHandler on_packet = nullptr,
                    std::chrono::milliseconds backoff_initial = std::chrono::seconds(1),
                    std::chrono::milliseconds backoff_max     = std::chrono::seconds(30));

    // Main coroutine. Loops connect → read → on disconnect, sleep +
    // retry with doubling backoff. Returns only when the io_context
    // stops (clean shutdown).
    boost::asio::awaitable<void> Run();

    boost::asio::awaitable<bool>
        SendPacket(std::uint16_t wId, std::vector<std::byte> body) override;

    bool IsConnected() const override;

    // Map-server identity advertised to TWorld. When non-zero, the
    // client sends RW_RELAYSVR_REQ(wid) immediately on every (re)connect
    // so TWorld registers this map in its PeerRegistry and can route MW
    // traffic back (legacy parity: the map-server registration that
    // TWorld's OnRW_RELAYSVR_REQ keys m_pRelay / m_wID on). Convention:
    // LOBYTE(wid) = server_id, HIBYTE(wid) = group_id. Must be set
    // before Run() for the link to be routable; a zero wid leaves the
    // link anonymous (transport only).
    void SetRelayWid(std::uint16_t wid) { m_relay_wid = wid; }

private:
    // One connect attempt. Returns a connected socket on success,
    // nullptr on failure. The map↔world link is server-to-server, so
    // it speaks the cluster's 8-byte plain SS frame (WORD wSize | WORD
    // wID | DWORD XOR-fold-checksum) — the same shape TWorld's
    // WorldSession and TControlSvr's ControlSession use — NOT the
    // 16-byte sequenced client codec in tnetlib::AsioSession.
    boost::asio::awaitable<std::shared_ptr<boost::asio::ip::tcp::socket>>
        DialOnce();

    // Send RW_RELAYSVR_REQ(m_relay_wid) on the freshly-established
    // session. No-op when m_relay_wid == 0.
    boost::asio::awaitable<void> SendRegister();

    boost::asio::io_context&    m_io;
    // Strand wrapping the io_context's executor — used by SendPacket
    // to serialize concurrent outbound calls. The read loop runs on
    // its own coroutine (no strand) so inbound dispatch isn't blocked
    // by outbound traffic. Same pattern as the strand-based send
    // queues in other Asio servers.
    boost::asio::strand<boost::asio::any_io_executor> m_send_strand;
    std::string                 m_host;
    std::uint16_t               m_port;
    InboundHandler              m_on_packet;
    std::chrono::milliseconds   m_backoff_initial;
    std::chrono::milliseconds   m_backoff_max;
    std::uint16_t               m_relay_wid = 0;

    // Active connected socket, or nullptr when disconnected. shared_ptr
    // so the read loop and SendPacket callers can hold refs across
    // co_awaits without resurrecting a dead socket.
    std::shared_ptr<boost::asio::ip::tcp::socket> m_session;
};

} // namespace tmapsvr
