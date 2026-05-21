// MessageRouter coverage — verifies the four routing primitives
// against a PeerRegistry populated with loopback peer sockets.
// Reads the bytes off each peer's client side to confirm the right
// peer(s) received the message with the right wID + body.
//
// Pattern copies test_admin_forwarders.cpp / test_admin_shell.cpp:
//   - FakeServiceInventory pre-stages the service rows
//   - StagePeer creates a (client, server-side) socket pair, wraps the
//     server side in ControlSession+PeerSession, calls
//     PeerRegistry::SetConnection
//   - Test thread runs io_context on a worker, blocking reads on the
//     client sides catch the bytes the router pushes out

#include "control_session.h"
#include "message_router.h"
#include "peer_session.h"
#include "services/fake_service_inventory.h"
#include "services/peer_registry.h"

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

// ControlSession::SendPacket wire shape: 8-byte header
// (WORD wSize | WORD wID | DWORD chk) + body bytes.
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

// Drive a single co_await on the io_context, blocking until done.
// Same shape as test_admin_shell.cpp::RunOne.
template <class T>
T RunOne(asio::io_context& io, asio::awaitable<T> aw)
{
    auto fut = asio::co_spawn(io, std::move(aw), asio::use_future);
    io.restart();
    io.run();
    return fut.get();
}

// Marker bytes the tests send — easy to spot in the wire dump.
const std::vector<std::byte> kBody = {
    std::byte{0xDE}, std::byte{0xAD}, std::byte{0xBE}, std::byte{0xEF},
};
constexpr std::uint16_t kWid = 0xABCD;

// ---------------------------------------------------------------------------

