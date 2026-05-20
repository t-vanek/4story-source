#pragma once

// LogServer — UDP receiver for legacy _UDPPACKET frames. Boost.Asio
// async_receive_from loop; each datagram is decoded into a LogRecord
// and handed to the configured ILogSink.

#include "services/log_sink.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace tlogsvr {

// Decode one received datagram into a LogRecord. Exposed for unit
// tests; the receive loop calls this on every async_receive_from
// completion. Returns false (and leaves `out` partially populated)
// on malformed input — caller must increment the drop counter and
// not forward `out` to the sink.
//
// Bad-format reasons covered:
//   * datagram shorter than UdpPacket + LogData prefix
//   * UdpPacket.command != LP_LOG
//   * UdpPacket.size declares more bytes than were received
//   * trailing blob would exceed LogData.logPayload (512 bytes)
bool DecodeLogDatagram(const std::byte* data, std::size_t len,
                       LogRecord& out);

struct LogServerConfig
{
    std::string    bind_address = "0.0.0.0";
    std::uint16_t  port = 2000;
    ILogSink*      sink = nullptr;          // non-owning
};

class LogServer
{
public:
    LogServer(boost::asio::io_context& io, LogServerConfig config);

    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const { return m_port; }
    std::uint64_t PacketsReceived() const { return m_received.load(); }
    std::uint64_t DropsBadFormat() const  { return m_drops_format.load(); }

private:
    boost::asio::io_context&        m_io;
    boost::asio::ip::udp::socket    m_socket;
    std::uint16_t                   m_port;
    ILogSink*                       m_sink;
    std::atomic<std::uint64_t>      m_received{0};
    std::atomic<std::uint64_t>      m_drops_format{0};
};

} // namespace tlogsvr
