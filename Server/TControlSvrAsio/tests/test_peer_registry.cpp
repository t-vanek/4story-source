// F1 of the modern cluster control plane: peer self-registration.
//
// Two layers under test:
//   1. PeerRegistry's dynamic registry table — Register/Heartbeat/
//      Deregister/ExpireStale lease semantics. Pure unit, no asio.
//   2. The CT_PEER_* wire handlers — drive each handler through
//      DispatchForTest-style direct calls against a loopback
//      OperatorSession, then read the ACK bytes off the wire and
//      assert on (accepted, reason_code, lease_epoch) tuples.
//
// No DB, no SOCI. Runs everywhere we can build the binary.

#include "control_session.h"
#include "handlers/handlers.h"
#include "operator_session.h"
#include "peer_session.h"
#include "services/fake_service_inventory.h"
#include "services/peer_registry.h"

#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/use_future.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace asio = boost::asio;
using boost::asio::ip::tcp;
using namespace std::chrono_literals;

using tcontrolsvr::ControlSession;
using tcontrolsvr::FakeServiceInventory;
using tcontrolsvr::HandlerContext;
using tcontrolsvr::OperatorSession;
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

ServiceInstance MakeService(std::uint32_t sid, std::uint8_t type_id,
                            std::string name)
{
    ServiceInstance s{};
    s.service_id = sid;
    s.type_id    = type_id;
    s.group_id   = 1;
    s.server_id  = static_cast<std::uint8_t>(sid & 0xFF);
    s.name       = std::move(name);
    return s;
}

// -------- Pure-unit tests on PeerRegistry --------------------------

void TestRegisterAssignsMonotonicEpoch()
{
    std::printf("[unit — Register assigns monotonic lease epochs]\n");
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, "svc-1"));
    inv.AddService(MakeService(2, 1, "svc-2"));
    PeerRegistry reg(inv);

    RegistryEntry a{}; a.service_id = 1; a.reported_name = "a";
    RegistryEntry b{}; b.service_id = 2; b.reported_name = "b";

    const auto e1 = reg.Register(a);
    const auto e2 = reg.Register(b);
    const auto e1b = reg.Register(a);   // re-register bumps epoch

    Check(e1 != 0,   "first epoch is non-zero");
    Check(e2 > e1,   "second registration gets larger epoch");
    Check(e1b > e2,  "re-registration of same service bumps past prior peak");

    Check(reg.FindRegistration(1)->lease_epoch == e1b,
        "live epoch reflects most recent Register");
}

void TestHeartbeatRejectsStaleEpoch()
{
    std::printf("[unit — Heartbeat rejects stale epoch]\n");
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, "svc-1"));
    PeerRegistry reg(inv);
    RegistryEntry e{}; e.service_id = 1;
    const auto old = reg.Register(e);
    const auto fresh = reg.Register(e);  // bumps past old

    Check(reg.Heartbeat(1, old, 0, 0) == 0,
        "old epoch is rejected (returns 0)");
    Check(reg.Heartbeat(1, fresh, 5, 100) == fresh,
        "current epoch is accepted, returns same epoch");
    Check(reg.FindRegistration(1)->cur_users == 5,
        "Heartbeat persists cur_users");
    Check(reg.FindRegistration(1)->max_users == 100,
        "Heartbeat persists max_users");
}

void TestHeartbeatRejectsUnknownService()
{
    std::printf("[unit — Heartbeat on unknown service_id returns 0]\n");
    FakeServiceInventory inv;
    PeerRegistry reg(inv);
    Check(reg.Heartbeat(999, 1, 0, 0) == 0,
        "unknown service heartbeat → 0");
}

void TestDeregisterRemovesEntry()
{
    std::printf("[unit — Deregister with correct epoch wipes entry]\n");
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, "svc-1"));
    PeerRegistry reg(inv);
    RegistryEntry e{}; e.service_id = 1;
    const auto epoch = reg.Register(e);

    Check(reg.Deregister(1, epoch + 1) == false,
        "wrong epoch is rejected");
    Check(reg.FindRegistration(1) != nullptr,
        "entry survives the wrong-epoch attempt");
    Check(reg.Deregister(1, epoch) == true,
        "correct epoch accepted");
    Check(reg.FindRegistration(1) == nullptr,
        "entry gone after Deregister");
}

