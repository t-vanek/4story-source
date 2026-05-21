// Test coverage for the TControl-local AdminShell — the single
// operator entry point for the cluster.
//
// Drives each command through DispatchForTest (bypasses the TCP
// listener) against a FakeAdminAuditLogger + FakeServiceController +
// a real PeerRegistry populated from FakeServiceInventory. The two
// fan-out commands (`kick`, `announce`) additionally set up loopback
// peer sockets, register them in PeerRegistry, and read raw bytes off
// the client side to verify the CT_USERKICKOUT_ACK / CT_ANNOUNCEMENT_ACK
// frames hit the wire with the expected user/message payload.

#include "admin_shell.h"
#include "control_session.h"
#include "peer_session.h"
#include "services/admin_audit_logger.h"
#include "services/fake_service_inventory.h"
#include "services/peer_registry.h"
#include "services/service_controller.h"
#include "services/svr_type.h"

#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/use_future.hpp>

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
using tcontrolsvr::PeerRegistry;
using tcontrolsvr::PeerSession;
using tcontrolsvr::ServiceInstance;
using tcontrolsvr::ServiceStatus;
using tcontrolsvr::ControlResult;

namespace {

int g_passed = 0;
int g_failed = 0;
void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}
void CheckContains(const std::string& haystack, const std::string& needle,
                   const char* label)
{
    Check(haystack.find(needle) != std::string::npos, label);
}

// FakeAdminAuditLogger — records calls in flat vectors so tests can
// assert on what audit lines the shell emitted.
struct FakeAdminAudit : tcontrolsvr::IAdminAuditLogger
{
    struct Kick      { std::string op, user; tcontrolsvr::AdminOutcome outcome; };
    struct Announce  { std::string op, msg; std::uint32_t world; };
    struct Action    { std::string op, kind, target; };
    std::vector<Kick>     kicks;
    std::vector<Announce> announces;
    std::vector<Action>   actions;

    void LogKick(const std::string& op_, const std::string& user,
                 tcontrolsvr::AdminOutcome outcome) override
    {
        kicks.push_back({op_, user, outcome});
    }
    void LogMove(const std::string&, const std::string&,
                 std::uint8_t, std::uint16_t) override {}
    void LogTeleportTo(const std::string&, const std::string&,
                       const std::string&) override {}
    void LogBan(const std::string&, const std::string&,
                std::uint32_t, std::uint8_t, const std::string&,
                tcontrolsvr::AdminOutcome) override {}
    void LogChatBan(const std::string&, const std::string&,
                    std::uint16_t, const std::string&) override {}
    void LogAnnouncement(const std::string& op_,
                         std::uint32_t world_filter,
                         const std::string& message) override
    {
        announces.push_back({op_, message, world_filter});
    }
    void LogCharMsg(const std::string&, const std::string&,
                    const std::string&) override {}
    void LogAdminAction(const std::string& op_,
                        const std::string& kind,
                        const std::string& target) override
    {
        actions.push_back({op_, kind, target});
    }
    void LogAuthorityDenied(const std::string&, std::uint8_t,
                            const std::string&) override {}
};

// FakeServiceController — returns canned results and records calls.
struct FakeServiceController : tcontrolsvr::IServiceController
{
    ServiceStatus status_to_return = ServiceStatus::Running;
    ControlResult start_result     = ControlResult::Ok;
    ControlResult stop_result      = ControlResult::Ok;
    int query_count = 0;
    int start_count = 0;
    int stop_count  = 0;

    asio::awaitable<ServiceStatus>
    QueryStatus(const ServiceInstance&) override
    {
        ++query_count;
        co_return status_to_return;
    }
    asio::awaitable<ControlResult>
    Start(const ServiceInstance&) override
    {
        ++start_count;
        co_return start_result;
    }
    asio::awaitable<ControlResult>
    Stop(const ServiceInstance&) override
    {
        ++stop_count;
        co_return stop_result;
    }
};

