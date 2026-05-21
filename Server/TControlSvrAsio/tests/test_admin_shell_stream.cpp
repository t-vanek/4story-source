// F5 — stream. Drives a real telnet-style client through the admin
// shell's `subscribe registry` flow:
//   1. Open TCP socket to the shell, send "subscribe registry\n"
//   2. Read the banner the server sends back
//   3. Trigger PeerRegistry mutations from the test thread
//   4. Verify the resulting registry.* lines arrive on the client
//
// Stand up the full AdminShell + acceptor — not DispatchForTest —
// because the subscribe path is implemented in HandleSession, not
// Dispatch. Worker thread drives io_context; the test thread does
// synchronous reads/writes on the client socket.

#include "admin_shell.h"
#include "services/admin_audit_logger.h"
#include "services/fake_service_inventory.h"
#include "services/peer_registry.h"
#include "services/service_controller.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/write.hpp>

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>

namespace asio = boost::asio;
using boost::asio::ip::tcp;
using namespace std::chrono_literals;

using tcontrolsvr::AdminShell;
using tcontrolsvr::FakeServiceInventory;
using tcontrolsvr::PeerRegistry;
using tcontrolsvr::RegistryEntry;
using tcontrolsvr::ServiceInstance;

namespace {

int g_passed = 0;
int g_failed = 0;
void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}
void CheckContains(const std::string& h, const std::string& n,
                   const char* label)
{
    Check(h.find(n) != std::string::npos, label);
}

ServiceInstance MakeService(std::uint32_t sid, std::uint8_t group_id,
                            std::uint8_t type_id, std::string name)
{
    ServiceInstance s{};
    s.service_id = sid;
    s.group_id   = group_id;
    s.type_id    = type_id;
    s.server_id  = static_cast<std::uint8_t>(sid & 0xFF);
    s.name       = std::move(name);
    return s;
}

struct FakeAdminAudit : tcontrolsvr::IAdminAuditLogger
{
    void LogKick(const std::string&, const std::string&,
                 tcontrolsvr::AdminOutcome) override {}
    void LogMove(const std::string&, const std::string&,
                 std::uint8_t, std::uint16_t) override {}
    void LogTeleportTo(const std::string&, const std::string&,
                       const std::string&) override {}
    void LogBan(const std::string&, const std::string&,
                std::uint32_t, std::uint8_t, const std::string&,
                tcontrolsvr::AdminOutcome) override {}
    void LogChatBan(const std::string&, const std::string&,
                    std::uint16_t, const std::string&) override {}
    void LogAnnouncement(const std::string&, std::uint32_t,
                         const std::string&) override {}
    void LogCharMsg(const std::string&, const std::string&,
                    const std::string&) override {}
    void LogAdminAction(const std::string&, const std::string&,
                        const std::string&) override {}
    void LogAuthorityDenied(const std::string&, std::uint8_t,
                            const std::string&) override {}
};

struct FakeServiceController : tcontrolsvr::IServiceController
{
    asio::awaitable<tcontrolsvr::ServiceStatus>
    QueryStatus(const ServiceInstance&) override
    { co_return tcontrolsvr::ServiceStatus::Unknown; }
    asio::awaitable<tcontrolsvr::ControlResult>
    Start(const ServiceInstance&) override
    { co_return tcontrolsvr::ControlResult::NotSupported; }
    asio::awaitable<tcontrolsvr::ControlResult>
    Stop(const ServiceInstance&) override
    { co_return tcontrolsvr::ControlResult::NotSupported; }
};

