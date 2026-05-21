// Reconciliation-loop coverage. Drives ControlServer::
// ReconcileScmStatusOnce against a programmable FakeController that
// returns canned per-service statuses, then verifies:
//   - cached PeerRegistry::Status(sid).status is updated on every
//     observed transition
//   - exactly one ScmStatusChanged event is published per transition
//   - no event is published when the live read matches the cache
//   - the event carries the right prev/new status pair + sid + name
//   - the reconcile call returns the count of transitions
//
// No DB, no real SCM/systemd. ControlServer is built with the
// minimal config needed for the loop to read peers + controller —
// everything else stays null.

#include "control_server.h"
#include "services/fake_service_inventory.h"
#include "services/peer_registry.h"
#include "services/registry_event_bus.h"
#include "services/service_controller.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <cstdio>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace asio = boost::asio;

using tcontrolsvr::ControlResult;
using tcontrolsvr::ControlServer;
using tcontrolsvr::ControlServerConfig;
using tcontrolsvr::FakeServiceInventory;
using tcontrolsvr::IServiceController;
using tcontrolsvr::PeerRegistry;
using tcontrolsvr::RegistryEvent;
using tcontrolsvr::RegistryEventKind;
using tcontrolsvr::ServiceInstance;
using tcontrolsvr::ServiceStatus;

namespace {

int g_passed = 0;
int g_failed = 0;
void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

ServiceInstance MakeService(std::uint32_t sid, std::string name)
{
    ServiceInstance s{};
    s.service_id = sid;
    s.type_id    = 4;
    s.group_id   = 1;
    s.server_id  = static_cast<std::uint8_t>(sid & 0xFF);
    s.name       = std::move(name);
    return s;
}

// Programmable controller — Tests set the canned status per sid,
// then call ReconcileScmStatusOnce. Counts QueryStatus calls so we
// can assert the loop didn't skip a service.
struct CannedController final : IServiceController
{
    std::unordered_map<std::uint32_t, ServiceStatus> values;
    int query_calls = 0;

