// Unit-level coverage of the persistence wiring on PeerRegistry's
// side. A FakeRegistryPersistence records every Upsert/Touch/Remove
// call so the test can assert that:
//   - Register publishes an Upsert with the right snapshot
//   - Heartbeat publishes a Touch with current users + heartbeat_unix
//   - Deregister + ExpireStale publish a Remove
//   - Hydrate() repopulates the in-RAM map AND advances the epoch
//     counter past every stored lease, so a subsequent Register
//     can't collide with a resurrected entry
//
// The SOCI impl is integration-tested separately against a live DB
// (env-gated, same pattern as TLogSvrAsio/TPatchSvrAsio).

#include "services/fake_service_inventory.h"
#include "services/peer_registry.h"
#include "services/registry_persistence.h"

#include <cstdio>
#include <string>
#include <vector>

using tcontrolsvr::FakeServiceInventory;
using tcontrolsvr::IRegistryPersistence;
using tcontrolsvr::NoopRegistryPersistence;
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

ServiceInstance MakeService(std::uint32_t sid, std::string name)
{
    ServiceInstance s{};
    s.service_id = sid;
    s.type_id    = 1;
    s.group_id   = 1;
    s.server_id  = static_cast<std::uint8_t>(sid & 0xFF);
    s.name       = std::move(name);
    return s;
}

struct FakeRegistryPersistence final : IRegistryPersistence
{
    struct UpsertCall  { RegistryEntry entry; };
    struct TouchCall   {
        std::uint32_t sid; std::uint64_t lease;
        std::uint32_t cur; std::uint32_t max; std::int64_t hb_unix;
    };
    struct RemoveCall  { std::uint32_t sid; };

    std::vector<UpsertCall> upserts;
    std::vector<TouchCall>  touches;
    std::vector<RemoveCall> removes;
    std::vector<RegistryEntry> load_returns;

    void Upsert(const RegistryEntry& e) override { upserts.push_back({e}); }
    void Touch(std::uint32_t sid, std::uint64_t lease,
               std::uint32_t cur, std::uint32_t max,
               std::int64_t hb) override
    {
        touches.push_back({sid, lease, cur, max, hb});
    }
    void Remove(std::uint32_t sid) override { removes.push_back({sid}); }
    std::vector<RegistryEntry> LoadAll() override { return load_returns; }
};

// ---------------------------------------------------------------------------

void TestRegisterCallsUpsert()
{
    std::printf("[persistence — Register propagates to Upsert]\n");
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, "svc-1"));
    PeerRegistry peers(inv);
    FakeRegistryPersistence fp;
    peers.SetPersistence(&fp);

    RegistryEntry e{};
    e.service_id    = 1;
    e.reported_name = "svc-1";
    e.reported_addr = "10.0.0.1";
    e.reported_port = 4816;
    e.version       = "5.0.0";
    e.pid           = 999;
    const auto lease = peers.Register(e);

    Check(fp.upserts.size() == 1, "exactly one Upsert call");
    const auto& u = fp.upserts.front().entry;
    Check(u.service_id == 1,            "Upsert.entry.service_id");
    Check(u.reported_name == "svc-1",   "Upsert.entry.reported_name");
    Check(u.reported_addr == "10.0.0.1","Upsert.entry.reported_addr");
    Check(u.reported_port == 4816,      "Upsert.entry.reported_port");
    Check(u.version == "5.0.0",         "Upsert.entry.version");
    Check(u.pid == 999,                 "Upsert.entry.pid");
    Check(u.lease_epoch == lease,
        "Upsert.entry.lease_epoch matches returned value");
}

void TestHeartbeatCallsTouch()
{
    std::printf("[persistence — Heartbeat propagates to Touch]\n");
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, "svc-1"));
    PeerRegistry peers(inv);
    FakeRegistryPersistence fp;
    peers.SetPersistence(&fp);

    RegistryEntry e{}; e.service_id = 1;
    const auto lease = peers.Register(e);
    peers.Heartbeat(1, lease, 7, 200);

    Check(fp.touches.size() == 1, "exactly one Touch call");
    const auto& t = fp.touches.front();
    Check(t.sid   == 1,       "Touch.sid");
    Check(t.lease == lease,   "Touch.lease echoed");
    Check(t.cur   == 7,       "Touch.cur_users");
    Check(t.max   == 200,     "Touch.max_users");
    Check(t.hb_unix > 0,      "Touch.heartbeat_unix populated");
}

void TestHeartbeatWithStaleLeaseDoesNotTouch()
{
    std::printf("[persistence — stale-lease heartbeat skips Touch]\n");
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, "svc-1"));
    PeerRegistry peers(inv);
    FakeRegistryPersistence fp;
    peers.SetPersistence(&fp);

    RegistryEntry e{}; e.service_id = 1;
    const auto live = peers.Register(e);
    fp.touches.clear();
    peers.Heartbeat(1, /*lease=*/999, 0, 0);  // bogus epoch
    Check(fp.touches.empty(),
        "no Touch call when epoch mismatches");
    (void)live;
}