void TestExpireStaleSweep()
{
    std::printf("[unit — ExpireStale reaps old entries]\n");
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, "svc-1"));
    inv.AddService(MakeService(2, 1, "svc-2"));
    PeerRegistry reg(inv);
    RegistryEntry a{}; a.service_id = 1;
    RegistryEntry b{}; b.service_id = 2;
    reg.Register(a);
    reg.Register(b);

    std::this_thread::sleep_for(80ms);
    // Refresh svc-2's heartbeat so only svc-1 is stale.
    const auto b_epoch = reg.FindRegistration(2)->lease_epoch;
    reg.Heartbeat(2, b_epoch, 0, 0);

    const auto expired = reg.ExpireStale(50ms);
    Check(expired == 1, "exactly one stale entry expired");
    Check(reg.FindRegistration(1) == nullptr, "svc-1 gone");
    Check(reg.FindRegistration(2) != nullptr, "svc-2 survives");
}

void TestRegistrySnapshotIsByValue()
{
    std::printf("[unit — Registry() snapshot is independent]\n");
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, "svc-1"));
    PeerRegistry reg(inv);
    RegistryEntry e{}; e.service_id = 1; e.reported_name = "v1";
    reg.Register(e);

    auto snap = reg.Registry();
    // Mutate through the live registry — snapshot must not change.
    RegistryEntry e2{}; e2.service_id = 1; e2.reported_name = "v2";
    reg.Register(e2);
    Check(snap.size() == 1, "snapshot still has one entry");
    Check(snap[0].reported_name == "v1",
        "snapshot keeps the pre-mutation value");
}

// -------- Wire-level tests on the handlers --------------------------
//
// Build a loopback (client, server) socket pair, wrap the server side
// in an OperatorSession, dispatch the request through the wire
// handler, then read the ACK frame off the client side. Compares the
// reply tuple byte-for-byte.

struct LoopbackPair
{
    asio::io_context io;
    tcp::socket      client;
    tcp::socket      server;

    LoopbackPair() : client(io), server(io)
    {
        tcp::acceptor acc(io, tcp::endpoint(tcp::v4(), 0));
        client.connect(acc.local_endpoint());
        acc.accept(server);
    }
};

// Header + body layout matches ControlSession::SendPacket.
struct CapturedPacket
{
    std::uint16_t          wId;
    std::vector<std::byte> body;
};

CapturedPacket ReadOnePacket(tcp::socket& sock)
{
    std::uint8_t hdr[8] = {};
    asio::read(sock, asio::buffer(hdr, sizeof(hdr)));
    std::uint16_t wSize = 0, wId = 0;
    std::memcpy(&wSize, hdr, 2);
    std::memcpy(&wId, hdr + 2, 2);
    const std::size_t body_size = wSize > sizeof(hdr) ? wSize - sizeof(hdr) : 0;
    std::vector<std::byte> body(body_size);
    if (body_size) asio::read(sock, asio::buffer(body.data(), body_size));
    return {wId, std::move(body)};
}

// Wire encoders mirroring wire_codec.h.
template <class T>
void PushPOD(std::vector<std::byte>& out, T v)
{
    const auto* p = reinterpret_cast<const std::byte*>(&v);
    out.insert(out.end(), p, p + sizeof(T));
}
void PushString(std::vector<std::byte>& out, const std::string& s)
{
    PushPOD<std::int32_t>(out, static_cast<std::int32_t>(s.size()));
    const auto* p = reinterpret_cast<const std::byte*>(s.data());
    out.insert(out.end(), p, p + s.size());
}

void TestRegisterReqAcceptsKnownService()
{
    std::printf("[wire — CT_PEER_REGISTER_REQ on known service is accepted]\n");
    LoopbackPair rig;
    FakeServiceInventory inv;
    inv.AddService(MakeService(0x010204, 4, "map-1"));
    PeerRegistry peers(inv);
    HandlerContext ctx{};
    ctx.peers = &peers;

    auto wire = std::make_shared<ControlSession>(std::move(rig.server));
    auto op = std::make_shared<OperatorSession>(wire);

    // Build a register body for svc 0x010204.
    std::vector<std::byte> body;
    PushPOD<std::uint32_t>(body, 0x010204);
    PushString(body, "map-1");
    PushString(body, "10.0.0.42");
    PushPOD<std::uint16_t>(body, 3815);
    PushString(body, "5.0.0-modern");
    PushPOD<std::uint32_t>(body, 12345);  // pid
    PushPOD<std::uint64_t>(body, 1700000000ULL);  // start_unix

    asio::co_spawn(rig.io,
        tcontrolsvr::handlers::OnPeerRegisterReq(op, body, ctx),
        asio::detached);
    std::thread runner([&] { rig.io.run(); });

    const auto pkt = ReadOnePacket(rig.client);
    runner.join();

    Check(pkt.wId == tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_PEER_REGISTER_ACK),
        "wID == CT_PEER_REGISTER_ACK");

    // Reply body: BYTE accepted, DWORD reason, QWORD lease, DWORD hb_interval
    std::uint8_t  accepted = 0;
    std::uint32_t reason   = 0;
    std::uint64_t lease    = 0;
    std::uint32_t hb_int   = 0;
    std::memcpy(&accepted, pkt.body.data(), 1);
    std::memcpy(&reason,   pkt.body.data() + 1, 4);
    std::memcpy(&lease,    pkt.body.data() + 5, 8);
    std::memcpy(&hb_int,   pkt.body.data() + 13, 4);

    Check(accepted == 1,   "accepted=1");
    Check(reason   == 0,   "reason_code=0 on success");
    Check(lease    > 0,    "lease_epoch is non-zero");
    Check(hb_int   == 30,  "heartbeat_interval_sec=30 (default cadence)");

    // Registry state: entry present, connection wired.
    const auto* e = peers.FindRegistration(0x010204);
    Check(e != nullptr, "registry has the new entry");
    Check(e && e->reported_name == "map-1", "reported_name persisted");
    Check(e && e->reported_addr == "10.0.0.42", "reported_addr persisted");
    Check(e && e->reported_port == 3815,        "reported_port persisted");
    Check(e && e->version == "5.0.0-modern",    "version persisted");
    Check(e && e->pid == 12345,                 "pid persisted");
    Check(peers.Connection(0x010204) != nullptr,
        "PeerRegistry::Connection is wired to the inbound session");
}

