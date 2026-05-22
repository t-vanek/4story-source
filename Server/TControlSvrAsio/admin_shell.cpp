#include "admin_shell.h"

#include "message_router.h"
#include "peer_session.h"
#include "senders.h"
#include "services/admin_audit_logger.h"
#include "services/peer_registry.h"
#include "services/service_controller.h"
#include "services/svr_type.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <spdlog/spdlog.h>

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <utility>

namespace tcontrolsvr {

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

const char* StatusName(ServiceStatus s)
{
    switch (s)
    {
        case ServiceStatus::Stopped:         return "stopped";
        case ServiceStatus::StartPending:    return "start-pending";
        case ServiceStatus::StopPending:     return "stop-pending";
        case ServiceStatus::Running:         return "running";
        case ServiceStatus::ContinuePending: return "continue-pending";
        case ServiceStatus::PausePending:    return "pause-pending";
        case ServiceStatus::Paused:          return "paused";
        case ServiceStatus::NotInstalled:    return "not-installed";
        case ServiceStatus::Unknown:
        default:                             return "unknown";
    }
}

const char* TypeName(std::uint8_t type_id)
{
    using namespace svr_type;
    switch (type_id)
    {
        case kLoginSvr: return "login";
        case kLogSvr:   return "log";
        case kPatchSvr: return "patch";
        case kMapSvr:   return "map";
        case kWorldSvr: return "world";
        default:        return "other";
    }
}

const char* ResultName(ControlResult r)
{
    switch (r)
    {
        case ControlResult::Ok:           return "ok";
        case ControlResult::Failed:       return "failed";
        case ControlResult::NotSupported: return "not-supported";
    }
    return "unknown";
}

// Parse a u32 from "0x..." (hex) or plain decimal. Returns false on
// malformed input — the command handler converts that into a usage
// reply rather than crashing.
bool ParseU32(const std::string& s, std::uint32_t& out)
{
    if (s.empty()) return false;
    try
    {
        std::size_t consumed = 0;
        if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
            out = static_cast<std::uint32_t>(
                std::stoul(s.substr(2), &consumed, 16));
        else
            out = static_cast<std::uint32_t>(
                std::stoul(s, &consumed, 10));
        return consumed > 0;
    }
    catch (...) { return false; }
}

bool ParseU8(const std::string& s, std::uint8_t& out)
{
    std::uint32_t v = 0;
    if (!ParseU32(s, v) || v > 255) return false;
    out = static_cast<std::uint8_t>(v);
    return true;
}

bool ParseU16(const std::string& s, std::uint16_t& out)
{
    std::uint32_t v = 0;
    if (!ParseU32(s, v) || v > 0xFFFF) return false;
    out = static_cast<std::uint16_t>(v);
    return true;
}

// Format a registry event as a single key=value line terminated
// with '\n'. Grep-friendly and matches the existing audit-log shape.
std::string FormatRegistryEvent(const RegistryEvent& ev)
{
    std::ostringstream os;
    os << "registry." << RegistryEventKindName(ev.kind)
       << " sid=0x" << std::hex << ev.service_id << std::dec
       << " lease=" << ev.lease_epoch;
    if (!ev.reported_name.empty())
        os << " name=" << ev.reported_name;
    if (ev.kind == RegistryEventKind::Registered)
    {
        os << " addr=" << ev.reported_addr << ":" << ev.reported_port
           << " version=" << ev.version;
    }
    if (ev.kind == RegistryEventKind::Heartbeat)
    {
        os << " users=" << ev.cur_users << "/" << ev.max_users;
    }
    os << "\n";
    return os.str();
}

// Hex-string → byte vector. Accepts even-length strings of [0-9a-fA-F].
// Returns false on malformed input (odd length or non-hex char).
bool ParseHexBody(const std::string& s, std::vector<std::byte>& out)
{
    out.clear();
    if (s.size() % 2 != 0) return false;
    out.reserve(s.size() / 2);
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (std::size_t i = 0; i < s.size(); i += 2)
    {
        const int hi = nibble(s[i]);
        const int lo = nibble(s[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back(static_cast<std::byte>((hi << 4) | lo));
    }
    return true;
}

} // namespace

AdminShell::AdminShell(boost::asio::io_context& io,
                      const std::string& bind_address,
                      std::uint16_t port,
                      OperatorCountFn operator_count,
                      PeerRegistry& peers,
                      IServiceController& controller,
                      IAdminAuditLogger* audit,
                      MessageRouter* router,
                      std::chrono::steady_clock::time_point started_at)
    : m_acceptor(io)
    , m_port(port)
    , m_operator_count(std::move(operator_count))
    , m_peers(peers)
    , m_controller(controller)
    , m_audit(audit)
    , m_router(router)
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
        auto sock = std::make_shared<boost::asio::ip::tcp::socket>(
            std::move(raw));
        boost::asio::co_spawn(m_acceptor.get_executor(),
            self->HandleSession(sock), boost::asio::detached);
    }
}