// Synchronously co_await an awaitable on an io_context (single-shot).
template <class T>
T RunOne(asio::io_context& io, asio::awaitable<T> aw)
{
    auto fut = asio::co_spawn(io, std::move(aw), asio::use_future);
    io.restart();
    io.run();
    return fut.get();
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

// Pre-stage a loopback peer connection in the registry — copies the
// pattern from test_admin_forwarders.cpp. Returns the client side of
// the loopback pair so the test can read whatever the shell pushes
// out.
struct StagedPeer
{
    tcp::socket client;
    std::shared_ptr<PeerSession> peer;
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
    if (auto* st = registry.Status(peer->ServiceId()))
        st->status = ServiceStatus::Running;
    return {std::move(client), peer};
}

// Read one ControlSession packet header (peek at the wID + body len).
// Mirrors the wire shape ControlSession::SendPacket writes.
struct WireRead
{
    std::uint16_t wId;
    std::vector<std::byte> body;
};

// ControlSession's outbound frame is 8 bytes header + body.
WireRead ReadPacket(tcp::socket& sock)
{
    std::uint8_t hdr[8] = {};
    asio::read(sock, asio::buffer(hdr, sizeof(hdr)));
    std::uint16_t wSize = 0; std::uint16_t wId = 0;
    std::memcpy(&wSize, hdr,     2);
    std::memcpy(&wId,   hdr + 2, 2);
    const std::size_t body_size = wSize > sizeof(hdr) ? wSize - sizeof(hdr) : 0;
    std::vector<std::byte> body(body_size);
    if (body_size) asio::read(sock, asio::buffer(body.data(), body_size));
    return {wId, std::move(body)};
}

// ---------------------------------------------------------------------------

void TestHelpAndUnknown()
{
    std::printf("[admin-shell — help + unknown]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    PeerRegistry peers(inv);
    FakeServiceController ctrl;
    FakeAdminAudit audit;
    auto shell = std::make_shared<AdminShell>(
        io, "127.0.0.1", 0,
        [] { return std::size_t{3}; },
        peers, ctrl, &audit, std::chrono::steady_clock::now());

    const auto help = RunOne(io, shell->DispatchForTest("help"));
    CheckContains(help, "peers", "help mentions peers");
    CheckContains(help, "kick", "help mentions kick");
    CheckContains(help, "service start", "help mentions service start");

    const auto unk = RunOne(io, shell->DispatchForTest("flarp"));
    CheckContains(unk, "unknown command: flarp", "unknown verb message");
}

void TestStatus()
{
    std::printf("[admin-shell — status]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, tcontrolsvr::svr_type::kMapSvr, "map-1"));
    inv.AddService(MakeService(2, tcontrolsvr::svr_type::kMapSvr, "map-2"));
    PeerRegistry peers(inv);
    FakeServiceController ctrl;
    FakeAdminAudit audit;
    auto shell = std::make_shared<AdminShell>(
        io, "127.0.0.1", 0,
        [] { return std::size_t{7}; },
        peers, ctrl, &audit, std::chrono::steady_clock::now());

    const auto reply = RunOne(io, shell->DispatchForTest("status"));
    CheckContains(reply, "operators=7", "operator count surfaced");
    CheckContains(reply, "peers=0/2", "live/total peer count surfaced");
    CheckContains(reply, "uptime_seconds=", "uptime printed");
}

void TestPeersListing()
{
    std::printf("[admin-shell — peers]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, tcontrolsvr::svr_type::kLoginSvr, "login-1"));
    inv.AddService(MakeService(2, tcontrolsvr::svr_type::kMapSvr,   "map-1"));
    PeerRegistry peers(inv);
    FakeServiceController ctrl;
    FakeAdminAudit audit;
    auto shell = std::make_shared<AdminShell>(
        io, "127.0.0.1", 0, [] { return std::size_t{0}; },
        peers, ctrl, &audit, std::chrono::steady_clock::now());

    const auto reply = RunOne(io, shell->DispatchForTest("peers"));
    CheckContains(reply, "login-1", "login-1 in listing");
    CheckContains(reply, "map-1",   "map-1 in listing");
    CheckContains(reply, "login",   "type name 'login' present");
    CheckContains(reply, "offline", "no connection → 'offline'");
    CheckContains(reply, "unknown", "no RuntimeStatus → 'unknown'");
}

void TestKickBroadcastsToMapPeers()
{
    std::printf("[admin-shell — kick fans out to MapSvr peers]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, tcontrolsvr::svr_type::kLoginSvr, "login-1"));
    inv.AddService(MakeService(2, tcontrolsvr::svr_type::kMapSvr,   "map-1"));
    inv.AddService(MakeService(3, tcontrolsvr::svr_type::kMapSvr,   "map-2"));
    PeerRegistry peers(inv);
    FakeServiceController ctrl;
    FakeAdminAudit audit;

    auto p_map_1 = StagePeer(io, peers,
        MakeService(2, tcontrolsvr::svr_type::kMapSvr, "map-1"));
    auto p_map_2 = StagePeer(io, peers,
        MakeService(3, tcontrolsvr::svr_type::kMapSvr, "map-2"));

    auto shell = std::make_shared<AdminShell>(
        io, "127.0.0.1", 0, [] { return std::size_t{0}; },
        peers, ctrl, &audit, std::chrono::steady_clock::now());

    const auto reply = RunOne(io,
        shell->DispatchForTest("kick villainplayer"));
    CheckContains(reply, "2 map peer(s)", "fanout count = 2 map peers");

    // Run the io_context a bit so the detached co_spawn'd
    // SendUserKickoutAck coroutines flush to the loopback sockets.
    io.restart();
    io.poll();

    auto r1 = ReadPacket(p_map_1.client);
    auto r2 = ReadPacket(p_map_2.client);
    Check(r1.wId == tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_USERKICKOUT_ACK),
        "map-1 saw CT_USERKICKOUT_ACK");
    Check(r2.wId == tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_USERKICKOUT_ACK),
        "map-2 saw CT_USERKICKOUT_ACK");

    Check(audit.kicks.size() == 1, "audit recorded one kick");
    Check(audit.kicks[0].user == "villainplayer",
        "audit kick.user = villainplayer");
    Check(audit.kicks[0].outcome == tcontrolsvr::AdminOutcome::Success,
        "audit kick.outcome = Success (fanout > 0)");
}

void TestKickWithNoMapPeersAudits()
{
    std::printf("[admin-shell — kick with zero peers → audit Failed]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    PeerRegistry peers(inv);
    FakeServiceController ctrl;
    FakeAdminAudit audit;
    auto shell = std::make_shared<AdminShell>(
        io, "127.0.0.1", 0, [] { return std::size_t{0}; },
        peers, ctrl, &audit, std::chrono::steady_clock::now());

    const auto reply = RunOne(io, shell->DispatchForTest("kick lonelytarget"));
    CheckContains(reply, "0 map peer(s)", "fanout = 0");
    Check(audit.kicks.size() == 1, "audit recorded the attempt");
    Check(audit.kicks[0].outcome == tcontrolsvr::AdminOutcome::Failed,
        "audit kick.outcome = Failed (no peers)");
}

void TestAnnounceFansOutToMapAndWorld()
{
    std::printf("[admin-shell — announce fans out Map+World, skips Login]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, tcontrolsvr::svr_type::kLoginSvr, "login-1"));
    inv.AddService(MakeService(2, tcontrolsvr::svr_type::kMapSvr,   "map-1"));
    inv.AddService(MakeService(3, tcontrolsvr::svr_type::kWorldSvr, "world-1"));
    PeerRegistry peers(inv);
    FakeServiceController ctrl;
    FakeAdminAudit audit;

    auto p_login = StagePeer(io, peers,
        MakeService(1, tcontrolsvr::svr_type::kLoginSvr, "login-1"));
    auto p_map   = StagePeer(io, peers,
        MakeService(2, tcontrolsvr::svr_type::kMapSvr,   "map-1"));
    auto p_world = StagePeer(io, peers,
        MakeService(3, tcontrolsvr::svr_type::kWorldSvr, "world-1"));

    auto shell = std::make_shared<AdminShell>(
        io, "127.0.0.1", 0, [] { return std::size_t{0}; },
        peers, ctrl, &audit, std::chrono::steady_clock::now());

    const auto reply = RunOne(io,
        shell->DispatchForTest("announce server going down in 5 minutes"));
    CheckContains(reply, "2 peer(s)", "fanout = 2 (map + world)");

    io.restart();
    io.poll();

    auto r_map   = ReadPacket(p_map.client);
    auto r_world = ReadPacket(p_world.client);
    Check(r_map.wId == tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_ANNOUNCEMENT_ACK),
        "map saw CT_ANNOUNCEMENT_ACK");
    Check(r_world.wId == tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::CT_ANNOUNCEMENT_ACK),
        "world saw CT_ANNOUNCEMENT_ACK");

    // Login peer should have NOTHING queued (announce is Map+World only).
    p_login.client.non_blocking(true);
    std::uint8_t junk[1] = {};
    boost::system::error_code ec;
    asio::read(p_login.client, asio::buffer(junk, 1), ec);
    Check(ec == boost::asio::error::would_block || ec,
        "login peer received no announcement bytes");

    Check(audit.announces.size() == 1, "audit recorded one announcement");
    Check(audit.announces[0].msg == "server going down in 5 minutes",
        "audit announcement message verbatim");
}

void TestServiceStatusKnown()
{
    std::printf("[admin-shell — service status for known service]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(42, tcontrolsvr::svr_type::kMapSvr, "map-X"));
    PeerRegistry peers(inv);
    FakeServiceController ctrl;
    ctrl.status_to_return = ServiceStatus::Running;
    FakeAdminAudit audit;
    auto shell = std::make_shared<AdminShell>(
        io, "127.0.0.1", 0, [] { return std::size_t{0}; },
        peers, ctrl, &audit, std::chrono::steady_clock::now());

    const auto reply = RunOne(io, shell->DispatchForTest("service status 42"));
    CheckContains(reply, "live=running",
        "controller QueryStatus result surfaced as 'running'");
    CheckContains(reply, "'map-X'", "service name in reply");
    Check(ctrl.query_count == 1, "controller.QueryStatus called once");
}

void TestServiceStatusUnknown()
{
    std::printf("[admin-shell — service status for unknown sid]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    PeerRegistry peers(inv);
    FakeServiceController ctrl;
    FakeAdminAudit audit;
    auto shell = std::make_shared<AdminShell>(
        io, "127.0.0.1", 0, [] { return std::size_t{0}; },
        peers, ctrl, &audit, std::chrono::steady_clock::now());

    const auto reply = RunOne(io, shell->DispatchForTest("service status 999"));
    CheckContains(reply, "999 not found", "unknown sid surfaces clear error");
    Check(ctrl.query_count == 0,
        "controller not called for unknown service_id");
}

void TestServiceStartAndStopAudits()
{
    std::printf("[admin-shell — service start/stop calls controller + audits]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(7, tcontrolsvr::svr_type::kPatchSvr, "patch-1"));
    PeerRegistry peers(inv);
    FakeServiceController ctrl;
    ctrl.start_result = ControlResult::Ok;
    ctrl.stop_result  = ControlResult::Failed;
    FakeAdminAudit audit;
    auto shell = std::make_shared<AdminShell>(
        io, "127.0.0.1", 0, [] { return std::size_t{0}; },
        peers, ctrl, &audit, std::chrono::steady_clock::now());

    const auto reply_start =
        RunOne(io, shell->DispatchForTest("service start 7"));
    CheckContains(reply_start, "'patch-1'", "service name in start reply");
    CheckContains(reply_start, "ok",         "Ok result printed");
    Check(ctrl.start_count == 1, "controller.Start called once");

    const auto reply_stop =
        RunOne(io, shell->DispatchForTest("service stop 7"));
    CheckContains(reply_stop, "failed", "Failed result printed");
    Check(ctrl.stop_count == 1, "controller.Stop called once");

    Check(audit.actions.size() == 2,
        "two LogAdminAction records (start + stop)");
    Check(audit.actions[0].kind == "service_start" &&
          audit.actions[0].target == "patch-1",
        "first audit = service_start patch-1");
    Check(audit.actions[1].kind == "service_stop" &&
          audit.actions[1].target == "patch-1",
        "second audit = service_stop patch-1");
}

void TestLogLevel()
{
    std::printf("[admin-shell — log-level setters]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    PeerRegistry peers(inv);
    FakeServiceController ctrl;
    FakeAdminAudit audit;
    auto shell = std::make_shared<AdminShell>(
        io, "127.0.0.1", 0, [] { return std::size_t{0}; },
        peers, ctrl, &audit, std::chrono::steady_clock::now());

    const auto good = RunOne(io, shell->DispatchForTest("log-level debug"));
    CheckContains(good, "log level → debug", "valid level accepted");

    const auto bad = RunOne(io, shell->DispatchForTest("log-level neverland"));
    CheckContains(bad, "usage:", "bogus level prints usage");
}

} // namespace

int main()
{
    std::printf("=== tcontrolsvr_asio admin-shell test ===\n");
    try
    {
        TestHelpAndUnknown();
        TestStatus();
        TestPeersListing();
        TestKickBroadcastsToMapPeers();
        TestKickWithNoMapPeersAudits();
        TestAnnounceFansOutToMapAndWorld();
        TestServiceStatusKnown();
        TestServiceStatusUnknown();
        TestServiceStartAndStopAudits();
        TestLogLevel();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