void TestRegisterReqRejectsUnknownService()
{
    std::printf("[wire — CT_PEER_REGISTER_REQ on unknown service is rejected]\n");
    LoopbackPair rig;
    FakeServiceInventory inv;  // empty
    PeerRegistry peers(inv);
    HandlerContext ctx{};
    ctx.peers = &peers;

    auto wire = std::make_shared<ControlSession>(std::move(rig.server));
    auto op = std::make_shared<OperatorSession>(wire);

    std::vector<std::byte> body;
    PushPOD<std::uint32_t>(body, 0xDEADBEEF);
    PushString(body, "ghost");
    PushString(body, "1.2.3.4");
    PushPOD<std::uint16_t>(body, 0);
    PushString(body, "");
    PushPOD<std::uint32_t>(body, 0);
    PushPOD<std::uint64_t>(body, 0);

    asio::co_spawn(rig.io,
        tcontrolsvr::handlers::OnPeerRegisterReq(op, body, ctx),
        asio::detached);
    std::thread runner([&] { rig.io.run(); });
    const auto pkt = ReadOnePacket(rig.client);
    runner.join();

    std::uint8_t  accepted = 0;
    std::uint32_t reason   = 0;
    std::uint64_t lease    = 0;
    std::memcpy(&accepted, pkt.body.data(),     1);
    std::memcpy(&reason,   pkt.body.data() + 1, 4);
    std::memcpy(&lease,    pkt.body.data() + 5, 8);
    Check(accepted == 0, "accepted=0");
    Check(reason   == 1, "reason_code=1 (unknown service)");
    Check(lease    == 0, "no lease issued on rejection");
    Check(peers.FindRegistration(0xDEADBEEF) == nullptr,
        "no registry entry created");
}

void TestRegisterReqRejectsMalformedBody()
{
    std::printf("[wire — CT_PEER_REGISTER_REQ malformed body is rejected]\n");
    LoopbackPair rig;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, "svc-1"));
    PeerRegistry peers(inv);
    HandlerContext ctx{};
    ctx.peers = &peers;

    auto wire = std::make_shared<ControlSession>(std::move(rig.server));
    auto op = std::make_shared<OperatorSession>(wire);

    std::vector<std::byte> body(3);  // shorter than DWORD service_id
    asio::co_spawn(rig.io,
        tcontrolsvr::handlers::OnPeerRegisterReq(op, body, ctx),
        asio::detached);
    std::thread runner([&] { rig.io.run(); });
    const auto pkt = ReadOnePacket(rig.client);
    runner.join();

    std::uint8_t  accepted = 0;
    std::uint32_t reason   = 0;
    std::memcpy(&accepted, pkt.body.data(),     1);
    std::memcpy(&reason,   pkt.body.data() + 1, 4);
    Check(accepted == 0, "accepted=0");
    Check(reason   == 2, "reason_code=2 (malformed)");
}

