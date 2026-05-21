#pragma once

// MetricsEndpoint — minimal HTTP server exposing the Metrics
// registry in Prometheus text-exposition format on `GET /metrics`.
//
// Why a separate listener: the existing fourstory::ops::HealthEndpoint
// serves /healthz on a small text response; mixing /metrics (which
// can be many KB once handler + DB-query metrics fill out) on the
// same port would couple monitoring scrape latency to the liveness
// probe. Two ports, two responsibilities.
//
// Scope is intentionally tight: no HTTP keep-alive, no compression,
// no auth — bind on localhost (default) and have Prometheus or a
// sidecar scrape locally. Any non-/metrics GET returns 404.

#include "metrics.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>

namespace tmapsvr::ops {

class MetricsEndpoint
{
public:
    MetricsEndpoint(boost::asio::io_context& io,
                    std::uint16_t            port,
                    const Metrics&           metrics);

    boost::asio::awaitable<void> Run();

    std::uint16_t Port() const { return m_port; }

    void StopAccepting();

private:
    boost::asio::awaitable<void> HandleConnection(
        boost::asio::ip::tcp::socket sock);

    boost::asio::io_context&         m_io;
    const Metrics&                   m_metrics;
    boost::asio::ip::tcp::acceptor   m_acceptor;
    std::uint16_t                    m_port;
};

// Format helper — exposed for unit tests of the text format
// independently of the listener. Returns the Prometheus-format
// body (no HTTP headers).
std::string FormatPrometheus(const Metrics::Snapshot& snap);

} // namespace tmapsvr::ops