boost::asio::awaitable<void>
AdminShell::HandleSession(std::shared_ptr<boost::asio::ip::tcp::socket> sock)
{
    using namespace boost::asio;
    const std::string banner =
        "tcontrolsvr admin shell — type 'help' for commands\n> ";
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
            co_await async_write(*sock, buffer(std::string("> ")),
                use_awaitable);
            continue;
        }
        if (line == "quit" || line == "exit") break;
        if (line == "subscribe registry")
        {
            // Hand the socket over to the streaming loop. Returns
            // when the client closes the connection.
            co_await RunRegistryStream(sock);
            break;
        }

        const std::string reply = (co_await Dispatch(line)) + "\n> ";
        co_await async_write(*sock, buffer(reply),
            redirect_error(use_awaitable, ec));
        if (ec) break;
    }
}

boost::asio::awaitable<void>
AdminShell::RunRegistryStream(
    std::shared_ptr<boost::asio::ip::tcp::socket> sock)
{
    using namespace boost::asio;
    auto exec = co_await this_coro::executor;

    // Per-session shared state. Lives via shared_ptr because a stale
    // event post() can outlive the coroutine by one io_context tick —
    // the shutdown flag tells the post handler not to touch the
    // timer once we've unsubscribed and started tearing down.
    struct Shared
    {
        std::deque<std::string>     queue;
        std::mutex                  mtx;
        steady_timer                wakeup;
        std::atomic<bool>           shutdown{false};
        explicit Shared(any_io_executor e) : wakeup(e) {}
    };
    auto sh = std::make_shared<Shared>(exec);
    sh->wakeup.expires_at(std::chrono::steady_clock::time_point::max());

    // Subscribe. The callback runs on the publisher's thread (almost
    // always the io_context thread). We format on the publisher side
    // — cheap — then post the result onto our executor so the writer
    // can drain it in coroutine context without touching the queue
    // mutex from arbitrary threads.
    const auto token = m_peers.Events().Subscribe(
        [sh, exec](const RegistryEvent& ev) {
            auto line = FormatRegistryEvent(ev);
            post(exec, [sh, line = std::move(line)]() mutable {
                if (sh->shutdown.load()) return;
                {
                    std::lock_guard<std::mutex> lk(sh->mtx);
                    sh->queue.push_back(std::move(line));
                }
                sh->wakeup.cancel();
            });
        });

    // Banner so the operator sees the stream is live before any
    // peer-driven event arrives.
    {
        const std::string banner =
            "# subscribed: registry — close connection to exit\n";
        boost::system::error_code ec;
        co_await async_write(*sock, buffer(banner),
            redirect_error(use_awaitable, ec));
        if (ec)
        {
            sh->shutdown.store(true);
            m_peers.Events().Unsubscribe(token);
            co_return;
        }
    }

    while (sock->is_open())
    {
        // Drain whatever's queued. Snapshot under lock; write under
        // no lock so a publisher posting concurrently isn't blocked
        // on the socket write.
        std::deque<std::string> drained;
        {
            std::lock_guard<std::mutex> lk(sh->mtx);
            drained.swap(sh->queue);
        }
        bool dead = false;
        for (auto& line : drained)
        {
            boost::system::error_code ec;
            co_await async_write(*sock, buffer(line),
                redirect_error(use_awaitable, ec));
            if (ec) { dead = true; break; }
        }
        if (dead) break;

        // Wait for the next event or a 30s keepalive tick. The
        // keepalive is what tells us the client went away — without
        // it we could sit idle for hours not noticing a dead socket.
        sh->wakeup.expires_after(std::chrono::seconds(30));
        boost::system::error_code ec;
        co_await sh->wakeup.async_wait(
            redirect_error(use_awaitable, ec));
        // ec == operation_aborted is the canceled-by-publisher path;
        // empty ec means the 30s tick fired with no events.
        if (!ec)
        {
            const std::string ping = "# alive\n";
            co_await async_write(*sock, buffer(ping),
                redirect_error(use_awaitable, ec));
            if (ec) break;
        }
    }

    sh->shutdown.store(true);
    m_peers.Events().Unsubscribe(token);
}