void TestHeartbeatReqAcceptsCurrentLease()
{
    std::printf("[wire — CT_PEER_HEARTBEAT_REQ with current lease accepted]\n");
    LoopbackPair rig;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, "svc-1"));
    PeerRegistry peers(inv);
    RegistryEntry pre{}; pre.service_id = 1;
    const auto lease = peers.Register(pre);
    HandlerContext ctx{};
    ctx.peers = &peers;

    auto wire = std::make_shared<ControlSession>(std::move(rig.server));
    auto op = std::make_shared<OperatorSession>(wire);

    std::vector<std::byte> body;
    PushPOD<std::uint32_t>(body, 1);
    PushPOD<std::uint64_t>(body, lease);
    PushPOD<std::uint32_t>(body, 42);   // cur_users
    PushPOD<std::uint32_t>(body, 200);  // max_users

    asio::co_spawn(rig.io,
        tcontrolsvr::handlers::OnPeerHeartbeatReq(op, body, ctx),
        asio::detached);
    std::thread runner([&] { rig.io.run(); });
    const auto pkt = ReadOnePacket(rig.client);
    runner.join();

    std::uint8_t  accepted = 0;
    std::uint64_t echoed   = 0;
    std::memcpy(&accepted, pkt.body.data(),     1);
    std::memcpy(&echoed,   pkt.body.data() + 1, 8);
    Check(accepted == 1, "accepted=1");
    Check(echoed   == lease, "lease epoch echoed back");
    Check(peers.FindRegistration(1)->cur_users == 42,
        "cur_users persisted");
    Check(peers.FindRegistration(1)->max_users == 200,
        "max_users persisted");
}

void TestHeartbeatReqRejectsStaleLease()
{
    std::printf("[wire — CT_PEER_HEARTBEAT_REQ with stale lease rejected]\n");
    LoopbackPair rig;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, "svc-1"));
    PeerRegistry peers(inv);
    HandlerContext ctx{};
    ctx.peers = &peers;

    auto wire = std::make_shared<ControlSession>(std::move(rig.server));
    auto op = std::make_shared<OperatorSession>(wire);

    // Build a heartbeat for service that was never registered.
    std::vector<std::byte> body;
    PushPOD<std::uint32_t>(body, 1);
    PushPOD<std::uint64_t>(body, 999);  // bogus lease
    PushPOD<std::uint32_t>(body, 0);
    PushPOD<std::uint32_t>(body, 0);

    asio::co_spawn(rig.io,
        tcontrolsvr::handlers::OnPeerHeartbeatReq(op, body, ctx),
        asio::detached);
    std::thread runner([&] { rig.io.run(); });
    const auto pkt = ReadOnePacket(rig.client);
    runner.join();

    std::uint8_t accepted = 0;
    std::uint64_t lease   = 0;
    std::memcpy(&accepted, pkt.body.data(),     1);
    std::memcpy(&lease,    pkt.body.data() + 1, 8);
    Check(accepted == 0, "accepted=0 (peer must re-register)");
    Check(lease    == 0, "no lease echoed");
}

void TestDeregisterReqClearsConnection()
{
    std::printf("[wire — CT_PEER_DEREGISTER_REQ wipes entry + connection]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, "svc-1"));
    PeerRegistry peers(inv);

    // Pre-stage: register + wire a fake connection.
    RegistryEntry pre{}; pre.service_id = 1;
    const auto lease = peers.Register(pre);
    // Provide a connection so Deregister has something to clear.
    tcp::acceptor acc(io, tcp::endpoint(tcp::v4(), 0));
    tcp::socket client(io), server(io);
    client.connect(acc.local_endpoint());
    acc.accept(server);
    auto wire = std::make_shared<ControlSession>(std::move(server));
    auto peer = std::make_shared<tcontrolsvr::PeerSession>(wire,
        *peers.FindService(1));
    peers.SetConnection(1, peer);

    HandlerContext ctx{};
    ctx.peers = &peers;

    auto op_wire = std::make_shared<ControlSession>(std::move(client));
    auto op = std::make_shared<OperatorSession>(op_wire);

    std::vector<std::byte> body;
    PushPOD<std::uint32_t>(body, 1);
    PushPOD<std::uint64_t>(body, lease);

    asio::co_spawn(io,
        tcontrolsvr::handlers::OnPeerDeregisterReq(op, body, ctx),
        asio::detached);
    io.run();

    Check(peers.FindRegistration(1) == nullptr,
        "registry entry gone");
    Check(peers.Connection(1) == nullptr,
        "PeerRegistry::Connection cleared");
}

} // namespace

int main()
{
    std::printf("=== tcontrolsvr_asio peer-registry test ===\n");
    try
    {
        TestRegisterAssignsMonotonicEpoch();
        TestHeartbeatRejectsStaleEpoch();
        TestHeartbeatRejectsUnknownService();
        TestDeregisterRemovesEntry();
        TestExpireStaleSweep();
        TestRegistrySnapshotIsByValue();
        TestRegisterReqAcceptsKnownService();
        TestRegisterReqRejectsUnknownService();
        TestRegisterReqRejectsMalformedBody();
        TestHeartbeatReqAcceptsCurrentLease();
        TestHeartbeatReqRejectsStaleLease();
        TestDeregisterReqClearsConnection();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
