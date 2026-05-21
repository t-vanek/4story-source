#include "log_peer.h"

#include <boost/asio/buffer.hpp>

#include <spdlog/spdlog.h>

namespace tmapsvr {

UdpLogPeer::UdpLogPeer(boost::asio::io_context& io,
                       const std::string& host,
                       std::uint16_t port)
    : m_socket(io)
{
    if (host.empty() || port == 0)
    {
        spdlog::info("log_peer: disabled (empty host or port = 0) — audit "
                     "events will go to spdlog only");
        return;
    }

    boost::system::error_code ec;

    // Resolve once at construction. The DNS lookup happens here, on
    // the boot thread, so a misconfigured hostname surfaces during
    // startup instead of on the first audit event under load.
    boost::asio::ip::udp::resolver resolver(io);
    auto endpoints = resolver.resolve(boost::asio::ip::udp::v4(),
        host, std::to_string(port), ec);
    if (ec || endpoints.empty())
    {
        spdlog::warn("log_peer: resolve {}:{} failed: {} — peer stays "
                     "disabled, audit events will go to spdlog only",
            host, port, ec ? ec.message() : "no endpoints");
        return;
    }
    m_dest = *endpoints.begin();

    m_socket.open(boost::asio::ip::udp::v4(), ec);
    if (ec)
    {
        spdlog::warn("log_peer: socket open failed: {} — peer stays "
                     "disabled", ec.message());
        return;
    }

    m_enabled = true;
    spdlog::info("log_peer: UDP sink ready at {}:{}", host, port);
}

bool UdpLogPeer::Send(std::span<const std::byte> bytes)
{
    if (!m_enabled || bytes.empty()) return false;

    boost::system::error_code ec;
    m_socket.send_to(boost::asio::buffer(bytes.data(), bytes.size()),
        m_dest, /*flags*/ 0, ec);
    if (ec)
    {
        // Log at debug, not warn — a transient UDP send failure
        // (collector restart, network blip) is exactly the
        // best-effort scenario this peer is designed for. Loud
        // warnings here would flood the spdlog sink on a slow
        // collector.
        spdlog::debug("log_peer: send failed ({} bytes): {}",
            bytes.size(), ec.message());
        return false;
    }
    return true;
}

} // namespace tmapsvr