boost::asio::awaitable<std::string>
AdminShell::DispatchForTest(const std::string& line)
{
    co_return co_await Dispatch(line);
}

boost::asio::awaitable<std::string>
AdminShell::Dispatch(const std::string& line)
{
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;

    if (cmd == "help")
    {
        co_return
            "Commands:\n"
            "  help                          show this list\n"
            "  status                        operators + peers + uptime\n"
            "  peers                         service inventory + dial state\n"
            "  peer <service_id>             one-service detail (static + runtime + registry)\n"
            "  registry                      live peer self-registrations + heartbeats\n"
            "  discover <group> <type>       list live peers of (group,type) — same data\n"
            "                                CT_PEER_DISCOVER_REQ returns; group=0 = any\n"
            "  kick <user>                   broadcast UserKickout to all map peers\n"
            "  announce <message...>         broadcast announcement\n"
            "  service status <service_id>   query peer service status (SCM)\n"
            "  service start  <service_id>   SCM start on peer machine\n"
            "  service stop   <service_id>   SCM stop  on peer machine\n"
            "  route service <sid> <wId> [hex-body]\n"
            "  route type <group> <type> <wId> [hex-body]\n"
            "  route broadcast type <type> <wId> [hex-body]\n"
            "  route broadcast group <group> <type> <wId> [hex-body]\n"
            "                                send raw CT_* frames to peers\n"
            "  subscribe registry            stream registry events as key=value lines\n"
            "                                (registered/heartbeat/deregistered/expired) —\n"
            "                                close the connection to exit\n"
            "  cluster start <type>          SCM Start every peer of <type> (login|log|patch|map|world)\n"
            "  cluster stop <type>           SCM Stop every peer of <type>\n"
            "  cluster restart <sid> [timeout=120]\n"
            "                                Stop, wait for lease expiry, then Start\n"
            "  cluster wait-healthy [timeout=60]\n"
            "                                block until every static service has a live\n"
            "                                registration; reply lists missing services on timeout\n"
            "  log-level <level>             local spdlog level\n"
            "  quit                          close the admin connection";
    }
    if (cmd == "status")
        co_return CmdStatus();
    if (cmd == "peers")
        co_return CmdPeers();
    if (cmd == "registry")
        co_return CmdRegistry();
    if (cmd == "discover")
    {
        std::string group_s, type_s;
        in >> group_s >> type_s;
        std::uint32_t group = 0, type = 0;
        if (!ParseU32(group_s, group) || !ParseU32(type_s, type) ||
            group > 0xFF || type > 0xFF)
        {
            co_return "usage: discover <group> <type>  "
                      "(group=0 means any; type is 1..5)";
        }
        co_return CmdDiscover(static_cast<std::uint8_t>(group),
                              static_cast<std::uint8_t>(type));
    }
    if (cmd == "peer")
    {
        std::string sid_s;
        in >> sid_s;
        std::uint32_t sid = 0;
        if (!ParseU32(sid_s, sid))
            co_return "usage: peer <service_id>  (decimal or 0x-hex)";
        co_return CmdPeer(sid);
    }
    if (cmd == "route")
    {
        std::string rest;
        std::getline(in, rest);
        co_return co_await CmdRoute(rest);
    }
    if (cmd == "cluster")
    {
        std::string rest;
        std::getline(in, rest);
        co_return co_await CmdCluster(rest);
    }
    if (cmd == "kick")
    {
        std::string user;
        in >> user;
        if (user.empty()) co_return "usage: kick <user_id>";
        // The user-id field on the wire is a string (CHARACTER name in
        // the legacy protocol, not the dwUserID); pass through verbatim.
        co_return CmdKick(0, user);
    }
    if (cmd == "announce")
    {
        std::string rest;
        std::getline(in, rest);
        rest = Trim(rest);
        if (rest.empty()) co_return "usage: announce <message...>";
        co_return CmdAnnounce(rest);
    }
    if (cmd == "service")
    {
        std::string sub;
        in >> sub;
        std::uint32_t sid = 0;
        in >> sid;
        if (sub.empty() || sid == 0)
            co_return "usage: service <status|start|stop> <service_id>";
        if (sub == "status") co_return co_await CmdServiceStatus(sid);
        if (sub == "start")  co_return co_await CmdServiceStart(sid);
        if (sub == "stop")   co_return co_await CmdServiceStop(sid);
        co_return "unknown service subcommand: " + sub;
    }
    if (cmd == "log-level")
    {
        std::string lvl;
        in >> lvl;
        co_return CmdLogLevel(lvl);
    }
    co_return "unknown command: " + cmd + " — type 'help'";
}

