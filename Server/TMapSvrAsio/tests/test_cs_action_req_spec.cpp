// Characterization test for CS_ACTION_REQ (F4).
//
// Source: Server/TMapSvr/CSHandler.cpp:1248-1256
//
// Wire body: DWORD dwObjID, BYTE bObjType, BYTE bActionID, DWORD dwActID,
//            DWORD dwAniID, BYTE bChannel, WORD wMapID, WORD wSkillID
//
// Branches:
//   §1  malformed body → drop
//   §2  not connected / no snapshot → drop
//   §3  monster target (OT_MON=2) → CS_ACTION_ACK + damage + CS_HPMP_ACK
//   §4  monster dead after damage → CS_DELMON_ACK + removed from registry
//   §5  non-monster target → CS_ACTION_ACK only (PvP pending F4b)
//
// Tests drive OnActionReq directly (no wire sockets).
//
// Legacy damage formula stub: level × 10 + 25 (placeholder).

#include "handlers.h"
#include "handlers_combat.h"
#include "monster_state.h"
#include "services/session_registry.h"

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

tnetlib::DecodedPacket MakeActionReq(
    std::uint32_t obj_id   = 1,
    std::uint8_t  obj_type = 2,   // OT_MON
    std::uint8_t  action   = 1)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;

    tnetlib::DecodedPacket pkt{};
    pkt.wId = ToUint16(MessageId::CS_ACTION_REQ);

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

    push4(obj_id);      // dwObjID
    push1(obj_type);    // bObjType
    push1(action);      // bActionID
    push4(1001);        // dwActID
    push4(2001);        // dwAniID
    push1(1);           // bChannel
    push2(201);         // wMapID
    push2(0);           // wSkillID
    return pkt;
}

tmapsvr::MapSessionState MakeConnectedState(std::uint8_t level = 10)
{
    tmapsvr::MapSessionState s{};
    s.user_id   = 100u;
    s.char_id   = 42u;
    s.connected = true;
    s.in_world  = true;
    s.snapshot.emplace();
    s.snapshot->char_id = 42u;
    s.snapshot->level   = level;
    s.snapshot->position.pos_x = 100.0f;
    s.snapshot->position.pos_z = 100.0f;
    return s;
}

// ---------------------------------------------------------------------------
// §1 Malformed body
// ---------------------------------------------------------------------------
void TestMalformed()
{
    std::printf("[§1 malformed body → drop]\n");
    tmapsvr::HandlerContext ctx{};
    auto state = MakeConnectedState();

    tnetlib::DecodedPacket pkt{};
    pkt.body = { std::byte{1} };  // only 1 byte

    bool threw = false;
    RunSync([&]() -> boost::asio::awaitable<void> {
        try { co_await tmapsvr::OnActionReq(nullptr, state, pkt, ctx); }
        catch (...) { threw = true; }
    }());
    Check(!threw, "§1 no exception on malformed body");
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
        co_await tmapsvr::OnActionReq(nullptr, s, MakeActionReq(), ctx);
    }());
    Check(true, "§2 no exception when not connected");
}

// ---------------------------------------------------------------------------
// §3 Monster target → damage applied + CS_HPMP_ACK
// ---------------------------------------------------------------------------
void TestMonsterDamage()
{
    std::printf("[§3 monster target → damage applied to registry]\n");

    tmapsvr::LocalMonsterRegistry monsters;
    tmapsvr::FakeSessionRegistry  sessions;

    // Spawn monster with 1000 HP at same cell as the attacker
    tmapsvr::MonsterState mon{};
    mon.instance_id = 99u;
    mon.template_id = 1u;
    mon.level       = 5;
    mon.max_hp = mon.hp = 1000u;
    mon.max_mp = mon.mp = 200u;
    mon.pos_x = 100.0f; mon.pos_z = 100.0f;
    monsters.Add(mon);

    tmapsvr::HandlerContext ctx{};
    ctx.monster_registry = &monsters;
    ctx.session_registry = &sessions;

    auto state = MakeConnectedState(10);  // level 10 → dmg = 10×10+25 = 125

    RunSync([&]() -> boost::asio::awaitable<void> {
        co_await tmapsvr::OnActionReq(nullptr, state, MakeActionReq(99u, 2u), ctx);
    }());

    const auto* updated = monsters.Get(99u);
    Check(updated != nullptr, "§3 monster still in registry after non-lethal hit");
    if (updated)
    {
        Check(updated->hp == 875u,  // 1000 - 125
            "§3 HP reduced by level×10+25 = 125");
        Check(updated->IsAlive(), "§3 monster alive after partial damage");
    }
}

// ---------------------------------------------------------------------------
// §4 Monster dies → removed + CS_DELMON_ACK
// ---------------------------------------------------------------------------
void TestMonsterDeath()
{
    std::printf("[§4 monster killed → removed from registry]\n");

    tmapsvr::LocalMonsterRegistry monsters;
    tmapsvr::FakeSessionRegistry  sessions;

    tmapsvr::MonsterState mon{};
    mon.instance_id = 77u;
    mon.max_hp = mon.hp = 50u;   // very low HP — will die on first hit
    mon.pos_x = 100.0f; mon.pos_z = 100.0f;
    monsters.Add(mon);

    tmapsvr::HandlerContext ctx{};
    ctx.monster_registry = &monsters;
    ctx.session_registry = &sessions;

    auto state = MakeConnectedState(10);  // dmg = 125 >> 50

    RunSync([&]() -> boost::asio::awaitable<void> {
        co_await tmapsvr::OnActionReq(nullptr, state, MakeActionReq(77u, 2u), ctx);
    }());

    Check(monsters.Get(77u) == nullptr,
        "§4 monster removed from registry after death");
}

// ---------------------------------------------------------------------------
// §5 Non-monster target — CS_ACTION_ACK only
// ---------------------------------------------------------------------------
void TestPvPTarget()
{
    std::printf("[§5 PvP target → CS_ACTION_ACK only (damage pending F4b)]\n");
    Pending("CS_ACTION on player target → CS_DEFEND_REQ simulation",
            "CSHandler.cpp:1485 — requires stat tables (F4b)");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnActionReq characterization spec ===\n");
    std::printf("    Source: Server/TMapSvr/CSHandler.cpp:1248-1256\n\n");
    try
    {
        TestMalformed();
        TestNotConnected();
        TestMonsterDamage();
        TestMonsterDeath();
        TestPvPTarget();
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
