#include "metrics_endpoint.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <spdlog/spdlog.h>

#include <istream>
#include <sstream>
#include <string>

namespace tmapsvr::ops {

std::string FormatPrometheus(const Metrics::Snapshot& snap)
{
    std::ostringstream os;

    // Handler metrics ---------------------------------------------
    os << "# HELP tmapsvr_handler_calls_total Dispatched handler calls\n"
       << "# TYPE tmapsvr_handler_calls_total counter\n";
    for (const auto& h : snap.handlers)
        os << "tmapsvr_handler_calls_total{wId=\"0x"
           << std::hex << h.wId << std::dec << "\"} " << h.calls << "\n";

    os << "# HELP tmapsvr_handler_errors_total Failed handler calls "
       "(rate-limit / exception)\n"
       << "# TYPE tmapsvr_handler_errors_total counter\n";
    for (const auto& h : snap.handlers)
        os << "tmapsvr_handler_errors_total{wId=\"0x"
           << std::hex << h.wId << std::dec << "\"} " << h.errors << "\n";

    os << "# HELP tmapsvr_handler_latency_us Handler latency summary "
       "(microseconds)\n"
       << "# TYPE tmapsvr_handler_latency_us summary\n";
    for (const auto& h : snap.handlers)
    {
        const auto label_prefix = std::string{"{wId=\"0x"};
        std::ostringstream label;
        label << label_prefix << std::hex << h.wId << std::dec << "\"";
        os << "tmapsvr_handler_latency_us_count" << label.str() << "} "
           << h.lat_count << "\n";
        os << "tmapsvr_handler_latency_us_sum"   << label.str() << "} "
           << h.lat_sum_us << "\n";
        os << "tmapsvr_handler_latency_us_min"   << label.str() << "} "
           << h.lat_min_us << "\n";
        os << "tmapsvr_handler_latency_us_max"   << label.str() << "} "
           << h.lat_max_us << "\n";
    }

    // DB query metrics --------------------------------------------
    os << "# HELP tmapsvr_db_query_calls_total SOCI query invocations\n"
       << "# TYPE tmapsvr_db_query_calls_total counter\n";
    for (const auto& q : snap.queries)
        os << "tmapsvr_db_query_calls_total{name=\"" << q.name << "\"} "
           << q.calls << "\n";

    os << "# HELP tmapsvr_db_query_latency_us SOCI query latency summary\n"
       << "# TYPE tmapsvr_db_query_latency_us summary\n";
    for (const auto& q : snap.queries)
    {
        const auto lbl = std::string{"{name=\""} + q.name + "\"";
        os << "tmapsvr_db_query_latency_us_count" << lbl << "} "
           << q.lat_count << "\n";
        os << "tmapsvr_db_query_latency_us_sum"   << lbl << "} "
           << q.lat_sum_us << "\n";
        os << "tmapsvr_db_query_latency_us_min"   << lbl << "} "
           << q.lat_min_us << "\n";
        os << "tmapsvr_db_query_latency_us_max"   << lbl << "} "
           << q.lat_max_us << "\n";
    }

    return os.str();
}

MetricsEndpoint::MetricsEndpoint(boost::asio::io_context& io,
                                 std::uint16_t            port,
                                 const Metrics&           metrics)
    : m_io(io)
    , m_metrics(metrics)
    , m_acceptor(io)
    , m_port(0)
{
    if (port == 0) return;
    using boost::asio::ip::tcp;
    tcp::endpoint ep(boost::asio::ip::address_v4::loopback(), port);
    boost::system::error_code ec;
    m_acceptor.open(ep.protocol(), ec);
    if (ec) { spdlog::warn("metrics: open failed: {}", ec.message()); return; }
    m_acceptor.set_option(tcp::acceptor::reuse_address(true), ec);
    m_acceptor.bind(ep, ec);
    if (ec) { spdlog::warn("metrics: bind {}: {}", port, ec.message()); return; }
    m_acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
    if (ec) { spdlog::warn("metrics: listen {}: {}", port, ec.message()); return; }
    m_port = m_acceptor.local_endpoint().port();
    spdlog::info("metrics: /metrics endpoint listening on 127.0.0.1:{}",
        m_port);
}

void MetricsEndpoint::StopAccepting()
{
    boost::system::error_code ignored;
    m_acceptor.close(ignored);
}

boost::asio::awaitable<void>
MetricsEndpoint::Run()
{
    using boost::asio::ip::tcp;
    if (!m_acceptor.is_open()) co_return;
    while (m_acceptor.is_open())
    {
        boost::system::error_code ec;
        tcp::socket sock = co_await m_acceptor.async_accept(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) break;
        boost::asio::co_spawn(m_io,
            HandleConnection(std::move(sock)),
            boost::asio::detached);
    }
}

boost::asio::awaitable<void>
MetricsEndpoint::HandleConnection(boost::asio::ip::tcp::socket sock)
{
    // Tiny HTTP/1.1 reader — consume up to the end of headers
    // (CRLFCRLF), parse the request line, ignore everything else.
    boost::asio::streambuf buf;
    boost::system::error_code ec;
    co_await boost::asio::async_read_until(sock, buf, "\r\n\r\n",
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec) co_return;

    std::istream is(&buf);
    std::string method, path, version;
    is >> method >> path >> version;

    std::string body;
    std::string status;
    std::string content_type = "text/plain; version=0.0.4";

    if (method == "GET" && (path == "/metrics" || path == "/"))
    {
        body   = FormatPrometheus(m_metrics.Sample());
        status = "200 OK";
    }
    else
    {
        body   = "Not Found\n";
        status = "404 Not Found";
    }

    std::ostringstream os;
    os << "HTTP/1.1 " << status << "\r\n"
       << "Content-Type: " << content_type << "\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Connection: close\r\n"
       << "\r\n"
       << body;
    const auto response = os.str();
    co_await boost::asio::async_write(sock,
        boost::asio::buffer(response),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));

    sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    sock.close(ec);
}

} // namespace tmapsvr::ops
