// F6 — cluster orchestration commands. Drives `cluster start/stop`,
// `cluster restart`, and `cluster wait-healthy` through DispatchForTest
// against a FakeServiceController that records Start/Stop calls, plus
// a real PeerRegistry that we mutate from the test thread to simulate
// peers coming up and going away.
//
// No DB, no real SCM — the controller is a counting stub. Wait paths
// (restart, wait-healthy) use real steady_timer polling, so we keep
// timeouts and PollIntervals short to keep test runtime under a few
// hundred ms.

#include "admin_shell.h"
#include "message_router.h"
#include "services/admin_audit_logger.h"
#include "services/fake_service_inventory.h"
#include "services/peer_registry.h"
#include "services/service_controller.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/use_future.hpp>

#include <chrono>
#include <cstdio>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace asio = boost::asio;
using namespace std::chrono_literals;

using tcontrolsvr::AdminShell;
using tcontrolsvr::FakeServiceInventory;
using tcontrolsvr::MessageRouter;
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

struct FakeAdminAudit : tcontrolsvr::IAdminAuditLogger
{
    std::vector<std::string> actions;
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
    void LogAdminAction(const std::string&,
                        const std::string& kind,
                        const std::string& target) override
    {
        actions.push_back(kind + ":" + target);
    }
    void LogAuthorityDenied(const std::string&, std::uint8_t,
                            const std::string&) override {}
};

struct CountingController : tcontrolsvr::IServiceController
{
    int start_calls = 0;
    int stop_calls  = 0;
    tcontrolsvr::ControlResult start_result = tcontrolsvr::ControlResult::Ok;
    tcontrolsvr::ControlResult stop_result  = tcontrolsvr::ControlResult::Ok;

    asio::awaitable<tcontrolsvr::ServiceStatus>
    QueryStatus(const ServiceInstance&) override
    { co_return tcontrolsvr::ServiceStatus::Unknown; }
    asio::awaitable<tcontrolsvr::ControlResult>
    Start(const ServiceInstance&) override
    {
        ++start_calls;
        co_return start_result;
    }
    asio::awaitable<tcontrolsvr::ControlResult>
    Stop(const ServiceInstance&) override
    {
        ++stop_calls;
        co_return stop_result;
    }
};

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

template <class T>
T RunOne(asio::io_context& io, asio::awaitable<T> aw)
{
    auto fut = asio::co_spawn(io, std::move(aw), asio::use_future);
    io.restart();
    io.run();
    return fut.get();
}

std::shared_ptr<AdminShell>
MakeShell(asio::io_context& io, PeerRegistry& peers,
          tcontrolsvr::IServiceController& ctrl,
          FakeAdminAudit& audit)
{
    return std::make_shared<AdminShell>(
        io, "127.0.0.1", 0,
        [] { return std::size_t{0}; },
        peers, ctrl, &audit, /*router=*/nullptr,
        std::chrono::steady_clock::now());
}

// ---------------------------------------------------------------------------

void TestClusterStartStopByType()
{
    std::printf("[cluster — start/stop iterate by type]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 4, "map-1"));
    inv.AddService(MakeService(2, 1, 4, "map-2"));
    inv.AddService(MakeService(3, 1, 1, "login-1"));   // different type
    PeerRegistry peers(inv);
    CountingController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit);

    const auto start_reply = RunOne(io,
        shell->DispatchForTest("cluster start map"));
    CheckContains(start_reply, "ok=2/2", "start hit both map peers");
    Check(ctrl.start_calls == 2, "controller.Start called twice");

    const auto stop_reply = RunOne(io,
        shell->DispatchForTest("cluster stop map"));
    CheckContains(stop_reply, "ok=2/2", "stop hit both map peers");
    Check(ctrl.stop_calls == 2, "controller.Stop called twice");

    Check(audit.actions.size() == 2,
        "two LogAdminAction records (start + stop)");
    Check(audit.actions[0] == "cluster_start:map" &&
          audit.actions[1] == "cluster_stop:map",
        "audit labels reflect both commands");
}

void TestClusterStartReportsUnsupported()
{
    std::printf("[cluster — start surfaces NotSupported counts]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 4, "map-1"));
    inv.AddService(MakeService(2, 1, 4, "map-2"));
    PeerRegistry peers(inv);
    CountingController ctrl;
    ctrl.start_result = tcontrolsvr::ControlResult::NotSupported;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit);

    const auto reply = RunOne(io,
        shell->DispatchForTest("cluster start map"));
    CheckContains(reply, "ok=0/2",        "no successful starts");
    CheckContains(reply, "unsupported=2", "both calls returned NotSupported");
}

void TestClusterStartBadTypeReportsUsage()
{
    std::printf("[cluster — bad type name shows usage]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    PeerRegistry peers(inv);
    CountingController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit);

    const auto reply = RunOne(io,
        shell->DispatchForTest("cluster start nonsensetype"));
    CheckContains(reply, "usage: cluster start", "usage on bad type name");
}

