// Spec test for HardcodedLevelChart.
//
// Verifies formulas are monotonically increasing and produce sane
// values at level 1 / 10 / 50 / 100. Tests do NOT pin exact values
// (they will change when the real TLEVELCHART table is loaded);
// they only enforce invariants.
//
// Also tests CalcBaseDamage(level) is positive and increasing.

#include "level_chart.h"

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

void TestMonsterStats()
{
    std::printf("[HardcodedLevelChart::GetMonsterStats — invariants]\n");
    tmapsvr::HardcodedLevelChart chart;

    const auto s1  = chart.GetMonsterStats(1);
    const auto s10 = chart.GetMonsterStats(10);
    const auto s50 = chart.GetMonsterStats(50);
    const auto s100= chart.GetMonsterStats(100);

    Check(s1.max_hp   > 0,    "level 1 max_hp > 0");
    Check(s10.max_hp  > s1.max_hp,  "level 10 > level 1");
    Check(s50.max_hp  > s10.max_hp, "level 50 > level 10");
    Check(s100.max_hp > s50.max_hp, "level 100 > level 50");

    Check(s1.max_mp  > 0,   "level 1 max_mp > 0");
    Check(s10.max_mp > s1.max_mp, "max_mp monotonically increasing");

    Check(s1.exp_give  > 0,    "level 1 exp > 0");
    Check(s100.exp_give > s1.exp_give, "exp monotonically increasing");

    // Sanity: level-5 monster should have HP in range [200, 10000]
    const auto s5 = chart.GetMonsterStats(5);
    Check(s5.max_hp >= 200 && s5.max_hp <= 10000,
        "level 5 HP in [200, 10000]");

    std::printf("  lv1 hp=%u  lv10 hp=%u  lv50 hp=%u  lv100 hp=%u\n",
        s1.max_hp, s10.max_hp, s50.max_hp, s100.max_hp);
}

void TestPlayerStats()
{
    std::printf("[HardcodedLevelChart::GetPlayerStats — invariants]\n");
    tmapsvr::HardcodedLevelChart chart;

    const auto p1  = chart.GetPlayerStats(1);
    const auto p50 = chart.GetPlayerStats(50);

    Check(p1.max_hp  > 0,   "player lv1 max_hp > 0");
    Check(p50.max_hp > p1.max_hp, "player HP monotonically increasing");
    Check(p1.exp_give == 0, "players give 0 exp on kill");
}

void TestCalcBaseDamage()
{
    std::printf("[CalcBaseDamage — positive + monotone]\n");

    const auto d1  = tmapsvr::CalcBaseDamage(1);
    const auto d10 = tmapsvr::CalcBaseDamage(10);
    const auto d50 = tmapsvr::CalcBaseDamage(50);

    Check(d1  > 0,  "damage at level 1 > 0");
    Check(d10 > d1, "damage increases with level");
    Check(d50 > d10,"damage at 50 > damage at 10");
}

void TestLevelZeroSafe()
{
    std::printf("[Level 0 → treated as level 1]\n");
    tmapsvr::HardcodedLevelChart chart;
    const auto s0 = chart.GetMonsterStats(0);
    const auto s1 = chart.GetMonsterStats(1);
    Check(s0.max_hp == s1.max_hp, "level 0 clamped to level 1 HP");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  HardcodedLevelChart spec ===\n\n");
    try
    {
        TestMonsterStats();
        TestPlayerStats();
        TestCalcBaseDamage();
        TestLevelZeroSafe();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
