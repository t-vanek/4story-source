// Unit test: stat_engine.h — the pure max-HP/MP derivation, faithful to
// the legacy TObjBase formula. Uses the live-DB coefficients verified at
// port time: FTYPE_HP/MP rateX = 20.66, FTYPE_1ST rateX = 1.03,
// BASE_STAT = 1. Warrior (class0/race0) CON=1+9+13=23, MEN=1+11+5=17.

#include "services/stat_engine.h"
#include "services/stat_chart.h"
#include "domain/stat.h"

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

// Build a chart seeded with the real live-DB coefficients + a couple of
// class/race rows so the math is exercised against known-good numbers.
tmapsvr::InMemoryStatChart MakeChart()
{
    using namespace tmapsvr;
    InMemoryStatChart c;
    c.AddFormula(FTYPE_HP,  FormulaRow{ 0, 20.66f, 0.f });
    c.AddFormula(FTYPE_MP,  FormulaRow{ 0, 20.66f, 0.f });
    c.AddFormula(FTYPE_1ST, FormulaRow{ 0, 1.03f,  0.f });
    // class0 (warrior): CON=13 MEN=5 ; class3 (mage): CON=5 MEN=11
    c.AddClass(0, StatBlock{ 9, 9, 13, 0, 0, 5 });
    c.AddClass(3, StatBlock{ 0, 0, 5, 11, 9, 11 });
    // race0: CON=9 MEN=11
    c.AddRace(0, StatBlock{ 10, 11, 9, 9, 8, 11 });
    return c;
}
} // namespace

int main()
{
    using namespace tmapsvr;
    namespace se = tmapsvr::stat_engine;
    const InMemoryStatChart c = MakeChart();

    // --- Warrior L1: CON=1+9+13=23, rate^0=1 → HP=round(23*20.66)=475 ---
    {
        const auto hp = se::MaxHP(c, /*class*/0, /*race*/0, /*level*/1);
        const auto mp = se::MaxMP(c, 0, 0, 1);
        EXPECT(hp == 475);   // 23 * 20.66 = 475.18
        EXPECT(mp == 351);   // MEN=1+11+5=17 ; 17 * 20.66 = 351.22
    }

    // --- Growth: L10 applies pow(1.03, 9) ≈ 1.3048 ----------------------
    {
        // CON_pure = 23 * 1.03^9 = 30.01 ; *20.66 = 619.9 → 620
        EXPECT(se::MaxHP(c, 0, 0, 10) == 620);
        // MEN_pure = 17 * 1.03^9 = 22.18 ; *20.66 = 458.3 → 458
        EXPECT(se::MaxMP(c, 0, 0, 10) == 458);
    }

    // --- Level 0 and level 1 both yield exponent 0 (multiplier 1) -------
    {
        EXPECT(se::MaxHP(c, 0, 0, 0) == se::MaxHP(c, 0, 0, 1));
    }

    // --- Mage (class3) L10: CON=1+9+5=15, MEN=1+11+11=23 ----------------
    {
        // CON_pure=15*1.3048=19.57 ; *20.66=404.4 → 404
        EXPECT(se::MaxHP(c, 3, 0, 10) == 404);
        // MEN_pure=23*1.3048=30.01 ; *20.66=620.0 → 620
        EXPECT(se::MaxMP(c, 3, 0, 10) == 620);
        // Mage has more MP than HP; warrior the reverse — the class
        // identity the formula is supposed to produce.
        EXPECT(se::MaxMP(c, 3, 0, 10) > se::MaxHP(c, 3, 0, 10));
        EXPECT(se::MaxHP(c, 0, 0, 10) > se::MaxMP(c, 0, 0, 10));
    }

    // --- Missing data degrades safely: unknown class/race → BASE_STAT
    //     only (1) ; HP = round(1*20.66)=21, never 0 ---------------------
    {
        const auto hp = se::MaxHP(c, /*class*/250, /*race*/250, 1);
        EXPECT(hp == 21);   // (1+0+0) * 20.66 = 20.66 → 21
    }

    // --- Absent formula row → floored at 1, never 0 --------------------
    {
        InMemoryStatChart empty;   // no rows at all
        EXPECT(se::MaxHP(empty, 0, 0, 1) == 1);
        EXPECT(se::MaxMP(empty, 0, 0, 1) == 1);
    }

    if (g_fails == 0)
        std::printf("test_stat_engine: max-HP/MP formula + growth + safe "
                    "defaults OK\n");
    return g_fails == 0 ? 0 : 1;
}
