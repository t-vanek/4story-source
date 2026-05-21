// Admin shell `route` commands (F4 — Gateway). Verifies that operator
// CLI input drives the F3 MessageRouter end-to-end and the bytes hit
// the right loopback peer(s). Separate test file from
// test_admin_shell.cpp so the existing parse/dispatch tests stay
// router-agnostic.

#include "admin_shell.h"
#include "control_session.h"
#include "message_router.h"
#include "peer_session.h"
#include "services/admin_audit_logger.h"
#include "services/fake_service_inventory.h"
#include "services/peer_registry.h"
#include "services/service_controller.h"
#include "services/svr_type.h"

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
using tcontrolsvr::AdminShell;
using tcontrolsvr::ControlSession;
using tcontrolsvr::FakeServiceInventory;
using tcontrolsvr::MessageRouter;
using tcontrolsvr::PeerRegistry;
using tcontrolsvr::PeerSession;
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

ServiceInstance MakeService(std::uint32_t sid,
                            std::uint8_t group_id,
                            std::uint8_t type_id,
                            std::string name)
{
    ServiceInstance s{};
    s.service_id = sid;
    s.group_id   = group_id;
    s.type_id    = type_id;
    s.server_id  = static_cast<std::uint8_t>(sid & 0xFF);
    s.name       = std::move(name);
    return s;
}

struct StagedPeer
{
    tcp::socket                    client;
    std::shared_ptr<PeerSession>   peer;
};

StagedPeer StagePeer(asio::io_context& io, PeerRegistry& registry,
                     const ServiceInstance& svc)
{
    asio::io_context client_io;
    tcp::acceptor acc(io, tcp::endpoint(tcp::v4(), 0));
    const auto port = acc.local_endpoint().port();
    tcp::socket client(client_io);
    std::thread connector([&client, port] {
        client.connect({asio::ip::make_address_v4("127.0.0.1"), port});
    });
    tcp::socket peer_side(io);
    acc.accept(peer_side);
    connector.join();
    auto wire = std::make_shared<ControlSession>(std::move(peer_side));
    auto peer = std::make_shared<PeerSession>(wire, svc);
    registry.SetConnection(peer->ServiceId(), peer);
    return {std::move(client), peer};
}

struct CapturedPacket
{
    std::uint16_t          wId;
    std::vector<std::byte> body;
};
CapturedPacket ReadPacket(tcp::socket& sock)
{
    std::uint8_t hdr[8] = {};
    asio::read(sock, asio::buffer(hdr, sizeof(hdr)));
    std::uint16_t wSize = 0, wId = 0;
    std::memcpy(&wSize, hdr,     2);
    std::memcpy(&wId,   hdr + 2, 2);
    const std::size_t body_size = wSize > sizeof(hdr) ? wSize - sizeof(hdr) : 0;
    std::vector<std::byte> body(body_size);
    if (body_size) asio::read(sock, asio::buffer(body.data(), body_size));
    return {wId, std::move(body)};
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
          FakeAdminAudit& audit, MessageRouter* router)
{
    return std::make_shared<AdminShell>(
        io, "127.0.0.1", 0,
        [] { return std::size_t{0}; },
        peers, ctrl, &audit, router,
        std::chrono::steady_clock::now());
}

// ---------------------------------------------------------------------------

void TestRouteServiceDispatchesToOnePeer()
{
    std::printf("[gateway — route service <sid> hits exactly that peer]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 4, "map-1"));
    inv.AddService(MakeService(2, 1, 4, "map-2"));
    PeerRegistry peers(inv);
    auto a = StagePeer(io, peers, MakeService(1, 1, 4, "map-1"));
    auto b = StagePeer(io, peers, MakeService(2, 1, 4, "map-2"));
    MessageRouter router(peers);
    FakeServiceController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit, &router);

    const auto reply = RunOne(io,
        shell->DispatchForTest("route service 2 0xABCD deadbeef"));
    CheckContains(reply, "ok", "command reports ok");

    io.restart(); io.poll();

    auto pkt = ReadPacket(b.client);
    Check(pkt.wId == 0xABCD, "wID parsed as hex");
    Check(pkt.body.size() == 4 &&
          pkt.body[0] == std::byte{0xDE} &&
          pkt.body[1] == std::byte{0xAD} &&
          pkt.body[2] == std::byte{0xBE} &&
          pkt.body[3] == std::byte{0xEF},
        "hex body decoded byte-for-byte");

    Check(audit.actions.size() == 1 && audit.actions[0] == "route_service:2",
        "audit recorded route_service:<sid>");

    a.client.non_blocking(true);
    std::uint8_t junk = 0;
    boost::system::error_code ec;
    asio::read(a.client, asio::buffer(&junk, 1), ec);
    Check(ec == asio::error::would_block || ec,
        "map-1 (non-target) received no bytes");
}

