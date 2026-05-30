// Unit test: money.h — three-tier gold/silver/cooper arithmetic + the
// monster money-drop roll, faithful to NetCode.h (MONEY_MULTIPLY = 1000)
// and TMonster.cpp:1085.

#include "services/money.h"
#include "domain/character.h"

#include <cstdint>
#include <cstdio>

namespace {
int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)
} // namespace

int main()
{
    using namespace tmapsvr;

    // --- CalcMoney / SplitMoney round-trip + carry -------------------
    {
        EXPECT(CalcMoney(0, 0, 0)   == 0);
        EXPECT(CalcMoney(0, 0, 500) == 500);
        EXPECT(CalcMoney(0, 1, 0)   == 1000);
        EXPECT(CalcMoney(1, 0, 0)   == 1000000);
        EXPECT(CalcMoney(2, 3, 4)   == 2 * 1000000 + 3 * 1000 + 4);

        std::uint32_t g = 9, s = 9, c = 9;
        SplitMoney(0, g, s, c);         EXPECT(g == 0 && s == 0 && c == 0);
        SplitMoney(1500, g, s, c);      EXPECT(g == 0 && s == 1 && c == 500);
        SplitMoney(1000000, g, s, c);   EXPECT(g == 1 && s == 0 && c == 0);
        SplitMoney(2003004, g, s, c);   EXPECT(g == 2 && s == 3 && c == 4);
        SplitMoney(-5, g, s, c);        EXPECT(g == 0 && s == 0 && c == 0); // clamp

        const std::int64_t total = CalcMoney(7, 42, 999);
        SplitMoney(total, g, s, c);     EXPECT(g == 7 && s == 42 && c == 999);
    }

    // --- AddMoneyToChar carries across tiers -------------------------
    {
        CharSnapshot s;
        s.dwGold = 0; s.dwSilver = 0; s.dwCooper = 800;
        AddMoneyToChar(s, 500);    // 1300 → 1 silver 300 cooper
        EXPECT(s.dwGold == 0 && s.dwSilver == 1 && s.dwCooper == 300);

        AddMoneyToChar(s, 999000); // 1,000,300 → 1 gold 0 silver 300 cooper
        EXPECT(s.dwGold == 1 && s.dwSilver == 0 && s.dwCooper == 300);
    }

    // --- RollMoneyDrop: prob gate + range ----------------------------
    {
        // prob 0 → never drops (0 > rand[0,99] is always false).
        EXPECT(RollMoneyDrop(0, 10, 100, [](std::uint32_t){ return 0u; }) == 0);

        // prob 100 passes the gate; min == max → empty range → 0.
        EXPECT(RollMoneyDrop(100, 50, 50, [](std::uint32_t){ return 0u; }) == 0);

        // prob 100, valid range, rand_below 0 → min.
        EXPECT(RollMoneyDrop(100, 50, 100, [](std::uint32_t){ return 0u; }) == 50);

        // gate passes (call 1 → 0), range top (call 2 → n-1) → max-1.
        int call = 0;
        auto stub = [&](std::uint32_t n) -> std::uint32_t
        { ++call; return (call == 1) ? 0u : (n - 1); };
        EXPECT(RollMoneyDrop(100, 50, 100, stub) == 99);   // 50 + (50-1)
    }

    if (g_fails == 0)
        std::printf("test_money: calc/split/add carry + drop roll OK\n");
    return g_fails == 0 ? 0 : 1;
}
