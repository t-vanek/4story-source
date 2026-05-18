#include "admin_shell.h"

#include "services/connection_registry.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <spdlog/spdlog.h>

#include <chrono>
#include <sstream>
#include <utility>

namespace tloginsvr {

namespace {

std::string Trim(std::string s)
{
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' ||
                          s.back() == ' '  || s.back() == '\t'))
        s.pop_back();
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

spdlog::level::level_enum ParseLevel(const std::string& s)
{
    if (s == "trace")    return spdlog::level::trace;
    if (s == "debug")    return spdlog::level::debug;
    if (s == "info")     return spdlog::level::info;
    if (s == "warn")     return spdlog::level::warn;
    if (s == "error")    return spdlog::level::err;
    if (s == "critical") return spdlog::level::critical;
    if (s == "off")      return spdlog::level::off;
    return spdlog::level::n_levels;
}

} // namespace

AdminShell::AdminShell(boost::asio::io_context& io,
                       const std::string& bind_address,
                       std::uint16_t port,
                       services::IConnectionRegistry* connection_registry,
                       std::chrono::steady_clock::time_point started_at)
    : m_acceptor(io)
    , m_port(port)
    , m_registry(connection_registry)
    , m_started_at(started_at)
{
    using boost::asio::ip::tcp;
    tcp::endpoint ep(boost::asio::ip::make_address(bind_address), port);
    m_acceptor.open(ep.protocol());
    m_acceptor.set_option(tcp::acceptor::reuse_address(true));
    m_acceptor.bind(ep);
    m_acceptor.listen();
    m_port = m_acceptor.local_endpoint().port();
}

boost::asio::awaitable<void> AdminShell::Run()
{
    auto self = shared_from_this();
    while (m_acceptor.is_open())
    {
        boost::system::error_code ec;
        auto raw = co_await m_acceptor.async_accept(
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec) break;
        auto sock = std::make_shared<boost::asio::ip::tcp::socket>(std::move(raw));
        boost::asio::co_spawn(m_acceptor.get_executor(),
            self->HandleSession(sock), boost::asio::detached);
    }
}

boost::asio::awaitable<void>
AdminShell::HandleSession(std::shared_ptr<boost::asio::ip::tcp::socket> sock)
{
    using namespace boost::asio;
    const std::string banner =
        "tloginsvr_asio admin shell — type 'help' for commands\n> ";
    co_await async_write(*sock, buffer(banner), use_awaitable);

    streambuf buf;
    while (sock->is_open())
    {
        boost::system::error_code ec;
        co_await async_read_until(*sock, buf, '\n',
            redirect_error(use_awaitable, ec));
        if (ec) break;

        std::istream is(&buf);
        std::string line;
        std::getline(is, line);
        line = Trim(line);
        if (line.empty()) {
            co_await async_write(*sock, buffer(std::string("> ")), use_awaitable);
            continue;
        }
        if (line == "quit" || line == "exit") break;

        const std::string reply = Dispatch(line) + "\n> ";
        co_await async_write(*sock, buffer(reply),
            redirect_error(use_awaitable, ec));
        if (ec) break;
    }
}

std::string AdminShell::Dispatch(const std::string& line)
{
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;

    if (cmd == "help")
    {
        return
            "Commands:\n"
            "  help                show this list\n"
            "  status              session count + uptime\n"
            "  kick <user_id>      close session for user_id\n"
            "  ban-ip <ip>         in-memory IP ban (not persisted)\n"
            "  unban-ip <ip>       remove in-memory IP ban\n"
            "  log-level <level>   spdlog level (trace|debug|info|warn|error|critical|off)\n"
            "  quit                close the admin connection";
    }
    if (cmd == "status")
    {
        const auto uptime = std::chrono::steady_clock::now() - m_started_at;
        const auto secs = std::chrono::duration_cast<std::chrono::seconds>(uptime).count();
        std::ostringstream os;
        os << "live_sessions=" << (m_registry ? m_registry->Count() : std::size_t{0})
           << " uptime_seconds=" << secs;
        return os.str();
    }
    if (cmd == "kick")
    {
        // Kick by user_id. Real impl would walk the registry's
        // by_user map and Close() the session. We expose this through
        // a future IConnectionRegistry::KickUser hook — for now we
        // report what would happen so operators can wire it once the
        // hook lands.
        std::int32_t uid = 0;
        in >> uid;
        if (uid == 0) return "usage: kick <user_id>";
        // TODO: extend IConnectionRegistry with a typed kick method.
        // For now operators can run a DELETE on TCURRENTUSER + wait
        // for the next session check; spdlog::info notes the request.
        spdlog::info("admin: kick requested for user_id={}", uid);
        return "kick logged (registry kick hook landing in a follow-up)";
    }
    if (cmd == "ban-ip" || cmd == "unban-ip")
    {
        std::string ip;
        in >> ip;
        if (ip.empty()) return "usage: " + cmd + " <ip>";
        spdlog::warn("admin: {} {} (in-memory ban set not yet wired through "
                     "to SociAuthService — log only)", cmd, ip);
        return cmd + " logged";
    }
    if (cmd == "log-level")
    {
        std::string lvl;
        in >> lvl;
        const auto level = ParseLevel(lvl);
        if (level == spdlog::level::n_levels)
            return "usage: log-level <trace|debug|info|warn|error|critical|off>";
        spdlog::set_level(level);
        return "log level → " + lvl;
    }
    return "unknown command: " + cmd + " — type 'help'";
}

} // namespace tloginsvr