void TestDeregisterCallsRemove()
{
    std::printf("[persistence — Deregister propagates to Remove]\n");
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, "svc-1"));
    PeerRegistry peers(inv);
    FakeRegistryPersistence fp;
    peers.SetPersistence(&fp);

    RegistryEntry e{}; e.service_id = 1;
    const auto lease = peers.Register(e);
    Check(peers.Deregister(1, lease), "Deregister returns true");
    Check(fp.removes.size() == 1, "one Remove call");
    Check(fp.removes.front().sid == 1, "Remove.sid matches");
}

void TestExpireStaleCallsRemove()
{
    std::printf("[persistence — ExpireStale propagates to Remove]\n");
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, "svc-1"));
    PeerRegistry peers(inv);
    FakeRegistryPersistence fp;
    peers.SetPersistence(&fp);

    RegistryEntry e{}; e.service_id = 1;
    peers.Register(e);
    fp.removes.clear();

    // Force a sweep with a tiny window so the freshly-registered
    // entry counts as stale immediately.
    const auto reaped = peers.ExpireStale(std::chrono::nanoseconds(1));
    Check(reaped == 1, "ExpireStale reaped one entry");
    Check(fp.removes.size() == 1, "one Remove call from sweep");
    Check(fp.removes.front().sid == 1, "Remove.sid is the reaped entry");
}

void TestHydrateRepopulatesAndAdvancesEpoch()
{
    std::printf("[persistence — Hydrate replays entries, advances epoch]\n");
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, "svc-1"));
    inv.AddService(MakeService(2, "svc-2"));
    PeerRegistry peers(inv);
    FakeRegistryPersistence fp;
    peers.SetPersistence(&fp);

    // Simulate two pre-existing rows the boot reload would have read.
    // Highest lease in the snapshot is 99 — the next Register call
    // must return >= 100, else a re-registering peer's new lease
    // would collide with a stored entry.
    std::vector<RegistryEntry> snapshot;
    {
        RegistryEntry a{}; a.service_id = 1; a.lease_epoch = 17;
        a.reported_name = "svc-1"; a.last_heartbeat_at =
            std::chrono::steady_clock::now();
        snapshot.push_back(a);
        RegistryEntry b{}; b.service_id = 2; b.lease_epoch = 99;
        b.reported_name = "svc-2"; b.last_heartbeat_at =
            std::chrono::steady_clock::now();
        snapshot.push_back(b);
    }
    peers.Hydrate(snapshot);

    Check(peers.FindRegistration(1) != nullptr,
        "Hydrate populated svc-1");
    Check(peers.FindRegistration(2) != nullptr,
        "Hydrate populated svc-2");
    Check(peers.FindRegistration(2)->lease_epoch == 99,
        "stored lease epoch preserved");

    // Hydrate must NOT call back into persistence (it's reading,
    // not writing) — the upserts log stays empty.
    Check(fp.upserts.empty(),
        "Hydrate did not write back through persistence");

    // Issue a fresh Register — its assigned epoch MUST be > 99,
    // otherwise a future Heartbeat against the hydrated entry could
    // match the wrong lease.
    RegistryEntry fresh{};
    fresh.service_id = 1;  // re-registering svc-1, which is in snapshot
    const auto new_lease = peers.Register(fresh);
    Check(new_lease > 99,
        "Register after Hydrate assigns lease > max(stored)");
}

void TestNoopPersistenceIsSilent()
{
    std::printf("[persistence — Noop default does nothing]\n");
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, "svc-1"));
    PeerRegistry peers(inv);

    NoopRegistryPersistence noop;
    peers.SetPersistence(&noop);

    RegistryEntry e{}; e.service_id = 1;
    const auto lease = peers.Register(e);
    peers.Heartbeat(1, lease, 1, 1);
    peers.Deregister(1, lease);
    // No assertion possible on the Noop side — the test just
    // verifies the call sites don't crash when persistence is
    // present but is a no-op. Pass marker:
    Check(true, "Noop persistence accepts all mutators without throwing");
}

} // namespace

int main()
{
    std::printf("=== tcontrolsvr_asio registry-persistence test ===\n");
    try
    {
        TestRegisterCallsUpsert();
        TestHeartbeatCallsTouch();
        TestHeartbeatWithStaleLeaseDoesNotTouch();
        TestDeregisterCallsRemove();
        TestExpireStaleCallsRemove();
        TestHydrateRepopulatesAndAdvancesEpoch();
        TestNoopPersistenceIsSilent();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
