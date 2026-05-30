// Unit test: the physical damage formula (services/damage_formula.h) — a
// faithful port of legacy CalcDamage's A/B/rand core (TObjBase.cpp:392-398).
// The bounds are pure (tested exactly); the roll is tested through a
// deterministic stub so there's no RNG in the assertion.

#include "services/damage_formula.h"

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

    // --- PhysDamageBounds: defense subtraction + 5/7 floors ----------
    {
        // Plain case: lo = min-def, hi = max-def.
        auto a = PhysDamageBounds(100, 200, 50);
        EXPECT(a.lo == 50 && a.hi == 150);

        // No defense: lo = min, hi = max.
        auto b = PhysDamageBounds(100, 200, 0);
        EXPECT(b.lo == 100 && b.hi == 200);

        // Over-armored: both endpoints floor at the legacy 5 / 7.
        auto c = PhysDamageBounds(10, 12, 100);
        EXPECT(c.lo == 5 && c.hi == 7);

        // lo floors at 5 but hi survives.
        auto d = PhysDamageBounds(53, 60, 50);   // 3→5, 10
        EXPECT(d.lo == 5 && d.hi == 10);

        // hi floors at 7 even when max-def == 4.
        auto e = PhysDamageBounds(100, 200, 196); // -96→5, 4→7
        EXPECT(e.lo == 5 && e.hi == 7);
    }

    // --- RollPhysicalDamage: lo + rand_below(span) -------------------
    {
        // The stub sees span == hi - lo, and a 0 roll yields lo.
        std::uint32_t seen_span = 0;
        auto roll0 = RollPhysicalDamage(100, 200, 50,
            [&](std::uint32_t n){ seen_span = n; return 0u; });
        EXPECT(seen_span == 100);          // 150 - 50
        EXPECT(roll0 == 50);               // lo + 0

        // Top of the range → hi - 1.
        auto roll_top = RollPhysicalDamage(100, 200, 50,
            [](std::uint32_t n){ return n - 1; });
        EXPECT(roll_top == 149);           // lo + span - 1

        // min == max above the floors → lo == hi → span clamps to 1
        // (legacy max(B-A,1)); the stub is asked for [0,1) and gets lo.
        std::uint32_t seen_deg = 999;
        auto roll_deg = RollPhysicalDamage(100, 100, 50,
            [&](std::uint32_t n){ seen_deg = n; return 0u; });
        EXPECT(seen_deg == 1 && roll_deg == 50);
    }

    if (g_fails == 0)
        std::printf("test_damage_formula: bounds floors + roll span OK\n");
    return g_fails == 0 ? 0 : 1;
}
