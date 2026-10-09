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
// Lifecycle and callbacks run on the Map runtime's single io_context thread.
// A write permit covers the entire composed write, including suspension. Each
// sender retains its original socket so queued bytes never enter a replacement.

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/experimental/channel.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <utility>

namespace tmapsvr {

class IWorldClient
{
public:
    virtual ~IWorldClient() = default;

    // Await the complete frame write, without waiting for an application ACK.
    // False means disconnected/write failure; writes are never replayed across
    // connections because their commit/processing outcome can be unknown.
    virtual boost::asio::awaitable<bool>
        SendPacket(std::uint16_t wId, std::vector<std::byte> body) = 0;

    // IsConnected reports transport availability. Admission additionally needs
    // IsRegistered: a complete acknowledgment from the current World peer.
    virtual bool IsConnected() const = 0;
    virtual bool IsRegistered() const { return IsConnected(); }
};

class AsioWorldClient final : public IWorldClient
{
public:
    // Await each inbound handler before reading the next frame. Database-backed
    // admission cannot race subsequent World verdicts; the body stays alive.
    using InboundHandler =
        std::function<boost::asio::awaitable<void>(std::uint16_t wId,
                           std::span<const std::byte> body)>;

    // Called with admission closed, before any replacement connection. Await
    // durable client teardown; false ends the reconnect loop during shutdown.
    using DisconnectHandler = std::function<boost::asio::awaitable<bool>()>;
    void SetDisconnectHandler(DisconnectHandler handler) { m_on_disconnect = std::move(handler); }

    AsioWorldClient(boost::asio::io_context& io,
                    std::string host,
                    std::uint16_t port,
                    InboundHandler on_packet = nullptr,
                    std::chrono::milliseconds backoff_initial = std::chrono::seconds(1),
                    std::chrono::milliseconds backoff_max     = std::chrono::seconds(30),
                    std::chrono::milliseconds registration_timeout = std::chrono::seconds(5));

    // Main coroutine. Loops connect → read → on disconnect, sleep +
    // retry with doubling backoff. A disconnect callback returning false ends
    // the loop; io_context shutdown also cancels pending operations.
    boost::asio::awaitable<void> Run();

    boost::asio::awaitable<bool>
        SendPacket(std::uint16_t wId, std::vector<std::byte> body) override;

    bool IsConnected() const override;
    bool IsRegistered() const override { return m_registered && IsConnected(); }

    // Map-server identity advertised to TWorld. When non-zero, the
    // client sends RW_RELAYSVR_REQ(wid) immediately on every (re)connect
    // so TWorld registers this map in its PeerRegistry and can route MW
    // traffic back (legacy parity: the map-server registration that
    // TWorld's OnRW_RELAYSVR_REQ keys m_pRelay / m_wID on). Convention:
    // LOBYTE(wid) = server_id, HIBYTE(wid) = server type (Map=4). Must be set
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
    boost::asio::awaitable<void> ReadConnection(
        std::shared_ptr<boost::asio::ip::tcp::socket> sock);

    boost::asio::io_context&    m_io;
    using WritePermit = boost::asio::experimental::channel<void(boost::system::error_code, int)>;
    std::shared_ptr<WritePermit> m_write_permit;
    DisconnectHandler m_on_disconnect;
    bool m_registered = false;
    std::chrono::milliseconds m_registration_timeout;
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