void TestClusterRestartHappyPath()
{
    std::printf("[cluster — restart Stops, waits for deregister, Starts]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 4, "map-1"));
    PeerRegistry peers(inv);

    // Pre-stage: the peer is currently registered. The wait loop in
    // 'cluster restart' polls until FindRegistration returns null.
    RegistryEntry r{};
    r.service_id    = 1;
    r.reported_name = "map-1";
    const auto lease = peers.Register(r);

    CountingController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit);

    // Schedule the deregister on the SAME io_context so it interleaves
    // with the shell's wait-loop on a single thread — no race with
    // FindRegistration() reads. Simulates the peer dialing back into
    // TControl and dropping its lease shortly after SCM Stop completes.
    asio::co_spawn(io, [&peers, lease]() -> asio::awaitable<void> {
        asio::steady_timer t(co_await asio::this_coro::executor);
        t.expires_after(50ms);
        boost::system::error_code ec;
        co_await t.async_wait(asio::redirect_error(
            asio::use_awaitable, ec));
        peers.Deregister(1, lease);
    }, asio::detached);

    const auto reply = RunOne(io,
        shell->DispatchForTest("cluster restart 1 5"));

    CheckContains(reply, "'map-1'", "reply names the restarted service");
    CheckContains(reply, "stop=ok", "Stop reported ok");
    CheckContains(reply, "start=ok", "Start reported ok");
    Check(ctrl.stop_calls == 1 && ctrl.start_calls == 1,
        "controller saw exactly one Stop + one Start");
}

void TestClusterRestartTimesOut()
{
    std::printf("[cluster — restart times out when peer never deregisters]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 4, "map-1"));
    PeerRegistry peers(inv);
    RegistryEntry r{};
    r.service_id    = 1;
    r.reported_name = "map-1";
    peers.Register(r);
    CountingController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit);

    // 1-second timeout, peer never deregisters → expect "timed out".
    const auto reply = RunOne(io,
        shell->DispatchForTest("cluster restart 1 1"));
    CheckContains(reply, "timed out", "reply states the timeout");
    Check(ctrl.stop_calls == 1, "Stop was attempted");
    Check(ctrl.start_calls == 0, "Start was NOT attempted after timeout");
}

void TestClusterRestartNotSupportedNoOp()
{
    std::printf("[cluster — restart with no SCM is a clear no-op]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 4, "map-1"));
    PeerRegistry peers(inv);
    CountingController ctrl;
    ctrl.stop_result  = tcontrolsvr::ControlResult::NotSupported;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit);

    const auto reply = RunOne(io,
        shell->DispatchForTest("cluster restart 1 10"));
    CheckContains(reply, "not-supported",
        "reply explains why nothing happened");
    Check(ctrl.start_calls == 0, "Start NOT called when SCM is disabled");
}

void TestClusterWaitHealthyAllRegistered()
{
    std::printf("[cluster — wait-healthy returns immediately when all up]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 4, "map-1"));
    inv.AddService(MakeService(2, 1, 4, "map-2"));
    PeerRegistry peers(inv);
    RegistryEntry a{}; a.service_id = 1;
    RegistryEntry b{}; b.service_id = 2;
    peers.Register(a);
    peers.Register(b);
    CountingController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit);

    const auto reply = RunOne(io,
        shell->DispatchForTest("cluster wait-healthy 5"));
    CheckContains(reply, "all services registered",
        "happy-path message");
    CheckContains(reply, "2 total", "service count surfaced");
}

void TestClusterWaitHealthyBlocksUntilLastRegisters()
{
    std::printf("[cluster — wait-healthy returns when last peer registers]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 4, "map-1"));
    inv.AddService(MakeService(2, 1, 4, "map-2"));
    PeerRegistry peers(inv);
    RegistryEntry a{}; a.service_id = 1;
    peers.Register(a);  // only one is up at start
    CountingController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit);

    // Schedule the late peer registration on the same io_context so
    // it interleaves with the wait-loop's polling on a single thread.
    asio::co_spawn(io, [&peers]() -> asio::awaitable<void> {
        asio::steady_timer t(co_await asio::this_coro::executor);
        t.expires_after(50ms);
        boost::system::error_code ec;
        co_await t.async_wait(asio::redirect_error(
            asio::use_awaitable, ec));
        RegistryEntry b{};
        b.service_id = 2;
        peers.Register(b);
    }, asio::detached);

    const auto reply = RunOne(io,
        shell->DispatchForTest("cluster wait-healthy 5"));

    CheckContains(reply, "all services registered",
        "wait completed once the late peer registered");
}

void TestClusterWaitHealthyTimeoutReportsMissing()
{
    std::printf("[cluster — wait-healthy timeout lists missing sids]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(0x010101, 1, 4, "map-1"));
    inv.AddService(MakeService(0x020201, 2, 4, "map-2"));
    PeerRegistry peers(inv);   // nothing registered
    CountingController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit);

    const auto reply = RunOne(io,
        shell->DispatchForTest("cluster wait-healthy 1"));
    CheckContains(reply, "timeout", "reply states timeout");
    CheckContains(reply, "0x10101",  "missing sid #1 listed");
    CheckContains(reply, "0x20201",  "missing sid #2 listed");
}

} // namespace

int main()
{
    std::printf("=== tcontrolsvr_asio cluster orchestration test ===\n");
    try
    {
        TestClusterStartStopByType();
        TestClusterStartReportsUnsupported();
        TestClusterStartBadTypeReportsUsage();
        TestClusterRestartHappyPath();
        TestClusterRestartTimesOut();
        TestClusterRestartNotSupportedNoOp();
        TestClusterWaitHealthyAllRegistered();
        TestClusterWaitHealthyBlocksUntilLastRegisters();
        TestClusterWaitHealthyTimeoutReportsMissing();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