std::string AdminShell::CmdStatus() const
{
    const auto uptime = std::chrono::steady_clock::now() - m_started_at;
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(
                          uptime).count();

    std::size_t peers_total = 0;
    std::size_t peers_live  = 0;
    for (const auto& svc : m_peers.Services())
    {
        ++peers_total;
        auto conn = m_peers.Connection(svc.service_id);
        if (conn && conn->Wire() && conn->Wire()->IsOpen())
            ++peers_live;
    }
    std::ostringstream os;
    os << "operators=" << (m_operator_count ? m_operator_count()
                                            : std::size_t{0})
       << " peers=" << peers_live << "/" << peers_total
       << " uptime_seconds=" << secs;
    return os.str();
}

std::string AdminShell::CmdPeers() const
{
    std::ostringstream os;
    const auto& services = m_peers.Services();
    if (services.empty())
        return "(no services registered)";
    os << "service_id type     name                              status         conn";
    for (const auto& svc : services)
    {
        const auto* st = m_peers.Status(svc.service_id);
        auto conn = m_peers.Connection(svc.service_id);
        const bool live = conn && conn->Wire() && conn->Wire()->IsOpen();
        const auto svc_status = st ? st->status : ServiceStatus::Unknown;
        os << "\n"
           << svc.service_id << "       "
           << TypeName(svc.type_id);
        // pad type name to 8 cols (login/log/patch/map/world all <=5)
        for (std::size_t i = std::string(TypeName(svc.type_id)).size();
             i < 8; ++i) os << ' ';
        os << " " << svc.name;
        for (std::size_t i = svc.name.size(); i < 33; ++i) os << ' ';
        os << " " << StatusName(svc_status);
        for (std::size_t i = std::string(StatusName(svc_status)).size();
             i < 14; ++i) os << ' ';
        os << " " << (live ? "online" : "offline");
    }
    return os.str();
}

std::string AdminShell::CmdRegistry() const
{
    const auto entries = m_peers.Registry();
    if (entries.empty())
        return "(no peers registered)";
    std::ostringstream os;
    os << "service_id  name                  addr                  "
          "version          lease  age_s  users";
    const auto now = std::chrono::steady_clock::now();
    for (const auto& e : entries)
    {
        const auto age = std::chrono::duration_cast<std::chrono::seconds>(
            now - e.last_heartbeat_at).count();
        os << "\n"
           << e.service_id << "    "
           << e.reported_name;
        for (std::size_t i = e.reported_name.size(); i < 22; ++i) os << ' ';
        std::ostringstream addr;
        addr << e.reported_addr << ":" << e.reported_port;
        os << addr.str();
        for (std::size_t i = addr.str().size(); i < 22; ++i) os << ' ';
        os << e.version;
        for (std::size_t i = e.version.size(); i < 17; ++i) os << ' ';
        os << e.lease_epoch << "      "
           << age << "s     "
           << e.cur_users << "/" << e.max_users;
    }
    return os.str();
}