void TestSendToServiceHitsCorrectPeer()
{
    std::printf("[router — SendToService hits matching peer only]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 4, "map-1"));
    inv.AddService(MakeService(2, 1, 4, "map-2"));
    PeerRegistry peers(inv);
    auto a = StagePeer(io, peers, MakeService(1, 1, 4, "map-1"));
    auto b = StagePeer(io, peers, MakeService(2, 1, 4, "map-2"));
    MessageRouter router(peers);

    const auto ok = RunOne(io,
        router.SendToService(2, kWid, kBody));
    Check(ok, "SendToService returned true for live peer");

    // map-2 should have one packet; map-1 should have nothing.
    auto pkt = ReadPacket(b.client);
    Check(pkt.wId == kWid, "map-2 saw the right wID");
    Check(pkt.body == kBody, "map-2 body matches");

    // map-1's socket must have no data. Probe in non-blocking mode.
    a.client.non_blocking(true);
    std::uint8_t junk = 0;
    boost::system::error_code ec;
    asio::read(a.client, asio::buffer(&junk, 1), ec);
    Check(ec == asio::error::would_block || ec,
        "map-1 (non-target) received no bytes");
}

void TestSendToServiceMissesUnknown()
{
    std::printf("[router — SendToService on unknown service_id]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    PeerRegistry peers(inv);
    MessageRouter router(peers);
    const auto ok = RunOne(io,
        router.SendToService(0xDEADBEEF, kWid, kBody));
    Check(!ok, "SendToService(unknown) returned false");
}

void TestSendToServiceSkipsOfflinePeer()
{
    std::printf("[router — SendToService skips peer with closed wire]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 4, "map-1"));
    PeerRegistry peers(inv);
    auto p = StagePeer(io, peers, MakeService(1, 1, 4, "map-1"));
    // Drop the client side so the server socket's peer is closed.
    p.client.close();
    // Closing remote doesn't auto-close the local socket — manually
    // close the server side so IsOpen() returns false.
    p.peer->Wire()->Close();
    MessageRouter router(peers);

    const auto ok = RunOne(io,
        router.SendToService(1, kWid, kBody));
    Check(!ok, "SendToService returned false on closed wire");
}

void TestSendToTypeRoundRobins()
{
    std::printf("[router — SendToType cycles through bucket]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 4, "map-1"));
    inv.AddService(MakeService(2, 1, 4, "map-2"));
    inv.AddService(MakeService(3, 1, 4, "map-3"));
    PeerRegistry peers(inv);
    auto a = StagePeer(io, peers, MakeService(1, 1, 4, "map-1"));
    auto b = StagePeer(io, peers, MakeService(2, 1, 4, "map-2"));
    auto c = StagePeer(io, peers, MakeService(3, 1, 4, "map-3"));
    MessageRouter router(peers);

    // Six SendToType calls should hit map-1, map-2, map-3, map-1, ...
    std::vector<std::uint32_t> picks;
    for (int i = 0; i < 6; ++i)
        picks.push_back(RunOne(io,
            router.SendToType(1, 4, kWid, kBody)));

    Check(picks[0] == 1, "pick 0 → map-1");
    Check(picks[1] == 2, "pick 1 → map-2");
    Check(picks[2] == 3, "pick 2 → map-3");
    Check(picks[3] == 1, "pick 3 → map-1 (wrapped)");
    Check(picks[4] == 2, "pick 4 → map-2");
    Check(picks[5] == 3, "pick 5 → map-3");

    auto ra1 = ReadPacket(a.client);
    auto rb1 = ReadPacket(b.client);
    auto rc1 = ReadPacket(c.client);
    auto ra2 = ReadPacket(a.client);
    auto rb2 = ReadPacket(b.client);
    auto rc2 = ReadPacket(c.client);
    Check(ra1.wId == kWid && rb1.wId == kWid && rc1.wId == kWid &&
          ra2.wId == kWid && rb2.wId == kWid && rc2.wId == kWid,
        "every recipient got the right wID");
}

void TestSendToTypeSkipsOfflinePeer()
{
    std::printf("[router — SendToType skips offline peer]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 4, "map-1"));
    inv.AddService(MakeService(2, 1, 4, "map-2"));
    PeerRegistry peers(inv);
    auto a = StagePeer(io, peers, MakeService(1, 1, 4, "map-1"));
    auto b = StagePeer(io, peers, MakeService(2, 1, 4, "map-2"));
    // Take map-1 offline.
    a.client.close();
    a.peer->Wire()->Close();
    MessageRouter router(peers);

    // Three round-robin attempts should all go to map-2.
    auto p1 = RunOne(io, router.SendToType(1, 4, kWid, kBody));
    auto p2 = RunOne(io, router.SendToType(1, 4, kWid, kBody));
    auto p3 = RunOne(io, router.SendToType(1, 4, kWid, kBody));
    Check(p1 == 2 && p2 == 2 && p3 == 2,
        "all picks → map-2 (only live peer)");
}

void TestSendToTypeEmptyBucket()
{
    std::printf("[router — SendToType returns 0 when no live peers]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    PeerRegistry peers(inv);
    MessageRouter router(peers);
    auto picked = RunOne(io, router.SendToType(1, 4, kWid, kBody));
    Check(picked == 0, "no live peers → picked=0");
}

void TestBroadcastToGroupTypeHitsAllInGroup()
{
    std::printf("[router — BroadcastToGroupType respects group filter]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(0x010101, 1, 4, "g1-map-1"));
    inv.AddService(MakeService(0x010102, 1, 4, "g1-map-2"));
    inv.AddService(MakeService(0x020101, 2, 4, "g2-map-1"));
    PeerRegistry peers(inv);
    auto a = StagePeer(io, peers, MakeService(0x010101, 1, 4, "g1-map-1"));
    auto b = StagePeer(io, peers, MakeService(0x010102, 1, 4, "g1-map-2"));
    auto c = StagePeer(io, peers, MakeService(0x020101, 2, 4, "g2-map-1"));
    MessageRouter router(peers);

    const auto fanout = RunOne(io,
        router.BroadcastToGroupType(1, 4, kWid, kBody));
    Check(fanout == 2, "fanout=2 (only group 1)");

    // group-1 peers got it.
    auto ra = ReadPacket(a.client);
    auto rb = ReadPacket(b.client);
    Check(ra.wId == kWid, "g1-map-1 saw the broadcast");
    Check(rb.wId == kWid, "g1-map-2 saw the broadcast");

    // group-2 peer didn't.
    c.client.non_blocking(true);
    std::uint8_t junk = 0;
    boost::system::error_code ec;
    asio::read(c.client, asio::buffer(&junk, 1), ec);
    Check(ec == asio::error::would_block || ec,
        "g2-map-1 received nothing (different group)");
}

void TestBroadcastToTypeAcrossGroups()
{
    std::printf("[router — BroadcastToType fans out across groups]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(0x010101, 1, 4, "g1-map-1"));
    inv.AddService(MakeService(0x020201, 2, 4, "g2-map-1"));
    inv.AddService(MakeService(0x010301, 1, 1, "g1-login-1"));   // wrong type
    PeerRegistry peers(inv);
    auto a = StagePeer(io, peers, MakeService(0x010101, 1, 4, "g1-map-1"));
    auto b = StagePeer(io, peers, MakeService(0x020201, 2, 4, "g2-map-1"));
    auto c = StagePeer(io, peers, MakeService(0x010301, 1, 1, "g1-login-1"));
    MessageRouter router(peers);

    const auto fanout = RunOne(io,
        router.BroadcastToType(4, kWid, kBody));
    Check(fanout == 2, "fanout=2 (both map peers, both groups)");

    auto ra = ReadPacket(a.client);
    auto rb = ReadPacket(b.client);
    Check(ra.wId == kWid && rb.wId == kWid, "both map peers got it");

    c.client.non_blocking(true);
    std::uint8_t junk = 0;
    boost::system::error_code ec;
    asio::read(c.client, asio::buffer(&junk, 1), ec);
    Check(ec == asio::error::would_block || ec,
        "login peer (wrong type) received nothing");
}

void TestBroadcastSkipsClosedPeers()
{
    std::printf("[router — Broadcast skips closed wires]\n");
    asio::io_context io;
    FakeServiceInventory inv;
    inv.AddService(MakeService(1, 1, 4, "map-1"));
    inv.AddService(MakeService(2, 1, 4, "map-2"));
    PeerRegistry peers(inv);
    auto a = StagePeer(io, peers, MakeService(1, 1, 4, "map-1"));
    auto b = StagePeer(io, peers, MakeService(2, 1, 4, "map-2"));
    a.peer->Wire()->Close();
    MessageRouter router(peers);

    const auto fanout_g = RunOne(io,
        router.BroadcastToGroupType(1, 4, kWid, kBody));
    Check(fanout_g == 1, "BroadcastToGroupType skipped closed peer");

    const auto fanout_t = RunOne(io,
        router.BroadcastToType(4, kWid, kBody));
    Check(fanout_t == 1, "BroadcastToType skipped closed peer");
}

} // namespace

int main()
{
    std::printf("=== tcontrolsvr_asio message-router test ===\n");
    try
    {
        TestSendToServiceHitsCorrectPeer();
        TestSendToServiceMissesUnknown();
        TestSendToServiceSkipsOfflinePeer();
        TestSendToTypeRoundRobins();
        TestSendToTypeSkipsOfflinePeer();
        TestSendToTypeEmptyBucket();
        TestBroadcastToGroupTypeHitsAllInGroup();
        TestBroadcastToTypeAcrossGroups();
        TestBroadcastSkipsClosedPeers();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
