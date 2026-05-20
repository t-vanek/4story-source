// Characterization test for CS_REVIVAL_REQ (F4 Part 4).
//
// Source: Server/TMapSvr/CSHandler.cpp:1067-1098
//
// Wire body: FLOAT fPosX, fPosY, fPosZ, BYTE bType  (13 bytes)
//
// Branches:
//   §1  not in world or not dead → drop
//   §2  valid body + is_dead=true → HP restored, is_dead cleared
//   §3  CS_REVIVAL_ACK broadcast to AOI
//   §4  map position updated after revival

#include "handlers.h"
#include "handlers_combat.h"
#include "map_state.h"
#include "services/session_registry.h"
#include "player_hp_registry.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>

#include <cstring>
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

tnetlib::DecodedPacket MakeRevivalReq(
    float px = 200.0f, float py = 5.0f, float pz = 200.0f,
    std::uint8_t type = 0)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;

    tnetlib::DecodedPacket pkt{};
    pkt.wId = ToUint16(MessageId::CS_REVIVAL_REQ);

    auto pushF = [&](float v) {
        std::uint32_t bits; std::memcpy(&bits, &v, 4);
        for (int i = 0; i < 4; ++i)
            pkt.body.push_back(static_cast<std::byte>((bits >> (8 * i)) & 0xFF));
    };
    auto push1 = [&](std::uint8_t v) {
        pkt.body.push_back(static_cast<std::byte>(v));
    };

    pushF(px); pushF(py); pushF(pz);
    push1(type);
    return pkt;
}

tmapsvr::MapSessionState MakeDeadState()
{
    tmapsvr::MapSessionState s{};
    s.user_id   = 100u;
    s.char_id   = 42u;
    s.connected = true;
    s.in_world  = true;
    s.is_dead   = true;
    s.snapshot.emplace();
    s.snapshot->char_id = 42u;
    s.snapshot->position.pos_x = 100.0f;
    s.snapshot->position.pos_z = 100.0f;
    return s;
}

// ---------------------------------------------------------------------------
// §1  Not in world → drop
// ---------------------------------------------------------------------------
void TestDropIfNotInWorld()
{
    std::printf("[§1 not in world → drop]\n");
    tmapsvr::HandlerContext ctx{};
    tmapsvr::MapSessionState s{};
    s.connected = true;
    s.in_world  = false;
    s.is_dead   = true;

    RunSync([&]() -> boost::asio::awaitable<void> {
        co_await tmapsvr::OnRevivalReq(nullptr, s, MakeRevivalReq(), ctx);
    }());
    Check(s.is_dead, "§1 is_dead unchanged when not in world");
}

// ---------------------------------------------------------------------------
// §1b  Still alive → drop
// ---------------------------------------------------------------------------
void TestDropIfAlive()
{
    std::printf("[§1b not dead → drop]\n");
    tmapsvr::HandlerContext ctx{};
    auto s = MakeDeadState();
    s.is_dead = false;  // alive

    RunSync([&]() -> boost::asio::awaitable<void> {
        co_await tmapsvr::OnRevivalReq(nullptr, s, MakeRevivalReq(), ctx);
    }());
    Check(!s.is_dead, "§1b is_dead stays false (was already alive)");
}

// ---------------------------------------------------------------------------
// §2  Revival restores HP + clears is_dead
// ---------------------------------------------------------------------------
void TestRevivalRestoresHp()
{
    std::printf("[§2 revival → HP restored, is_dead cleared]\n");

    tmapsvr::LocalPlayerHpRegistry php;
    php.Register(42u, 0, 8000, 0, 2000);  // dead (HP=0, max=8000)

    tmapsvr::LocalMapState       map;
    tmapsvr::FakeSessionRegistry sessions;

    tmapsvr::legacy::PlayerPresence p{};
    p.char_id = 42u; p.pos_x = 100.0f; p.pos_z = 100.0f;
    map.EnterMap(42u, p);

    tmapsvr::HandlerContext ctx{};
    ctx.player_hp        = &php;
    ctx.map_state        = &map;
    ctx.session_registry = &sessions;

    auto state = MakeDeadState();

    RunSync([&]() -> boost::asio::awaitable<void> {
        co_await tmapsvr::OnRevivalReq(nullptr, state,
            MakeRevivalReq(200.0f, 5.0f, 200.0f), ctx);
    }());

    Check(!state.is_dead, "§2 is_dead = false after revival");

    const auto* v = php.Get(42u);
    Check(v != nullptr && v->hp == 8000u,
        "§2 HP restored to max_hp after revival");
}

// ---------------------------------------------------------------------------
// §3  Position updated in snapshot
// ---------------------------------------------------------------------------
void TestPositionUpdated()
{
    std::printf("[§3 snapshot position updated to revival pos]\n");

    tmapsvr::LocalPlayerHpRegistry php;
    php.Register(42u, 0, 5000, 0, 1000);

    tmapsvr::LocalMapState       map;
    tmapsvr::FakeSessionRegistry sessions;
    tmapsvr::legacy::PlayerPresence p{};
    p.char_id = 42u; p.pos_x = 100.0f; p.pos_z = 100.0f;
    map.EnterMap(42u, p);

    tmapsvr::HandlerContext ctx{};
    ctx.player_hp        = &php;
    ctx.map_state        = &map;
    ctx.session_registry = &sessions;

    auto state = MakeDeadState();

    RunSync([&]() -> boost::asio::awaitable<void> {
        co_await tmapsvr::OnRevivalReq(nullptr, state,
            MakeRevivalReq(300.0f, 10.0f, 350.0f), ctx);
    }());

    Check(state.snapshot && state.snapshot->position.pos_x == 300.0f,
        "§3 snapshot pos_x updated to revival pos");
    Check(state.snapshot && state.snapshot->position.pos_z == 350.0f,
        "§3 snapshot pos_z updated to revival pos");
}

// ---------------------------------------------------------------------------
// §4  Aftermath penalties / NPC revival → PENDING
// ---------------------------------------------------------------------------
void TestAftermath()
{
    Pending("Aftermath penalties on revival (stat debuffs for GHOST/ATONCE)",
            "CSHandler.cpp:1084 pPlayer->Revival(AFTERMATH_*) — F5");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnRevivalReq characterization spec ===\n");
    std::printf("    Source: Server/TMapSvr/CSHandler.cpp:1067-1098\n\n");
    try
    {
        TestDropIfNotInWorld();
        TestDropIfAlive();
        TestRevivalRestoresHp();
        TestPositionUpdated();
        TestAftermath();
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