std::string AdminShell::CmdDiscover(std::uint8_t group_id,
                                    std::uint8_t type_id) const
{
    const auto live = m_peers.FindLiveByType(group_id, type_id);
    if (live.empty())
    {
        std::ostringstream os;
        os << "(no live peers of group=" << static_cast<int>(group_id)
           << " type="  << static_cast<int>(type_id) << ")";
        return os.str();
    }
    std::ostringstream os;
    os << "service_id  name                  addr";
    for (const auto& e : live)
    {
        os << "\n" << e.service_id << "    " << e.reported_name;
        for (std::size_t i = e.reported_name.size(); i < 22; ++i) os << ' ';
        os << e.reported_addr << ":" << e.reported_port;
    }
    return os.str();
}

std::string AdminShell::CmdKick(std::uint32_t /*uid*/,
                                const std::string& user)
{
    std::size_t fanout = 0;
    for (const auto& svc : m_peers.Services())
    {
        if (svc.type_id != svr_type::kMapSvr) continue;
        auto conn = m_peers.Connection(svc.service_id);
        if (!conn || !conn->Wire() || !conn->Wire()->IsOpen()) continue;
        boost::asio::co_spawn(m_acceptor.get_executor(),
            senders::SendUserKickoutAck(conn->Wire(), user),
            boost::asio::detached);
        ++fanout;
    }
    if (m_audit)
        m_audit->LogKick("admin-shell", user,
            fanout > 0 ? AdminOutcome::Success : AdminOutcome::Failed);
    std::ostringstream os;
    os << "kick " << user << " → " << fanout << " map peer(s)";
    return os.str();
}

std::string AdminShell::CmdAnnounce(const std::string& msg)
{
    std::size_t fanout = 0;
    for (const auto& svc : m_peers.Services())
    {
        if (svc.type_id != svr_type::kMapSvr &&
            svc.type_id != svr_type::kWorldSvr) continue;
        auto conn = m_peers.Connection(svc.service_id);
        if (!conn || !conn->Wire() || !conn->Wire()->IsOpen()) continue;
        boost::asio::co_spawn(m_acceptor.get_executor(),
            senders::SendAnnouncementAck(conn->Wire(), msg),
            boost::asio::detached);
        ++fanout;
    }
    if (m_audit)
        m_audit->LogAnnouncement("admin-shell",
            /*world_filter=*/0, msg);
    std::ostringstream os;
    os << "announce → " << fanout << " peer(s)";
    return os.str();
}

boost::asio::awaitable<std::string>
AdminShell::CmdServiceStatus(std::uint32_t sid)
{
    const auto* svc = m_peers.FindService(sid);
    if (!svc) co_return "service_id " + std::to_string(sid) + " not found";
    const auto live = co_await m_controller.QueryStatus(*svc);
    const auto* st = m_peers.Status(sid);
    std::ostringstream os;
    os << "service " << sid << " '" << svc->name << "'"
       << " live=" << StatusName(live);
    if (st)
        os << " cached=" << StatusName(st->status)
           << " cur_users=" << st->cur_users
           << " max_users=" << st->max_users
           << " stop_count=" << st->stop_count;
    co_return os.str();
}

boost::asio::awaitable<std::string>
AdminShell::CmdServiceStart(std::uint32_t sid)
{
    const auto* svc = m_peers.FindService(sid);
    if (!svc) co_return "service_id " + std::to_string(sid) + " not found";
    const auto rc = co_await m_controller.Start(*svc);
    if (m_audit)
        m_audit->LogAdminAction("admin-shell", "service_start", svc->name);
    co_return std::string("service start '") + svc->name + "' → "
        + ResultName(rc);
}

