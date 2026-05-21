#pragma once

// Log peer — UDP fire-and-forget link to a structured audit /
// telemetry collector (typically TLogSvrAsio).
//
// Unlike the WorldClient (persistent TCP with reconnect), the log
// peer is connectionless: Send takes a packed event payload and
// hands it to the OS, with no acknowledgement and no retry on the
// wire. UDP is the right fit because the log side is best-effort
// observability — dropping a packet under extreme load is preferred
// to back-pressuring the gameplay loop.
//
// Why now (T3): T4 observability will introduce structured audit
// events (login, char load, handler latency, …) that ship through
// this sink. Having the transport ready in T3 keeps T4 focused on
// the event schema + emission sites instead of also having to wire
// a new socket.
//
// Why the interface: production uses UdpLogPeer; tests use a
// FakeLogPeer that records each Send into an in-memory vector for
// inspection. Both implement ILogPeer.

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace tmapsvr {

class ILogPeer
{
public:
    virtual ~ILogPeer() = default;

    // Synchronous, non-blocking, fire-and-forget. Returns true when
    // the packet was handed to the OS send queue; false on socket
    // error or when the peer is disabled. Callers should not rely
    // on the return value for correctness — log emission is
    // best-effort. UDP, so no await is needed.
    virtual bool Send(std::span<const std::byte> bytes) = 0;

    virtual bool Enabled() const = 0;
};

// Production UDP impl. Constructed once at boot from cfg.audit_udp
// (host, port). Resolves the destination at construction and reuses
// the resolved endpoint for every Send — no per-send DNS hit.
//
// Empty host disables the peer: Send becomes a no-op returning
// false, Enabled() returns false. Lets dev runs and unit tests skip
// the UDP socket entirely.
class UdpLogPeer final : public ILogPeer
{
public:
    UdpLogPeer(boost::asio::io_context& io,
               const std::string& host,
               std::uint16_t port);

    bool Send(std::span<const std::byte> bytes) override;
    bool Enabled() const override { return m_enabled; }

private:
    boost::asio::ip::udp::socket   m_socket;
    boost::asio::ip::udp::endpoint m_dest;
    bool                           m_enabled = false;
};

} // namespace tmapsvr
