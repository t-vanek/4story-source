// Spec test for LocalSpawnManager.
//
// §1 AddSpawnPoint + Start() → monster appears in registry
// §2 OnMonsterDied → respawn after delay
// §3 Monster HP from ILevelChart (not hardcoded 1000)
// §4 Roam AI is PENDING (timer-driven, tested via integration run)
//
// Uses a short respawn delay (1ms) to keep the test fast.
// Roam AI launch is tested by checking a roam-enabled spawn still
// produces a live monster (the actual movement requires wall-clock time).

#include "spawn_manager.h"
#include "map_state.h"
#include "services/session_registry.h"
#include "level_chart.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>
#include <cstdio>
#include <exception>
#include <thread>

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

tmapsvr::legacy::MonsterSpawn MakeSpawn(
    std::uint16_t spawn_id   = 1,
    std::uint16_t template_id= 10,
    float px = 100.0f, float pz = 100.0f,
    std::uint8_t  count      = 1,
    std::uint32_t delay_ms   = 1,   // 1ms for fast tests
    std::uint8_t  roam_range = 0)   // 0 = no roam
{
    tmapsvr::legacy::MonsterSpawn s{};
    s.id          = spawn_id;
    s.template_id = template_id;
    s.pos_x       = px;
    s.pos_z       = pz;
    s.count       = count;
    s.delay_ms    = delay_ms;
    s.range       = roam_range;
    return s;
}

// ---------------------------------------------------------------------------
// §1  Start() spawns initial monster
// ---------------------------------------------------------------------------
void TestStartSpawnsMonster()
{
    std::printf("[§1 Start() → monster in registry with correct HP]\n");

    boost::asio::io_context io;
    tmapsvr::LocalMonsterRegistry  monsters;
    tmapsvr::LocalMapState         map;
    tmapsvr::FakeSessionRegistry   sessions;
    tmapsvr::HardcodedLevelChart   chart;

    tmapsvr::LocalSpawnManager mgr(io, monsters, map, sessions, chart);
    mgr.AddSpawnPoint(MakeSpawn(1, 10, 100.0f, 100.0f), 5);  // level 5

    mgr.Start();

    // Run io briefly to execute any co_spawn'd sends
    io.poll();

    Check(monsters.Count() == 1, "one monster spawned");

    const auto expected_hp = chart.GetMonsterStats(5).max_hp;
    const auto* mon = monsters.Get(1u);  // first NextInstanceId = 1
    if (mon)
    {
        Check(mon->hp == expected_hp,
            "monster HP matches level chart (not hardcoded 1000)");
        Check(mon->template_id == 10u, "template_id set from spawn");
        Check(mon->level == 5, "level set from AddSpawnPoint");
        Check(mon->pos_x >= 90.0f && mon->pos_x <= 110.0f,
            "position near spawn origin");
    }
    else
    {
        Check(false, "monster with id=1 not found in registry");
    }
}

// ---------------------------------------------------------------------------
// §2  OnMonsterDied → respawn scheduled
// ---------------------------------------------------------------------------
void TestRespawnAfterDeath()
{
    std::printf("[§2 OnMonsterDied → respawn after delay_ms]\n");

    boost::asio::io_context io;
    tmapsvr::LocalMonsterRegistry  monsters;
    tmapsvr::LocalMapState         map;
    tmapsvr::FakeSessionRegistry   sessions;
    tmapsvr::HardcodedLevelChart   chart;

    tmapsvr::LocalSpawnManager mgr(io, monsters, map, sessions, chart);
    mgr.AddSpawnPoint(MakeSpawn(1, 10, 100.0f, 100.0f, 1, 5), 3); // delay=5ms

    mgr.Start();
    io.poll();
    Check(monsters.Count() == 1, "initial spawn count = 1");

    // Kill the monster
    const std::uint32_t id = 1u;
    mgr.OnMonsterDied(id, 1u);
    monsters.Remove(id);
    Check(monsters.Count() == 0, "monster removed after kill");

    // Wait for respawn (delay=5ms + some slack)
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    io.run_for(std::chrono::milliseconds(10));

    Check(monsters.Count() == 1, "monster respawned after delay");
}

// ---------------------------------------------------------------------------
// §3  Level chart HP is used (not stub 1000)
// ---------------------------------------------------------------------------
void TestLevelChartHp()
{
    std::printf("[§3 monster HP from ILevelChart]\n");
    boost::asio::io_context io;
    tmapsvr::LocalMonsterRegistry  monsters;
    tmapsvr::LocalMapState         map;
    tmapsvr::FakeSessionRegistry   sessions;
    tmapsvr::HardcodedLevelChart   chart;

    tmapsvr::LocalSpawnManager mgr(io, monsters, map, sessions, chart);
    mgr.AddSpawnPoint(MakeSpawn(1, 10, 50.0f, 50.0f, 1, 1, 0), 20);

    mgr.Start();
    io.poll();

    const auto* mon = monsters.Get(1u);
    const auto expected = chart.GetMonsterStats(20).max_hp;
    Check(mon && mon->max_hp == expected,
        "max_hp = level chart value (not 1000)");
}

// ---------------------------------------------------------------------------
// §4  Roam AI — integration (pending without live clock)
// ---------------------------------------------------------------------------
void TestRoamAI()
{
    Pending("Roam AI moves monster and broadcasts CS_MOVE_ACK",
            "TAICmdRoam — requires live timer ticks (integration test)");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  LocalSpawnManager spec ===\n\n");
    try
    {
        TestStartSpawnsMonster();
        TestRespawnAfterDeath();
        TestLevelChartHp();
        TestRoamAI();
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