boost::asio::awaitable<std::string>
AdminShell::CmdServiceStop(std::uint32_t sid)
{
    const auto* svc = m_peers.FindService(sid);
    if (!svc) co_return "service_id " + std::to_string(sid) + " not found";
    const auto rc = co_await m_controller.Stop(*svc);
    if (m_audit)
        m_audit->LogAdminAction("admin-shell", "service_stop", svc->name);
    co_return std::string("service stop '") + svc->name + "' → "
        + ResultName(rc);
}

std::string AdminShell::CmdPeer(std::uint32_t sid) const
{
    const auto* svc  = m_peers.FindService(sid);
    const auto* st   = m_peers.Status(sid);
    const auto* reg  = m_peers.FindRegistration(sid);
    auto        conn = m_peers.Connection(sid);
    if (!svc && !reg)
        return "service_id " + std::to_string(sid) + " not found "
               "(neither in static inventory nor dynamic registry)";
    std::ostringstream os;
    os << "service_id=" << sid;
    if (svc)
    {
        os << "\nstatic:    name='" << svc->name
           << "' type=" << TypeName(svc->type_id)
           << " group=" << static_cast<int>(svc->group_id)
           << " server=" << static_cast<int>(svc->server_id)
           << " port=" << svc->port;
    }
    if (st)
    {
        os << "\nruntime:   status=" << StatusName(st->status)
           << " cur_users=" << st->cur_users
           << " max_users=" << st->max_users
           << " stop_count=" << st->stop_count;
    }
    if (reg)
    {
        const auto age = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - reg->last_heartbeat_at).count();
        os << "\nregistry:  reported='" << reg->reported_name
           << "' addr=" << reg->reported_addr << ":" << reg->reported_port
           << " version=" << reg->version
           << " lease=" << reg->lease_epoch
           << " hb_age=" << age << "s"
           << " pid=" << reg->pid;
    }
    const bool live = conn && conn->Wire() && conn->Wire()->IsOpen();
    os << "\nconn:      " << (live ? "online" : "offline");
    return os.str();
}