void TestRouteServiceUnknownReportsOffline()
{
    std::printf("[gateway — route service on unknown sid → offline reply]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    PeerRegistry peers(inv);
    MessageRouter router(peers);
    FakeServiceController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit, &router);

    const auto reply = RunOne(io,
        shell->DispatchForTest("route service 9999 0x1234"));
    CheckContains(reply, "offline / unknown",
        "missing service reports offline/unknown");
}

void TestRouteServiceMalformedHexBody()
{
    std::printf("[gateway — route service rejects malformed hex body]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 4, "map-1"));
    PeerRegistry peers(inv);
    auto p = StagePeer(io, peers, MakeService(1, 1, 4, "map-1"));
    MessageRouter router(peers);
    FakeServiceController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit, &router);

    const auto reply = RunOne(io,
        shell->DispatchForTest("route service 1 0xCAFE xyz"));
    CheckContains(reply, "malformed hex body",
        "non-hex body surfaces a clear error");
}

void TestRouteTypeRoundRobins()
{
    std::printf("[gateway — route type cycles through bucket]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 4, "map-1"));
    inv.AddService(MakeService(2, 1, 4, "map-2"));
    PeerRegistry peers(inv);
    auto a = StagePeer(io, peers, MakeService(1, 1, 4, "map-1"));
    auto b = StagePeer(io, peers, MakeService(2, 1, 4, "map-2"));
    MessageRouter router(peers);
    FakeServiceController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit, &router);

    const auto r1 = RunOne(io,
        shell->DispatchForTest("route type 1 4 0xFEFE"));
    const auto r2 = RunOne(io,
        shell->DispatchForTest("route type 1 4 0xFEFE"));
    CheckContains(r1, "service_id 1", "first call → service_id 1");
    CheckContains(r2, "service_id 2", "second call → service_id 2");
}

void TestRouteBroadcastTypeFansOut()
{
    std::printf("[gateway — route broadcast type fans out + counts]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(0x010104, 1, 4, "g1-map"));
    inv.AddService(MakeService(0x020104, 2, 4, "g2-map"));
    PeerRegistry peers(inv);
    auto a = StagePeer(io, peers, MakeService(0x010104, 1, 4, "g1-map"));
    auto b = StagePeer(io, peers, MakeService(0x020104, 2, 4, "g2-map"));
    MessageRouter router(peers);
    FakeServiceController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit, &router);

    const auto reply = RunOne(io,
        shell->DispatchForTest("route broadcast type 4 0xCAFE"));
    CheckContains(reply, "2 peer(s)", "broadcast count = 2");

    io.restart(); io.poll();
    auto pa = ReadPacket(a.client);
    auto pb = ReadPacket(b.client);
    Check(pa.wId == 0xCAFE && pb.wId == 0xCAFE,
        "both peers saw 0xCAFE");

    Check(audit.actions.size() == 1 &&
          audit.actions[0] == "route_broadcast_type:4",
        "audit recorded route_broadcast_type:4");
}

