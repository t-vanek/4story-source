#pragma once

// Minimal HTTP /healthz endpoint for k8s liveness/readiness probes
// and load-balancer health checks. Listens on a separate port from
// the game protocol so it's firewall-able + can be exposed without
// opening the wire protocol to the internet.
//
// Response format (Content-Type: application/json):
//   {"status":"ok","uptime_seconds":N,"version":"5.0"}
//
// HTTP parser is intentionally minimal — accepts any request that
// targets `/healthz` (anything else returns 404). No keepalive, no
// content-length parsing, no headers honored. ~50 LOC of Asio + raw
// string responses. Boost.Beast would be overkill for one endpoint.

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <chrono>
#include <cstdint>

namespace fourstory::ops {

class HealthEndpoint
{
public:
    HealthEndpoint(boost::asio::io_context& io, std::uint16_t port);

    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const { return m_port; }

private:
    boost::asio::ip::tcp::acceptor m_acceptor;
    std::uint16_t                  m_port;
    std::chrono::steady_clock::time_point m_started_at;
};

} // namespace fourstory::ops