boost::asio::awaitable<std::string>
AdminShell::CmdRoute(const std::string& rest)
{
    if (!m_router)
        co_return "route: MessageRouter unavailable";

    std::istringstream in(rest);
    std::string sub;
    in >> sub;

    auto need_router_or_usage =
        [](const std::string& usage) { return usage; };

    if (sub == "service")
    {
        std::string sid_s, wid_s, body_s;
        in >> sid_s >> wid_s >> body_s;
        std::uint32_t sid = 0;
        std::uint16_t wid = 0;
        std::vector<std::byte> body;
        if (!ParseU32(sid_s, sid) || !ParseU16(wid_s, wid))
            co_return "usage: route service <sid> <wId> [hex-body]";
        if (!body_s.empty() && !ParseHexBody(body_s, body))
            co_return "route service: malformed hex body";
        const bool ok = co_await m_router->SendToService(sid, wid,
            std::move(body));
        if (m_audit)
            m_audit->LogAdminAction("admin-shell", "route_service",
                std::to_string(sid));
        co_return ok
            ? "route service " + std::to_string(sid) + " → ok"
            : "route service " + std::to_string(sid) + " → offline / unknown";
    }
    if (sub == "type")
    {
        std::string g_s, t_s, wid_s, body_s;
        in >> g_s >> t_s >> wid_s >> body_s;
        std::uint8_t  g = 0, t = 0;
        std::uint16_t wid = 0;
        std::vector<std::byte> body;
        if (!ParseU8(g_s, g) || !ParseU8(t_s, t) || !ParseU16(wid_s, wid))
            co_return "usage: route type <group> <type> <wId> [hex-body]";
        if (!body_s.empty() && !ParseHexBody(body_s, body))
            co_return "route type: malformed hex body";
        const auto sid = co_await m_router->SendToType(g, t, wid,
            std::move(body));
        if (m_audit)
            m_audit->LogAdminAction("admin-shell", "route_type",
                std::to_string(g) + "/" + std::to_string(t));
        co_return sid == 0
            ? "route type → no live peers in bucket"
            : "route type → service_id " + std::to_string(sid);
    }
    if (sub == "broadcast")
    {
        std::string scope;
        in >> scope;
        if (scope == "type")
        {
            std::string t_s, wid_s, body_s;
            in >> t_s >> wid_s >> body_s;
            std::uint8_t  t = 0;
            std::uint16_t wid = 0;
            std::vector<std::byte> body;
            if (!ParseU8(t_s, t) || !ParseU16(wid_s, wid))
                co_return "usage: route broadcast type <type> <wId> [hex-body]";
            if (!body_s.empty() && !ParseHexBody(body_s, body))
                co_return "route broadcast type: malformed hex body";
            const auto fanout = co_await m_router->BroadcastToType(t, wid,
                std::move(body));
            if (m_audit)
                m_audit->LogAdminAction("admin-shell", "route_broadcast_type",
                    std::to_string(t));
            co_return "route broadcast type → " + std::to_string(fanout)
                + " peer(s)";
        }
        if (scope == "group")
        {
            std::string g_s, t_s, wid_s, body_s;
            in >> g_s >> t_s >> wid_s >> body_s;
            std::uint8_t  g = 0, t = 0;
            std::uint16_t wid = 0;
            std::vector<std::byte> body;
            if (!ParseU8(g_s, g) || !ParseU8(t_s, t) || !ParseU16(wid_s, wid))
                co_return "usage: route broadcast group <group> <type> "
                          "<wId> [hex-body]";
            if (!body_s.empty() && !ParseHexBody(body_s, body))
                co_return "route broadcast group: malformed hex body";
            const auto fanout = co_await m_router->BroadcastToGroupType(g, t,
                wid, std::move(body));
            if (m_audit)
                m_audit->LogAdminAction("admin-shell",
                    "route_broadcast_group",
                    std::to_string(g) + "/" + std::to_string(t));
            co_return "route broadcast group → " + std::to_string(fanout)
                + " peer(s)";
        }
        co_return "usage: route broadcast <type|group> ...";
    }
    co_return "usage: route <service|type|broadcast> ...";
}

namespace {
// Map a human-readable type tag to the service-type byte the registry
// + handlers use. Same numbering as services/svr_type.h.
bool ParseTypeName(const std::string& s, std::uint8_t& out)
{
    if      (s == "login") { out = 1; return true; }
    else if (s == "log")   { out = 2; return true; }
    else if (s == "patch") { out = 3; return true; }
    else if (s == "map")   { out = 4; return true; }
    else if (s == "world") { out = 5; return true; }
    std::uint8_t n = 0;
    if (ParseU8(s, n)) { out = n; return true; }
    return false;
}
} // namespace

