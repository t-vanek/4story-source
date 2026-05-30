// Unit test: skill_cooldown.h — the reuse-cooldown gate (faithful
// CTSkill::CanUse / GetReuseRemainTick). Pure remain/can-use math + the
// per-(char, skill) tracker, all clock-free / deterministic.

#include "services/skill_cooldown.h"

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

    // --- ReuseRemainMs --------------------------------------------------
    EXPECT(ReuseRemainMs(0,    1000, 5000) == 0);     // never used → ready
    EXPECT(ReuseRemainMs(1000, 1000, 5000) == 5000);  // just used → full
    EXPECT(ReuseRemainMs(1000, 3000, 5000) == 3000);  // 2000 elapsed → 3000 left
    EXPECT(ReuseRemainMs(1000, 6000, 5000) == 0);     // exactly elapsed → ready
    EXPECT(ReuseRemainMs(1000, 9000, 5000) == 0);     // long past → ready
    EXPECT(ReuseRemainMs(5000, 1000, 5000) == 0);     // clock backwards → ready
    EXPECT(ReuseRemainMs(1000, 1000, 0)    == 0);     // zero-delay → always ready

    // --- CanUseSkill ----------------------------------------------------
    EXPECT(CanUseSkill(0,    1000, 5000));            // never used
    EXPECT(!CanUseSkill(1000, 2000, 5000));           // 1000 elapsed < 5000
    EXPECT(CanUseSkill(1000, 6000, 5000));            // elapsed

    // --- SkillCooldownTracker -------------------------------------------
    {
        SkillCooldownTracker t;
        EXPECT(t.TryUse(42, 7, 1000, 5000));          // first use → OK (records 1000)
        EXPECT(!t.TryUse(42, 7, 2000, 5000));         // 1000 later → still cooling
        EXPECT(t.RemainMs(42, 7, 2000, 5000) == 4000);
        EXPECT(t.TryUse(42, 7, 6000, 5000));          // 5000 later → OK (records 6000)
        EXPECT(!t.TryUse(42, 7, 7000, 5000));         // cooling again

        // Independent per skill and per char.
        EXPECT(t.TryUse(42, 8, 2000, 5000));          // skill 8 ≠ skill 7
        EXPECT(t.TryUse(99, 7, 2000, 5000));          // char 99 ≠ char 42

        // Forget drops a char's stamps; others untouched.
        t.Forget(42);
        EXPECT(t.TryUse(42, 7, 6500, 5000));          // forgotten → ready
        EXPECT(!t.TryUse(99, 7, 2500, 5000));         // char 99 still cooling
    }

    if (g_fails == 0)
        std::printf("test_skill_cooldown: remain/can-use + tracker gate OK\n");
    return g_fails == 0 ? 0 : 1;
}