void TestRouteBroadcastGroupFiltersByGroup()
{
    std::printf("[gateway — route broadcast group respects group filter]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(0x010104, 1, 4, "g1-map"));
    inv.AddService(MakeService(0x020104, 2, 4, "g2-map"));
    PeerRegistry peers(inv);
    auto a = StagePeer(io, peers, MakeService(0x010104, 1, 4, "g1-map"));
    auto b = StagePeer(io, peers, MakeService(0x020104, 2, 4, "g2-map"));
    MessageRouter router(peers);
    FakeServiceController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit, &router);

    const auto reply = RunOne(io,
        shell->DispatchForTest("route broadcast group 1 4 0xABCD"));
    CheckContains(reply, "1 peer(s)", "fanout=1 (group 1 only)");

    io.restart(); io.poll();
    auto pa = ReadPacket(a.client);
    Check(pa.wId == 0xABCD, "g1-map saw the broadcast");

    b.client.non_blocking(true);
    std::uint8_t junk = 0;
    boost::system::error_code ec;
    asio::read(b.client, asio::buffer(&junk, 1), ec);
    Check(ec == asio::error::would_block || ec,
        "g2-map (different group) received nothing");
}

void TestPeerCommandUnifiesViews()
{
    std::printf("[gateway — peer <sid> shows static + runtime + registry]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(0x010104, 1, 4, "map-prime"));
    PeerRegistry peers(inv);

    // Pre-stage a runtime status entry + a registry entry so the
    // unified view has something to show on every line.
    if (auto* st = peers.Status(0x010104))
    {
        st->status    = tcontrolsvr::ServiceStatus::Running;
        st->cur_users = 17;
        st->max_users = 200;
    }
    tcontrolsvr::RegistryEntry r{};
    r.service_id    = 0x010104;
    r.reported_name = "map-prime";
    r.reported_addr = "10.0.0.5";
    r.reported_port = 3815;
    r.version       = "5.0.0-test";
    r.pid           = 9001;
    peers.Register(r);

    MessageRouter router(peers);
    FakeServiceController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit, &router);

    const auto reply = RunOne(io,
        shell->DispatchForTest("peer 0x010104"));
    CheckContains(reply, "name='map-prime'",   "static name line");
    CheckContains(reply, "status=running",     "runtime status line");
    CheckContains(reply, "cur_users=17",       "runtime user count line");
    CheckContains(reply, "addr=10.0.0.5:3815", "registry addr line");
    CheckContains(reply, "version=5.0.0-test", "registry version line");
    CheckContains(reply, "pid=9001",           "registry pid line");
}

void TestPeerCommandUnknownReportsNotFound()
{
    std::printf("[gateway — peer <sid> on unknown service]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    PeerRegistry peers(inv);
    MessageRouter router(peers);
    FakeServiceController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit, &router);

    const auto reply = RunOne(io, shell->DispatchForTest("peer 9999"));
    CheckContains(reply, "not found", "missing service surfaces error");
}

void TestRouteUsageMessages()
{
    std::printf("[gateway — usage messages for missing/bad args]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    PeerRegistry peers(inv);
    MessageRouter router(peers);
    FakeServiceController ctrl;
    FakeAdminAudit audit;
    auto shell = MakeShell(io, peers, ctrl, audit, &router);

    CheckContains(
        RunOne(io, shell->DispatchForTest("route")),
        "usage: route", "bare 'route' shows root usage");
    CheckContains(
        RunOne(io, shell->DispatchForTest("route service")),
        "usage: route service",
        "'route service' alone shows command usage");
    CheckContains(
        RunOne(io, shell->DispatchForTest("route type 1 4")),
        "usage: route type",
        "missing wId reports usage");
    CheckContains(
        RunOne(io, shell->DispatchForTest("route broadcast nonsense")),
        "usage: route broadcast",
        "unknown broadcast scope reports usage");
}

} // namespace

int main()
{
    std::printf("=== tcontrolsvr_asio admin-shell route test ===\n");
    try
    {
        TestRouteServiceDispatchesToOnePeer();
        TestRouteServiceUnknownReportsOffline();
        TestRouteServiceMalformedHexBody();
        TestRouteTypeRoundRobins();
        TestRouteBroadcastTypeFansOut();
        TestRouteBroadcastGroupFiltersByGroup();
        TestPeerCommandUnifiesViews();
        TestPeerCommandUnknownReportsNotFound();
        TestRouteUsageMessages();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