boost::asio::awaitable<std::string>
AdminShell::CmdCluster(const std::string& rest)
{
    std::istringstream in(rest);
    std::string sub;
    in >> sub;

    if (sub == "start" || sub == "stop")
    {
        std::string type_s;
        in >> type_s;
        std::uint8_t type_id = 0;
        if (!ParseTypeName(type_s, type_id))
            co_return "usage: cluster " + sub +
                " <type>  (login|log|patch|map|world or 0..255)";
        std::size_t ok = 0, tried = 0, unsupported = 0;
        for (const auto& svc : m_peers.Services())
        {
            if (svc.type_id != type_id) continue;
            ++tried;
            const auto rc = sub == "start"
                ? co_await m_controller.Start(svc)
                : co_await m_controller.Stop(svc);
            if (rc == ControlResult::Ok)            ++ok;
            else if (rc == ControlResult::NotSupported) ++unsupported;
        }
        if (m_audit)
            m_audit->LogAdminAction("admin-shell",
                "cluster_" + sub, type_s);
        std::ostringstream os;
        os << "cluster " << sub << " " << type_s
           << " → ok=" << ok << "/" << tried;
        if (unsupported > 0)
            os << " unsupported=" << unsupported;
        co_return os.str();
    }
    if (sub == "restart")
    {
        std::string sid_s, timeout_s;
        in >> sid_s >> timeout_s;
        std::uint32_t sid = 0;
        if (!ParseU32(sid_s, sid))
            co_return "usage: cluster restart <sid> [timeout=120]";
        std::uint32_t timeout_sec = 120;
        if (!timeout_s.empty() && !ParseU32(timeout_s, timeout_sec))
            co_return "usage: cluster restart <sid> [timeout=120]";

        const auto* svc = m_peers.FindService(sid);
        if (!svc)
            co_return "service_id " + std::to_string(sid) + " not found";

        const auto stop_rc = co_await m_controller.Stop(*svc);
        if (stop_rc == ControlResult::NotSupported)
            co_return "cluster restart: SCM not-supported "
                      "(DisabledServiceController) — no-op";
        if (stop_rc != ControlResult::Ok)
            co_return "cluster restart: Stop failed — aborting";

        // Wait for the registry entry to drop. Either the peer's
        // own DEREGISTER beats the lease-expiry sweep, or the sweep
        // reaps it after ~90s. Either way, FindRegistration() ==
        // nullptr is our "peer is fully down" signal.
        auto exec = co_await boost::asio::this_coro::executor;
        boost::asio::steady_timer poll(exec);
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::seconds(timeout_sec);
        while (m_peers.FindRegistration(sid) != nullptr)
        {
            if (std::chrono::steady_clock::now() >= deadline)
                co_return "cluster restart: timed out waiting for "
                          "peer to deregister";
            poll.expires_after(std::chrono::milliseconds(250));
            boost::system::error_code ec;
            co_await poll.async_wait(boost::asio::redirect_error(
                boost::asio::use_awaitable, ec));
        }

        const auto start_rc = co_await m_controller.Start(*svc);
        if (m_audit)
            m_audit->LogAdminAction("admin-shell", "cluster_restart",
                svc->name);
        co_return std::string("cluster restart '") + svc->name +
            "' → stop=ok start=" + ResultName(start_rc);
    }
    if (sub == "wait-healthy")
    {
        std::string timeout_s;
        in >> timeout_s;
        std::uint32_t timeout_sec = 60;
        if (!timeout_s.empty() && !ParseU32(timeout_s, timeout_sec))
            co_return "usage: cluster wait-healthy [timeout=60]";

        auto missing_ids = [&]() {
            std::vector<std::uint32_t> ms;
            for (const auto& svc : m_peers.Services())
                if (m_peers.FindRegistration(svc.service_id) == nullptr)
                    ms.push_back(svc.service_id);
            return ms;
        };

        auto exec = co_await boost::asio::this_coro::executor;
        boost::asio::steady_timer poll(exec);
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::seconds(timeout_sec);
        while (true)
        {
            const auto ms = missing_ids();
            if (ms.empty())
            {
                if (m_audit)
                    m_audit->LogAdminAction("admin-shell",
                        "cluster_wait_healthy", "ok");
                co_return "cluster wait-healthy → all services registered ("
                    + std::to_string(m_peers.Services().size())
                    + " total)";
            }
            if (std::chrono::steady_clock::now() >= deadline)
            {
                std::ostringstream os;
                os << "cluster wait-healthy → timeout, missing:";
                for (auto id : ms)
                {
                    os << " 0x" << std::hex << id << std::dec;
                }
                co_return os.str();
            }
            poll.expires_after(std::chrono::milliseconds(250));
            boost::system::error_code ec;
            co_await poll.async_wait(boost::asio::redirect_error(
                boost::asio::use_awaitable, ec));
        }
    }
    co_return "usage: cluster <start|stop|restart|wait-healthy> ...";
}

std::string AdminShell::CmdLogLevel(const std::string& lvl)
{
    const auto level = ParseLevel(lvl);
    if (level == spdlog::level::n_levels)
        return "usage: log-level <trace|debug|info|warn|error|critical|off>";
    spdlog::set_level(level);
    return "log level → " + lvl;
}

} // namespace tcontrolsvr
