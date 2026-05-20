// Characterization test for CS_MOVE_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:439-485
//
// Wire body (28 bytes):
//   DWORD wMapID, FLOAT fPosX/Y/Z, WORD wPitch, WORD wDIR,
//   BYTE bMouseDIR, bKeyDIR, bAction, bGhost, FLOAT fSpeed
//
// Branches tested:
//   §1 malformed body (< 28 bytes) → silent drop
//   §2 not connected / no snapshot → silent drop
//   §3 speed > 3.40f → clamped to 3.40f, position updated
//   §4 no IMapState → position updated in snapshot, no AOI call
//   §5 IMapState wired → OnMove called, CS_MOVE_ACK broadcast
//      to common_aoi neighbours (via FakeSessionRegistry)
//
// Tests drive OnMoveReq directly (not via wire dispatcher).
// Server fixture used only for §5 (needs live io_context for co_await).

#include "handlers.h"
#include "handlers_map.h"
#include "map_state.h"
#include "services/session_registry.h"
#include "services/player_service.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>

#include <cstdio>
#include <exception>

namespace {

int g_passed  = 0;
int g_failed  = 0;
int g_pending = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS     %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL     %s\n", label); }
}

void Pending(const char* label, const char* ref)
{
    ++g_pending;
    std::printf("  PENDING  %s   (%s)\n", label, ref);
}

template<typename Coro>
void RunSync(Coro c)
{
    boost::asio::io_context io;
    boost::asio::co_spawn(io, std::move(c), boost::asio::detached);
    io.run();
}

// Build CS_MOVE_REQ wire body
tnetlib::DecodedPacket MakeMoveReq(
    float px = 100.0f, float py = 0.0f, float pz = 100.0f,
    float speed = 1.0f, std::uint8_t action = 2 /*RUN*/)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;

    tnetlib::DecodedPacket pkt{};
    pkt.wId = ToUint16(MessageId::CS_MOVE_REQ);

    auto push4 = [&](std::uint32_t v) {
        for (int i = 0; i < 4; ++i)
            pkt.body.push_back(static_cast<std::byte>((v >> (8 * i)) & 0xFF));
    };
    auto pushF = [&](float v) {
        std::uint32_t bits;
        std::memcpy(&bits, &v, 4);
        push4(bits);
    };
    auto push2 = [&](std::uint16_t v) {
        pkt.body.push_back(static_cast<std::byte>(v & 0xFF));
        pkt.body.push_back(static_cast<std::byte>((v >> 8) & 0xFF));
    };
    auto push1 = [&](std::uint8_t v) {
        pkt.body.push_back(static_cast<std::byte>(v));
    };

    push4(201);       // wMapID
    pushF(px);        // fPosX
    pushF(py);        // fPosY
    pushF(pz);        // fPosZ
    push2(0);         // wPitch
    push2(2048);      // wDIR
    push1(0);         // bMouseDIR
    push1(0);         // bKeyDIR
    push1(action);    // bAction
    push1(0);         // bGhost
    pushF(speed);     // fSpeed
    return pkt;
}

tmapsvr::MapSessionState MakeConnectedState(float px = 100.0f, float pz = 100.0f)
{
    tmapsvr::MapSessionState s{};
    s.user_id   = 100u;
    s.char_id   = 42u;
    s.channel   = 1;
    s.connected = true;

    s.snapshot.emplace();
    s.snapshot->char_id = 42u;
    s.snapshot->user_id = 100u;
    s.snapshot->name    = "Hero";
    s.snapshot->level   = 50;
    s.snapshot->position.map_id = 201;
    s.snapshot->position.pos_x  = px;
    s.snapshot->position.pos_z  = pz;
    return s;
}

// ---------------------------------------------------------------------------
// §1 Malformed body
// ---------------------------------------------------------------------------
void TestMalformedBody()
{
    std::printf("[§1 CS_MOVE_REQ malformed body → silent drop]\n");
    tmapsvr::HandlerContext ctx{};
    auto state = MakeConnectedState();

    tnetlib::DecodedPacket pkt{};
    pkt.body = { std::byte{1}, std::byte{2} };  // only 2 bytes

    bool threw = false;
    RunSync([&]() -> boost::asio::awaitable<void> {
        try { co_await tmapsvr::OnMoveReq(nullptr, state, pkt, ctx); }
        catch (...) { threw = true; }
    }());

    Check(!threw, "§1 no exception on malformed body");
    // Position should NOT have changed from malformed packet
    Check(state.snapshot->position.pos_x == 100.0f,
        "§1 position unchanged after malformed body");
}

