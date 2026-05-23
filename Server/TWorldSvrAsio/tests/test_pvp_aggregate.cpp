// W3a-28 unit test: CalcWeekRecord helper.
//
// Scenarios:
//   1. Empty vRecord → weekrecord stays zeros.
//   2. Single fresh entry → weekrecord matches that entry.
//   3. Mix of fresh + stale entries → weekrecord sums only the
//      fresh, vRecord shrinks (stale rows trimmed in-place).
//   4. All-stale → weekrecord stays zeros, vRecord empty.
//   5. Edge case: day_index + 7 == today (the boundary entry is
//      dropped, per legacy `+7 <= today` predicate).
//   6. Idempotent re-call on already-trimmed input is a no-op.

#include "../services/pvp_aggregate.h"
#include "../services/guild_constants.h"
#include "../services/guild_registry.h"

#include <cstdio>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

tworldsvr::TPvPDayRecord MakeDay(std::int64_t day_index,
                                  std::uint16_t kill,
                                  std::uint16_t die,
                                  std::uint32_t pt0 = 0)
{
    tworldsvr::TPvPDayRecord d;
    d.day_index  = day_index;
    d.kill_count = kill;
    d.die_count  = die;
    d.points[0]  = pt0;
    return d;
}

void Test_Empty()
{
    tworldsvr::TGuildMember m;
    tworldsvr::CalcWeekRecord(m, /*today=*/100);
    EXPECT(m.weekrecord.kill_count == 0);
    EXPECT(m.weekrecord.die_count  == 0);
    EXPECT(m.weekrecord.points[0]  == 0);
    EXPECT(m.vRecord.empty());
}

void Test_SingleFresh()
{
    tworldsvr::TGuildMember m;
    m.vRecord.push_back(MakeDay(100, 5, 2, 50));
    tworldsvr::CalcWeekRecord(m, 100);
    EXPECT(m.weekrecord.kill_count == 5);
    EXPECT(m.weekrecord.die_count  == 2);
    EXPECT(m.weekrecord.points[0]  == 50);
    EXPECT(m.vRecord.size() == 1);
}

void Test_MixedFreshAndStale()
{
    tworldsvr::TGuildMember m;
    // Today=100. Window keeps entries where day_index + 7 > 100,
    // i.e. day_index > 93, i.e. days 94..100 inclusive (7 days).
    m.vRecord.push_back(MakeDay(92, 99, 99, 999));   // stale (drop)
    m.vRecord.push_back(MakeDay(93, 99, 99, 999));   // stale (drop, boundary: 93+7==100)
    m.vRecord.push_back(MakeDay(95, 1, 1, 10));      // fresh
    m.vRecord.push_back(MakeDay(98, 2, 2, 20));      // fresh
    m.vRecord.push_back(MakeDay(100, 3, 3, 30));     // fresh
    tworldsvr::CalcWeekRecord(m, 100);
    EXPECT(m.weekrecord.kill_count == 6);   // 1+2+3
    EXPECT(m.weekrecord.die_count  == 6);
    EXPECT(m.weekrecord.points[0]  == 60);  // 10+20+30
    EXPECT(m.vRecord.size() == 3);          // 92, 93 dropped
}

void Test_AllStale()
{
    tworldsvr::TGuildMember m;
    m.vRecord.push_back(MakeDay(50, 5, 5, 50));
    m.vRecord.push_back(MakeDay(70, 7, 7, 70));
    tworldsvr::CalcWeekRecord(m, 100);
    EXPECT(m.weekrecord.kill_count == 0);
    EXPECT(m.weekrecord.die_count  == 0);
    EXPECT(m.weekrecord.points[0]  == 0);
    EXPECT(m.vRecord.empty());
}

void Test_BoundaryEntry()
{
    // Single entry at day_index = today - 7 = 93. Predicate is
    // `day_index + 7 <= today` → 93 + 7 = 100 <= 100 → TRUE,
    // so drop. Matches legacy behavior.
    tworldsvr::TGuildMember m;
    m.vRecord.push_back(MakeDay(93, 99, 99, 999));
    tworldsvr::CalcWeekRecord(m, 100);
    EXPECT(m.weekrecord.kill_count == 0);
    EXPECT(m.vRecord.empty());

    // Sanity check: shift by 1 — day_index = 94 stays.
    tworldsvr::TGuildMember n;
    n.vRecord.push_back(MakeDay(94, 99, 99, 999));
    tworldsvr::CalcWeekRecord(n, 100);
    EXPECT(n.weekrecord.kill_count == 99);
    EXPECT(n.vRecord.size() == 1);
}

void Test_Idempotent()
{
    tworldsvr::TGuildMember m;
    m.vRecord.push_back(MakeDay(95, 1, 1, 10));
    m.vRecord.push_back(MakeDay(100, 2, 2, 20));

    tworldsvr::CalcWeekRecord(m, 100);
    const auto kc_first = m.weekrecord.kill_count;
    const auto vsize_first = m.vRecord.size();

    // Re-call — should be a no-op (already trimmed, weekrecord
    // re-derived to the same value).
    tworldsvr::CalcWeekRecord(m, 100);
    EXPECT(m.weekrecord.kill_count == kc_first);
    EXPECT(m.vRecord.size() == vsize_first);
}

} // namespace

int main()
{
    Test_Empty();
    Test_SingleFresh();
    Test_MixedFreshAndStale();
    Test_AllStale();
    Test_BoundaryEntry();
    Test_Idempotent();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_pvp_aggregate "
                    "(6 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_pvp_aggregate "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