// Read until '\n' off a blocking socket. Caller scopes the streambuf
// across calls so leftover bytes from one read aren't lost.
std::string ReadLine(tcp::socket& sock, asio::streambuf& buf)
{
    asio::read_until(sock, buf, '\n');
    std::istream is(&buf);
    std::string line;
    std::getline(is, line);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

// Loopback connect to a local port from a separate executor so the
// AdminShell io_context isn't blocked on the connect handshake.
tcp::socket ConnectTo(std::uint16_t port)
{
    asio::io_context tmp;
    tcp::socket sock(tmp);
    sock.connect({asio::ip::make_address_v4("127.0.0.1"), port});
    return sock;
}

// ---------------------------------------------------------------------------

void TestSubscribeRegistryStreamsEvents()
{
    std::printf("[stream — subscribe registry → live event lines]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(0x010204, 1, 2, "tlog-1"));
    PeerRegistry peers(inv);
    FakeServiceController ctrl;
    FakeAdminAudit audit;

    auto shell = std::make_shared<AdminShell>(
        io, "127.0.0.1", 0,
        [] { return std::size_t{0}; },
        peers, ctrl, &audit, /*router=*/nullptr,
        std::chrono::steady_clock::now());
    asio::co_spawn(io, shell->Run(), asio::detached);
    const auto port = shell->Port();

    std::thread runner([&] { io.run(); });

    auto client = ConnectTo(port);
    asio::streambuf buf;

    // Drain banner ("admin shell — type 'help'") + the empty prompt
    // line so we land on a clean position before issuing subscribe.
    asio::read_until(client, buf, '\n');
    // Consume the trailing "> " prompt without waiting for a newline.
    // The streambuf may already have it; use available() to peek.
    {
        std::istream is(&buf); std::string skip;
        std::getline(is, skip);
    }

    // Issue the subscribe command.
    const std::string cmd = "subscribe registry\n";
    asio::write(client, asio::buffer(cmd));

    // Server replies with a "# subscribed: registry ..." banner.
    const auto banner = ReadLine(client, buf);
    CheckContains(banner, "subscribed: registry",
        "server announces subscription");

    // Trigger a Register from the test thread. The subscriber
    // callback runs in this thread, formats the line, posts to the
    // io_context — worker thread writes it to our socket.
    RegistryEntry r{};
    r.service_id    = 0x010204;
    r.reported_name = "tlog-1";
    r.reported_addr = "10.0.0.99";
    r.reported_port = 2000;
    r.version       = "5.0.0";
    const auto lease = peers.Register(r);
    Check(lease > 0, "Register returned a non-zero lease");

    const auto reg_line = ReadLine(client, buf);
    CheckContains(reg_line, "registry.registered",
        "Register produced a 'registered' event line");
    CheckContains(reg_line, "sid=0x10204", "event line names the sid");
    CheckContains(reg_line, "name=tlog-1", "event line carries name");
    CheckContains(reg_line, "addr=10.0.0.99:2000",
        "event line carries addr:port");
    CheckContains(reg_line, "version=5.0.0", "event line carries version");

    // Trigger a Heartbeat — should produce a heartbeat line.
    peers.Heartbeat(0x010204, lease, 13, 100);
    const auto hb_line = ReadLine(client, buf);
    CheckContains(hb_line, "registry.heartbeat",
        "Heartbeat produced a 'heartbeat' event line");
    CheckContains(hb_line, "users=13/100",
        "heartbeat line carries cur/max user counts");

    // Trigger a Deregister — should produce a deregistered line.
    peers.Deregister(0x010204, lease);
    const auto dr_line = ReadLine(client, buf);
    CheckContains(dr_line, "registry.deregistered",
        "Deregister produced a 'deregistered' event line");

    // Close the client — server should exit the stream cleanly. Give
    // the io_context a moment to notice the closed socket so the
    // shell coroutine returns and the worker thread can join.
    client.close();
    std::this_thread::sleep_for(100ms);

    // To make io.run() return, stop the acceptor. Closing the client
    // alone won't tear down the listener — but the test just needs
    // the stream coroutine to exit; the accept loop blocks on the
    // next connection which will never come, so we manually stop.
    io.stop();
    runner.join();
}

void TestExpireStaleEmitsExpiredEvent()
{
    std::printf("[stream — ExpireStale emits 'expired' events]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 2, "tlog-x"));
    PeerRegistry peers(inv);
    FakeServiceController ctrl;
    FakeAdminAudit audit;

    auto shell = std::make_shared<AdminShell>(
        io, "127.0.0.1", 0,
        [] { return std::size_t{0}; },
        peers, ctrl, &audit, nullptr,
        std::chrono::steady_clock::now());
    asio::co_spawn(io, shell->Run(), asio::detached);
    const auto port = shell->Port();
    std::thread runner([&] { io.run(); });

    auto client = ConnectTo(port);
    asio::streambuf buf;
    asio::read_until(client, buf, '\n');
    { std::istream is(&buf); std::string skip; std::getline(is, skip); }
    asio::write(client, asio::buffer(std::string("subscribe registry\n")));
    (void)ReadLine(client, buf);  // banner

    // Register, then immediately expire with a zero-duration sweep
    // so the lease is reaped without the test waiting on real time.
    RegistryEntry r{};
    r.service_id = 1;
    r.reported_name = "tlog-x";
    peers.Register(r);
    (void)ReadLine(client, buf);  // consume the 'registered' line

    std::this_thread::sleep_for(20ms);
    const auto expired = peers.ExpireStale(1ms);
    Check(expired == 1, "ExpireStale reaped exactly one entry");

    const auto line = ReadLine(client, buf);
    CheckContains(line, "registry.expired",
        "ExpireStale produced an 'expired' event line");
    CheckContains(line, "sid=0x1", "expired event names the sid");

    client.close();
    std::this_thread::sleep_for(50ms);
    io.stop();
    runner.join();
}

} // namespace

int main()
{
    std::printf("=== tcontrolsvr_asio admin-shell stream test ===\n");
    try
    {
        TestSubscribeRegistryStreamsEvents();
        TestExpireStaleEmitsExpiredEvent();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