    asio::awaitable<ServiceStatus>
    QueryStatus(const ServiceInstance& svc) override
    {
        ++query_calls;
        auto it = values.find(svc.service_id);
        co_return it == values.end() ? ServiceStatus::Unknown : it->second;
    }
    asio::awaitable<ControlResult>
    Start(const ServiceInstance&) override
    { co_return ControlResult::NotSupported; }
    asio::awaitable<ControlResult>
    Stop(const ServiceInstance&) override
    { co_return ControlResult::NotSupported; }
};

// Subscribe to the bus + record every received event.
struct EventCollector
{
    std::vector<RegistryEvent> events;
    std::uint64_t              token = 0;
    explicit EventCollector(tcontrolsvr::RegistryEventBus& bus)
    {
        token = bus.Subscribe(
            [this](const RegistryEvent& e) { events.push_back(e); });
    }
};

template <class T>
T RunOne(asio::io_context& io, asio::awaitable<T> aw)
{
    auto fut = asio::co_spawn(io, std::move(aw), asio::use_future);
    io.restart();
    io.run();
    return fut.get();
}

// Build a minimal ControlServer with peers + controller wired and
// the listener bound to an ephemeral port (the bind is required —
// ControlServer's ctor opens m_acceptor unconditionally — but no
// accepts are issued in these tests).
std::unique_ptr<ControlServer>
MakeServer(asio::io_context& io, PeerRegistry& peers,
           IServiceController& ctrl)
{
    ControlServerConfig cfg{};
    cfg.port       = 0;
    cfg.peers      = &peers;
    cfg.controller = &ctrl;
    return std::make_unique<ControlServer>(io, cfg);
}

// ---------------------------------------------------------------------------

void TestReconcileUpdatesCacheAndPublishesEvent()
{
    std::printf("[reconcile — cache update + ScmStatusChanged publish]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, "map-1"));
    PeerRegistry peers(inv);
    CannedController ctrl;
    ctrl.values[1] = ServiceStatus::Running;
    EventCollector ec(peers.Events());
    auto server = MakeServer(io, peers, ctrl);

    const auto n = RunOne(io, server->ReconcileScmStatusOnce());
    Check(n == 1, "one transition observed (Unknown → Running)");
    Check(ctrl.query_calls == 1, "controller.QueryStatus called once");
    Check(peers.Status(1)->status == ServiceStatus::Running,
        "cached RuntimeStatus.status updated to Running");
    Check(ec.events.size() == 1, "exactly one event published");
    const auto& ev = ec.events.front();
    Check(ev.kind == RegistryEventKind::ScmStatusChanged,
        "event kind = ScmStatusChanged");
    Check(ev.service_id == 1,                "event.service_id");
    Check(ev.reported_name == "map-1",       "event.reported_name");
    Check(ev.service_status_prev == ServiceStatus::Unknown,
        "event.prev = Unknown (initial cache)");
    Check(ev.service_status == ServiceStatus::Running,
        "event.status = Running (live read)");
}

void TestReconcileWithNoChangePublishesNothing()
{
    std::printf("[reconcile — no transition → no event]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, "map-1"));
    PeerRegistry peers(inv);
    // Pre-set cached status so the live read matches.
    peers.Status(1)->status = ServiceStatus::Running;
    CannedController ctrl;
    ctrl.values[1] = ServiceStatus::Running;
    EventCollector ec(peers.Events());
    auto server = MakeServer(io, peers, ctrl);

    const auto n = RunOne(io, server->ReconcileScmStatusOnce());
    Check(n == 0, "no transitions");
    Check(ec.events.empty(),
        "no ScmStatusChanged events published when state matches");
}

void TestReconcileTracksMultipleServices()
{
    std::printf("[reconcile — multi-service tick visits everything]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, "map-1"));
    inv.AddService(MakeService(2, "map-2"));
    inv.AddService(MakeService(3, "map-3"));
    PeerRegistry peers(inv);
    // 1: stays Unknown→Stopped, 2: stays Unknown→Running,
    // 3: cache pre-set to Running, live=Running (no transition)
    peers.Status(3)->status = ServiceStatus::Running;
    CannedController ctrl;
    ctrl.values[1] = ServiceStatus::Stopped;
    ctrl.values[2] = ServiceStatus::Running;
    ctrl.values[3] = ServiceStatus::Running;
    EventCollector ec(peers.Events());
    auto server = MakeServer(io, peers, ctrl);

    const auto n = RunOne(io, server->ReconcileScmStatusOnce());
    Check(ctrl.query_calls == 3, "all three services queried");
    Check(n == 2, "exactly two transitions (svc 1 + svc 2)");
    Check(ec.events.size() == 2, "two events published");
    // Order is inventory order; can't guarantee strict ordering of
    // the events array, just that both kinds + sids are present.
    bool saw_1 = false, saw_2 = false;
    for (const auto& ev : ec.events)
    {
        if (ev.service_id == 1 &&
            ev.service_status == ServiceStatus::Stopped) saw_1 = true;
        if (ev.service_id == 2 &&
            ev.service_status == ServiceStatus::Running) saw_2 = true;
    }
    Check(saw_1, "event for svc-1 (→ Stopped)");
    Check(saw_2, "event for svc-2 (→ Running)");
}

void TestReconcileEventCarriesPrevAndNext()
{
    std::printf("[reconcile — prev/new pair captured on flip]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, "map-1"));
    PeerRegistry peers(inv);
    peers.Status(1)->status = ServiceStatus::Running;
    CannedController ctrl;
    ctrl.values[1] = ServiceStatus::Stopped;   // Running → Stopped
    EventCollector ec(peers.Events());
    auto server = MakeServer(io, peers, ctrl);

    (void)RunOne(io, server->ReconcileScmStatusOnce());
    Check(ec.events.size() == 1, "one event");
    Check(ec.events[0].service_status_prev == ServiceStatus::Running,
        "prev = Running");
    Check(ec.events[0].service_status == ServiceStatus::Stopped,
        "new = Stopped");
}

void TestReconcileNoServicesIsNoOp()
{
    std::printf("[reconcile — empty inventory returns 0 with no events]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    PeerRegistry peers(inv);
    CannedController ctrl;
    EventCollector ec(peers.Events());
    auto server = MakeServer(io, peers, ctrl);

    const auto n = RunOne(io, server->ReconcileScmStatusOnce());
    Check(n == 0, "no services → 0 transitions");
    Check(ctrl.query_calls == 0, "controller not consulted");
    Check(ec.events.empty(), "no events");
}

void TestReconcileSecondTickHasNoTransitionsIfStable()
{
    std::printf("[reconcile — second tick is idempotent when stable]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, "map-1"));
    PeerRegistry peers(inv);
    CannedController ctrl;
    ctrl.values[1] = ServiceStatus::Running;
    EventCollector ec(peers.Events());
    auto server = MakeServer(io, peers, ctrl);

    const auto n1 = RunOne(io, server->ReconcileScmStatusOnce());
    const auto n2 = RunOne(io, server->ReconcileScmStatusOnce());
    Check(n1 == 1, "first tick: Unknown → Running");
    Check(n2 == 0, "second tick: no transition");
    Check(ec.events.size() == 1, "exactly one event across both ticks");
}

} // namespace

int main()
{
    std::printf("=== tcontrolsvr_asio scm-status reconcile test ===\n");
    try
    {
        TestReconcileUpdatesCacheAndPublishesEvent();
        TestReconcileWithNoChangePublishesNothing();
        TestReconcileTracksMultipleServices();
        TestReconcileEventCarriesPrevAndNext();
        TestReconcileNoServicesIsNoOp();
        TestReconcileSecondTickHasNoTransitionsIfStable();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
