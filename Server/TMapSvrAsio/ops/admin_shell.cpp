#include "admin_shell.h"

#include "metrics.h"
#include "services/channel_presence.h"
#include "services/log_peer.h"
#include "services/rate_limiter.h"
#include "services/session_registry.h"
#include "services/world_client.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <istream>
#include <sstream>
#include <string>
#include <vector>

namespace tmapsvr::ops {

namespace {

// Send a line back to the admin client. Best-effort; on error we
// just let the connection coroutine bail.
boost::asio::awaitable<void>
WriteLine(boost::asio::ip::tcp::socket& sock, std::string text)
{
    text.push_back('\n');
    boost::system::error_code ec;
    co_await boost::asio::async_write(sock,
        boost::asio::buffer(text),
        boost::asio::redirect_error(boost::asio::use_awaitable, ec));
    (void)ec;
}

// Split a line into whitespace-separated tokens. Quoting is not
// supported — admin commands take simple integer / single-word
// arguments; multi-word strings (like broadcast messages) are
// passed as one trailing argument that consumes the rest.
std::vector<std::string> Tokenize(const std::string& line)
{
    std::vector<std::string> out;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) out.push_back(std::move(tok));
    return out;
}

// Reassemble all tokens from `start` into one space-separated
// string. Used by `broadcast <msg>` where the message tail is the
// natural payload.
std::string JoinFrom(const std::vector<std::string>& v, std::size_t start)
{
    std::string out;
    for (std::size_t i = start; i < v.size(); ++i) {
        if (i > start) out.push_back(' ');
        out.append(v[i]);
    }
    return out;
}

const char* kHelpText =
    "Commands:\n"
    "  help                        Show this list.\n"
    "  status                      Session count + peer states.\n"
    "  kick <char_id> [reason]     Close the session for char_id.\n"
    "  broadcast <msg>             (stub) Push msg to all sessions.\n"
    "  peer-status                 World + log peer connectivity.\n"
    "  quit                        Close this admin connection.";

} // namespace

AdminShell::AdminShell(boost::asio::io_context& io,
                       AdminShellConfig         cfg,
                       const HandlerContext&    services)
    : m_io(io)
    , m_cfg(std::move(cfg))
    , m_services(services)
    , m_acceptor(io)
    , m_port(0)
{
    if (m_cfg.port == 0) return;   // disabled

    using boost::asio::ip::tcp;
    boost::system::error_code ec;
    const auto addr = boost::asio::ip::make_address(m_cfg.bind, ec);
    if (ec)
    {
        spdlog::warn("admin_shell: invalid bind address '{}' ({}) — "
                     "shell disabled",
            m_cfg.bind, ec.message());
        return;
    }
    tcp::endpoint ep(addr, m_cfg.port);
    m_acceptor.open(ep.protocol());
    m_acceptor.set_option(tcp::acceptor::reuse_address(true));
    m_acceptor.bind(ep);
    m_acceptor.listen();
    m_port = m_acceptor.local_endpoint().port();
    spdlog::info("admin_shell: listening on {}:{} ({})",
        m_cfg.bind, m_port,
        m_cfg.secret.empty() ? "no auth (bind alone protects)"
                             : "shared-secret auth required");
}

boost::asio::awaitable<void>
AdminShell::Run()
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

void AdminShell::StopAccepting()
{
    boost::system::error_code ignored;
    m_acceptor.close(ignored);
}

boost::asio::awaitable<void>
AdminShell::HandleConnection(boost::asio::ip::tcp::socket sock)
{
    boost::asio::streambuf buf;
    bool authed = m_cfg.secret.empty();

    co_await WriteLine(sock, "tmapsvr_asio admin shell — type 'help' for "
                             "commands, 'quit' to exit");

    while (sock.is_open())
    {
        boost::system::error_code ec;
        std::size_t n = co_await boost::asio::async_read_until(sock, buf, '\n',
            boost::asio::redirect_error(boost::asio::use_awaitable, ec));
        if (ec || n == 0) break;

        std::istream is(&buf);
        std::string line;
        std::getline(is, line);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        if (!authed)
        {
            // First line must be `auth <secret>`. Any other line
            // closes the connection silently (no oracle for
            // brute-force).
            auto tokens = Tokenize(line);
            if (tokens.size() == 2 && tokens[0] == "auth" &&
                tokens[1] == m_cfg.secret)
            {
                authed = true;
                co_await WriteLine(sock, "OK");
            }
            else
            {
                spdlog::warn("admin_shell: auth failed from {}",
                    sock.remote_endpoint(ec).address().to_string());
                co_await WriteLine(sock, "DENIED");
                break;
            }
            continue;
        }

        const bool keep_going = co_await DispatchLine(sock, line);
        if (!keep_going) break;
    }
    boost::system::error_code ignored;
    sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
    sock.close(ignored);
}

