// Characterization test for CS_DEFEND_REQ (F4 Part 3).
//
// Source: Server/TMapSvr/CSHandler.cpp:1485-1518
//
// Wire body: 33 fields (DWORD×10, BYTE×8, WORD×5, FLOAT×6) ≥ 65 bytes.
//
// Branches:
//   §1  malformed body → drop
//   §2  not connected / no snapshot → drop
//   §3  valid body → CS_DEFEND_ACK broadcast to AOI
//   §4  player target (OT_PC=1) with player_hp wired → CS_HPMP_ACK
//   §5  full anti-cheat damage capping is PENDING F4 Part 4

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

// Build a minimal valid CS_DEFEND_REQ body (33 fields)
tnetlib::DecodedPacket MakeDefendReq(
    std::uint32_t attack_id  = 1,
    std::uint32_t target_id  = 42,
    std::uint8_t  target_type = 1,  // OT_PC
    std::uint32_t pys_max    = 200)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;

    tnetlib::DecodedPacket pkt{};
    pkt.wId = ToUint16(MessageId::CS_DEFEND_REQ);

    auto push4 = [&](std::uint32_t v) {
        for (int i = 0; i < 4; ++i)
            pkt.body.push_back(static_cast<std::byte>((v >> (8 * i)) & 0xFF));
    };
    auto push2 = [&](std::uint16_t v) {
        pkt.body.push_back(static_cast<std::byte>(v & 0xFF));
        pkt.body.push_back(static_cast<std::byte>((v >> 8) & 0xFF));
    };
    auto push1 = [&](std::uint8_t v) {
        pkt.body.push_back(static_cast<std::byte>(v));
    };
    auto pushF = [&](float v) {
        std::uint32_t bits; std::memcpy(&bits, &v, 4); push4(bits);
    };

    push4(0);            // dwHostID
    push4(attack_id);    // dwAttackID
    push4(target_id);    // dwTargetID
    push1(1);            // bAttackType (OT_PC)
    push1(target_type);  // bTargetType
    push2(0);            // wAttackPartyID
    push4(1001);         // dwActID
    push4(2001);         // dwAniID
    push1(1);            // bChannel
    push2(201);          // wMapID
    push1(10);           // bAttackerLevel
    push4(100);          // dwPysMinPower
    push4(pys_max);      // dwPysMaxPower
    push4(0);            // dwMgMinPower
    push4(0);            // dwMgMaxPower
    push2(0);            // wTransHP
    push2(0);            // wTransMP
    push1(0);            // bCurseProb
    push1(0);            // bEquipSpecial
    push1(1);            // bCanSelect
    push1(0);            // bAttackCountry
    push1(0);            // bAttackAidCountry
    push2(10);           // wAttackLevel
    push1(0);            // bCP
    push2(0);            // wSkillID
    push1(0);            // bSkillLevel
    pushF(100.0f); pushF(0.0f); pushF(100.0f);  // atk pos
    pushF(110.0f); pushF(0.0f); pushF(110.0f);  // def pos
    push4(1000);         // dwRemainTick
    return pkt;
}

tmapsvr::MapSessionState MakeConnectedState()
{
    tmapsvr::MapSessionState s{};
    s.user_id   = 100u;
    s.char_id   = 1u;
    s.connected = true;
    s.in_world  = true;
    s.snapshot.emplace();
    s.snapshot->char_id = 1u;
    s.snapshot->level   = 10;
    s.snapshot->position.pos_x = 100.0f;
    s.snapshot->position.pos_z = 100.0f;
    return s;
}

// ---------------------------------------------------------------------------
// §1 Malformed
// ---------------------------------------------------------------------------
void TestMalformed()
{
    std::printf("[§1 malformed body → drop]\n");
    tmapsvr::HandlerContext ctx{};
    auto state = MakeConnectedState();

    tnetlib::DecodedPacket pkt{};
    pkt.body = { std::byte{1}, std::byte{2} };

    bool threw = false;
    RunSync([&]() -> boost::asio::awaitable<void> {
        try { co_await tmapsvr::OnDefendReq(nullptr, state, pkt, ctx); }
        catch (...) { threw = true; }
    }());
    Check(!threw, "§1 no exception");
}

// ---------------------------------------------------------------------------
// §2 Not connected
// ---------------------------------------------------------------------------
void TestNotConnected()
{
    std::printf("[§2 not connected → drop]\n");
    tmapsvr::HandlerContext ctx{};
    tmapsvr::MapSessionState s{};
    s.connected = false;
    RunSync([&]() -> boost::asio::awaitable<void> {
        co_await tmapsvr::OnDefendReq(nullptr, s, MakeDefendReq(), ctx);
    }());
    Check(true, "§2 no exception");
}

// ---------------------------------------------------------------------------
// §3 Valid body → no crash, player HP reduced
// ---------------------------------------------------------------------------
void TestValidBody()
{
    std::printf("[§3 valid CS_DEFEND_REQ → player HP reduced]\n");

    tmapsvr::LocalMapState         map;
    tmapsvr::FakeSessionRegistry   sessions;
    tmapsvr::LocalPlayerHpRegistry php;
    php.Register(42u, 5000, 5000, 1000, 1000);  // target player

    tmapsvr::HandlerContext ctx{};
    ctx.map_state        = &map;
    ctx.session_registry = &sessions;
    ctx.player_hp        = &php;

    auto state = MakeConnectedState();  // attacker is char_id=1

    // Register attacker in map_state so AOI query works
    tmapsvr::legacy::PlayerPresence atk{};
    atk.char_id = 1u; atk.pos_x = 100.0f; atk.pos_z = 100.0f;
    map.EnterMap(1u, atk);

    RunSync([&]() -> boost::asio::awaitable<void> {
        co_await tmapsvr::OnDefendReq(nullptr, state,
            MakeDefendReq(1u, 42u, 1u, 200u), ctx);
    }());

    // Target player HP should be reduced by pys_max = 200
    const auto* v = php.Get(42u);
    Check(v != nullptr, "§3 target still registered");
    if (v)
        Check(v->hp == 4800u, "§3 HP reduced by pys_max=200 (5000-200=4800)");
}

// ---------------------------------------------------------------------------
// §4 Anti-cheat damage capping → PENDING
// ---------------------------------------------------------------------------
void TestDamageCap()
{
    Pending("CS_DEFEND_REQ: validate pys_max ≤ server-computed max (anti-cheat)",
            "CSHandler.cpp:1738 Defend() — requires stat tables (F4 Part 4)");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnDefendReq characterization spec ===\n");
    std::printf("    Source: Server/TMapSvr/CSHandler.cpp:1485-1518\n\n");
    try
    {
        TestMalformed();
        TestNotConnected();
        TestValidBody();
        TestDamageCap();
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
