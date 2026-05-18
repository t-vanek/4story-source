#include "fourstory/ops/health_endpoint.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/write.hpp>

#include <spdlog/spdlog.h>

#include <chrono>
#include <istream>
#include <sstream>
#include <string>

namespace fourstory::ops {

namespace {

constexpr const char* kVersion = "5.0";

std::string BuildHealthJson(std::chrono::seconds uptime)
{
    std::ostringstream os;
    os << "{\"status\":\"ok\""
       << ",\"uptime_seconds\":" << uptime.count()
       << ",\"version\":\"" << kVersion << "\"}";
    return os.str();
}

std::string BuildHttpResponse(int status_code,
                              std::string_view reason,
                              std::string_view content_type,
                              std::string_view body)
{
    std::ostringstream os;
    os << "HTTP/1.1 " << status_code << " " << reason << "\r\n"
       << "Content-Type: " << content_type << "\r\n"
       << "Content-Length: " << body.size() << "\r\n"
       << "Connection: close\r\n"
       << "\r\n"
       << body;
    return os.str();
}

boost::asio::awaitable<void>
HandleHealthRequest(boost::asio::ip::tcp::socket socket,
                    std::chrono::steady_clock::time_point started_at)
{
    boost::system::error_code ec;
    boost::asio::streambuf buf;

    // Read just the request line — we don't care about headers.
    co_await boost::asio::async_read_until(
        socket, buf, "\r\n",
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    if (ec) co_return;

    std::istream is(&buf);
    std::string method, target, version;
    is >> method >> target >> version;

    std::string response;
    if (target == "/healthz")
    {
        const auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - started_at);
        response = BuildHttpResponse(200, "OK", "application/json",
            BuildHealthJson(uptime));
    }
    else
    {
        response = BuildHttpResponse(404, "Not Found", "text/plain",
            "Not Found\n");
    }

    co_await boost::asio::async_write(
        socket, boost::asio::buffer(response),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));

    socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    socket.close(ec);
}

} // namespace

HealthEndpoint::HealthEndpoint(boost::asio::io_context& io, std::uint16_t port)
    : m_acceptor(io.get_executor(),
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
    , m_port(port)
    , m_started_at(std::chrono::steady_clock::now())
{
    m_acceptor.set_option(boost::asio::socket_base::reuse_address(true));
    if (m_port == 0)
    {
        m_port = m_acceptor.local_endpoint().port();
    }
}

boost::asio::awaitable<void>
HealthEndpoint::Run()
{
    boost::system::error_code ec;
    auto executor = co_await boost::asio::this_coro::executor;
    while (m_acceptor.is_open())
    {
        auto socket = co_await m_acceptor.async_accept(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) break;
        boost::asio::co_spawn(
            executor,
            HandleHealthRequest(std::move(socket), m_started_at),
            boost::asio::detached);
    }
}

} // namespace fourstory::ops