boost::asio::awaitable<bool>
AdminShell::DispatchLine(boost::asio::ip::tcp::socket& sock,
                         const std::string&            line)
{
    auto tokens = Tokenize(line);
    if (tokens.empty()) co_return true;
    const std::string& cmd = tokens[0];

    if (cmd == "help")
    {
        co_await WriteLine(sock, kHelpText);
    }
    else if (cmd == "status")
    {
        std::ostringstream os;
        os << "sessions: ";
        if (m_services.session_reg) os << m_services.session_reg->Size();
        else                        os << "(no registry)";
        os << "\npresence: ";
        if (m_services.presence)    os << m_services.presence->Size();
        else                        os << "(no presence)";
        os << "\nworld_peer: ";
        if (m_services.world_client)
            os << (m_services.world_client->IsConnected() ? "connected" : "disconnected");
        else
            os << "(not configured)";
        os << "\naudit_udp: ";
        if (m_services.log_peer)
            os << (m_services.log_peer->Enabled() ? "enabled" : "disabled");
        else
            os << "(not configured)";
        co_await WriteLine(sock, os.str());
    }
    else if (cmd == "kick" && tokens.size() >= 2)
    {
        std::uint32_t char_id = 0;
        auto [_, ec] = std::from_chars(tokens[1].data(),
            tokens[1].data() + tokens[1].size(), char_id);
        if (ec != std::errc{})
        {
            co_await WriteLine(sock, "ERR: char_id is not a number");
            co_return true;
        }
        if (!m_services.session_reg)
        {
            co_await WriteLine(sock, "ERR: no session registry");
            co_return true;
        }
        auto target = m_services.session_reg->Find(char_id);
        if (!target)
        {
            co_await WriteLine(sock, "ERR: char " + tokens[1] + " not online");
            co_return true;
        }
        const std::string reason = JoinFrom(tokens, 2);
        spdlog::info("admin_shell: kick char={} reason='{}'",
            char_id, reason.empty() ? "(none)" : reason);
        target->Close();
        co_await WriteLine(sock, "OK kicked " + tokens[1]);
    }
    else if (cmd == "broadcast")
    {
        if (tokens.size() < 2)
        {
            co_await WriteLine(sock, "ERR: broadcast requires a message");
            co_return true;
        }
        const std::string msg = JoinFrom(tokens, 1);
        // TODO: when the CT_ANNOUNCEMENT outbound encoder lands,
        // walk channel_presence and SendPacket(CS_ANNOUNCEMENT_ACK
        // | text) to every session. For now the command logs and
        // ack-stubs.
        spdlog::info("admin_shell: broadcast (stub) — msg='{}'", msg);
        co_await WriteLine(sock,
            "OK (stub) broadcast logged — packet emission lands with "
            "consolidation");
    }
    else if (cmd == "peer-status")
    {
        std::ostringstream os;
        os << "world_client: ";
        if (m_services.world_client)
            os << (m_services.world_client->IsConnected() ? "connected" : "disconnected");
        else
            os << "(not configured)";
        os << "\nlog_peer: ";
        if (m_services.log_peer)
            os << (m_services.log_peer->Enabled() ? "enabled" : "disabled");
        else
            os << "(not configured)";
        co_await WriteLine(sock, os.str());
    }
    else if (cmd == "quit" || cmd == "exit")
    {
        co_await WriteLine(sock, "bye");
        co_return false;
    }
    else
    {
        co_await WriteLine(sock,
            "ERR: unknown command '" + cmd + "' — try 'help'");
    }
    co_return true;
}

} // namespace tmapsvr::ops