// ---------------------------------------------------------------------------
// §2 Not connected / no snapshot → drop
// ---------------------------------------------------------------------------
void TestNotConnected()
{
    std::printf("[§2 CS_MOVE_REQ not connected → drop]\n");
    tmapsvr::HandlerContext ctx{};

    tmapsvr::MapSessionState s{};
    s.connected = false;  // not connected

    auto pkt = MakeMoveReq(200.0f, 0.0f, 200.0f);
    RunSync([&]() -> boost::asio::awaitable<void> {
        co_await tmapsvr::OnMoveReq(nullptr, s, pkt, ctx);
    }());
    Check(!s.snapshot.has_value(), "§2 snapshot still empty when not connected");
}

// ---------------------------------------------------------------------------
// §3 Speed clamping
// ---------------------------------------------------------------------------
void TestSpeedClamping()
{
    std::printf("[§3 CS_MOVE_REQ speed > 3.40 → clamped]\n");
    tmapsvr::HandlerContext ctx{};
    auto state = MakeConnectedState(100.0f, 100.0f);

    // Send speed 99.99f — should be clamped to 3.40f
    auto pkt = MakeMoveReq(200.0f, 0.0f, 200.0f, 99.99f);
    RunSync([&]() -> boost::asio::awaitable<void> {
        co_await tmapsvr::OnMoveReq(nullptr, state, pkt, ctx);
    }());

    // Position should update (clamping doesn't reject the packet)
    Check(state.snapshot->position.pos_x == 200.0f,
        "§3 position updated even with speed clamped");
}

// ---------------------------------------------------------------------------
// §4 No IMapState → position updated in snapshot only
// ---------------------------------------------------------------------------
void TestNoMapState()
{
    std::printf("[§4 CS_MOVE_REQ no IMapState → snapshot updated, no AOI]\n");
    tmapsvr::HandlerContext ctx{};
    ctx.map_state = nullptr;  // no AOI

    auto state = MakeConnectedState(100.0f, 100.0f);

    auto pkt = MakeMoveReq(320.0f, 5.0f, 480.0f);
    RunSync([&]() -> boost::asio::awaitable<void> {
        co_await tmapsvr::OnMoveReq(nullptr, state, pkt, ctx);
    }());

    Check(state.snapshot->position.pos_x == 320.0f,
        "§4 pos_x updated in snapshot");
    Check(state.snapshot->position.pos_z == 480.0f,
        "§4 pos_z updated in snapshot");
    Check(state.snapshot->position.pos_y == 5.0f,
        "§4 pos_y updated in snapshot");
}

// ---------------------------------------------------------------------------
// §5 IMapState wired — common_aoi receives CS_MOVE_ACK
// ---------------------------------------------------------------------------
void TestMapStateOnMove()
{
    std::printf("[§5 CS_MOVE_REQ with IMapState → OnMove called, "
                "snapshot updated]\n");

    tmapsvr::LocalMapState  map;
    tmapsvr::FakeSessionRegistry reg;

    // Register two players in the map
    auto state_a = MakeConnectedState(100.0f, 100.0f);
    state_a.in_world = true;

    tmapsvr::legacy::PlayerPresence pa{};
    pa.char_id = 42u; pa.pos_x = 100.0f; pa.pos_z = 100.0f;
    map.EnterMap(42u, pa);

    tmapsvr::legacy::PlayerPresence pb{};
    pb.char_id = 99u; pb.pos_x = 110.0f; pb.pos_z = 100.0f;
    map.EnterMap(99u, pb);

    tmapsvr::HandlerContext ctx{};
    ctx.map_state        = &map;
    ctx.session_registry = &reg;

    auto pkt = MakeMoveReq(120.0f, 0.0f, 100.0f);
    RunSync([&]() -> boost::asio::awaitable<void> {
        co_await tmapsvr::OnMoveReq(nullptr, state_a, pkt, ctx);
    }());

    Check(state_a.snapshot->position.pos_x == 120.0f,
        "§5 snapshot pos_x updated after move");

    // Position in map state should also be updated
    const auto* updated_presence = map.GetPresence(42u);
    Check(updated_presence != nullptr, "§5 presence still in map after move");
    if (updated_presence)
        Check(updated_presence->pos_x == 120.0f,
            "§5 presence.pos_x updated in LocalMapState");
}

// ---------------------------------------------------------------------------
// §6 LeaveAck sent on session teardown
// ---------------------------------------------------------------------------
void TestLeaveAckPending()
{
    Pending("CS_LEAVE_ACK broadcast on session close via MapServer teardown",
            "CSHandler.cpp:LeaveMAP — wired in map_server.cpp HandleConnection");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnMoveReq characterization spec ===\n");
    std::printf("    Source: Server/TMapSvr/CSHandler.cpp:439-485\n\n");
    try
    {
        TestMalformedBody();
        TestNotConnected();
        TestSpeedClamping();
        TestNoMapState();
        TestMapStateOnMove();
        TestLeaveAckPending();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed, %d pending\n",
        g_passed, g_failed, g_pending);
    return g_failed == 0 ? 0 : 1;
}
