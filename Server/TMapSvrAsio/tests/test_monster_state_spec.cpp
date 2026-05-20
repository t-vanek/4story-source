// Spec test for LocalMonsterRegistry.
//
// Sections:
//   §1  Add / Get / Count / Remove
//   §2  ApplyHpDelta — damage, heal, clamp to [0, max_hp], death
//   §3  GetNeighborIds — 3×3 cell coverage (CELL_SIZE=64)
//   §4  NextInstanceId — monotonic counter
//
// Pure-unit (no io_context or sockets).

#include "monster_state.h"

#include <cstdio>
#include <exception>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

tmapsvr::MonsterState MakeMon(std::uint32_t id, float px, float pz,
                               std::uint32_t max_hp = 1000)
{
    tmapsvr::MonsterState m{};
    m.instance_id = id;
    m.template_id = 1;
    m.level       = 5;
    m.max_hp = m.hp = max_hp;
    m.max_mp = m.mp = 200;
    m.pos_x = px;
    m.pos_z = pz;
    return m;
}

// ---------------------------------------------------------------------------
void TestAddGetRemove()
{
    std::printf("[§1 Add / Get / Count / Remove]\n");
    tmapsvr::LocalMonsterRegistry r;

    Check(r.Count() == 0, "empty registry has count 0");
    Check(r.Get(1) == nullptr, "Get on empty registry returns nullptr");

    r.Add(MakeMon(1, 100.0f, 100.0f));
    r.Add(MakeMon(2, 200.0f, 200.0f));

    Check(r.Count() == 2, "count = 2 after two adds");
    Check(r.Get(1) != nullptr, "Get(1) returns non-null");
    Check(r.Get(2) != nullptr, "Get(2) returns non-null");
    Check(r.Get(3) == nullptr, "Get(3) returns nullptr");

    if (auto* m = r.Get(1))
        Check(m->pos_x == 100.0f && m->pos_z == 100.0f,
            "Get returns correct position");

    Check(r.Remove(1),     "Remove(1) returns true");
    Check(!r.Remove(99),   "Remove(99) returns false (not found)");
    Check(r.Count() == 1,  "count = 1 after remove");
    Check(r.Get(1) == nullptr, "Get(1) returns nullptr after remove");
}

// ---------------------------------------------------------------------------
void TestApplyHpDelta()
{
    std::printf("[§2 ApplyHpDelta — damage, heal, clamp, death]\n");
    tmapsvr::LocalMonsterRegistry r;
    r.Add(MakeMon(10, 50.0f, 50.0f, 1000));

    // Normal damage
    auto hp = r.ApplyHpDelta(10, -200);
    Check(hp == 800u, "ApplyHpDelta -200 → hp=800");

    // Heal
    hp = r.ApplyHpDelta(10, +100);
    Check(hp == 900u, "ApplyHpDelta +100 → hp=900");

    // Clamp to 0
    hp = r.ApplyHpDelta(10, -5000);
    Check(hp == 0u, "ApplyHpDelta -5000 clamps to 0");
    Check(r.Get(10)->IsDead(), "IsDead() true when hp=0");

    // Clamp to max_hp
    hp = r.ApplyHpDelta(10, +9999);
    Check(hp == 1000u, "ApplyHpDelta +9999 clamps to max_hp=1000");
    Check(r.Get(10)->IsAlive(), "IsAlive() true after heal");

    // Unknown id
    auto hp2 = r.ApplyHpDelta(999, -10);
    Check(hp2 == 0u, "ApplyHpDelta on unknown id returns 0");
}

// ---------------------------------------------------------------------------
void TestGetNeighborIds()
{
    std::printf("[§3 GetNeighborIds — 3×3 cell coverage]\n");
    tmapsvr::LocalMonsterRegistry r;

    // Monster A: cell (1,1) = pos ~64–127
    r.Add(MakeMon(1, 80.0f, 80.0f));
    // Monster B: adjacent cell (2,1)
    r.Add(MakeMon(2, 140.0f, 80.0f));
    // Monster C: far away, cell (7,7)
    r.Add(MakeMon(3, 500.0f, 500.0f));

    auto near = r.GetNeighborIds(80.0f, 80.0f);
    bool has1 = false, has2 = false, has3 = false;
    for (auto id : near)
    {
        if (id == 1) has1 = true;
        if (id == 2) has2 = true;
        if (id == 3) has3 = true;
    }
    Check(has1, "monster in same cell is in neighbour query");
    Check(has2, "monster in adjacent cell is in neighbour query");
    Check(!has3, "far-away monster NOT in neighbour query");
}

// ---------------------------------------------------------------------------
void TestNextInstanceId()
{
    std::printf("[§4 NextInstanceId — monotonic]\n");
    tmapsvr::LocalMonsterRegistry r;

    auto id1 = r.NextInstanceId();
    auto id2 = r.NextInstanceId();
    auto id3 = r.NextInstanceId();
    Check(id1 > 0,    "first id > 0");
    Check(id2 > id1,  "second id > first");
    Check(id3 > id2,  "third id > second");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  LocalMonsterRegistry spec ===\n\n");
    try
    {
        TestAddGetRemove();
        TestApplyHpDelta();
        TestGetNeighborIds();
        TestNextInstanceId();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
