// Skill resource-cost engine tests — verifies skill_engine.h, faithful to
// CTSkill::GetRequiredMP / GetRequiredHP (Server/TMapSvr/TSkill.cpp:202).
//
//   type 0 -> free
//   type 2 -> maxResource * dwUse / 100   (exact, %-of-max)
//   type 1 -> deferred to 0 (level-scale base not in TSKILLCHART + rank
//             untracked; charging the unscaled base would over-bill)

#include "services/skill_engine.h"
#include "services/skill_chart.h"   // SkillTemplate (via domain/skill.h)

#include <cstdint>
#include <iostream>

using namespace tmapsvr;

namespace {

SkillTemplate MakeSkill(std::uint8_t mp_type, std::uint32_t use_mp,
                        std::uint8_t hp_type = 0, std::uint32_t use_hp = 0)
{
    SkillTemplate t{};
    t.wID        = 1;
    t.bUseMPType = mp_type;
    t.dwUseMP    = use_mp;
    t.bUseHPType = hp_type;
    t.dwUseHP    = use_hp;
    return t;
}

int g_fail = 0;
void Check(bool ok, const char* what)
{
    if (!ok) { std::cerr << "FAIL: " << what << "\n"; ++g_fail; }
}

} // namespace

int main()
{
    // --- type 0: no cost ------------------------------------------------
    {
        const auto t = MakeSkill(0, 1234);
        Check(skill_engine::RequiredMP(t, 1000) == 0, "type0 MP is free");
        Check(skill_engine::RequiredHP(t, 1000) == 0, "type0 HP is free");
    }

    // --- type 2: %-of-max, exact (maxMP * dwUseMP / 100) ----------------
    {
        const auto t10 = MakeSkill(2, 10);                 // 10 %
        Check(skill_engine::RequiredMP(t10, 600) == 60, "type2 10% of 600 = 60");
        Check(skill_engine::RequiredMP(t10, 0)   == 0,  "type2 of 0 max = 0");

        const auto t5 = MakeSkill(2, 5);
        Check(skill_engine::RequiredMP(t5, 1000) == 50, "type2 5% of 1000 = 50");

        // integer truncation: 7% of 610 = 42.7 -> 42
        const auto t7 = MakeSkill(2, 7);
        Check(skill_engine::RequiredMP(t7, 610) == 42, "type2 7% of 610 = 42 (trunc)");
    }

    // --- type 2 over the HP columns (bUseHPType / dwUseHP) --------------
    {
        const auto t = MakeSkill(/*mp*/0, 0, /*hp_type*/2, /*use_hp*/20);  // 20 %
        Check(skill_engine::RequiredHP(t, 500) == 100, "type2 HP 20% of 500 = 100");
        Check(skill_engine::RequiredMP(t, 500) == 0,   "no MP cost when MP type 0");
    }

    // --- type 1: deferred to 0 (must NOT charge the unscaled base) ------
    {
        const auto t = MakeSkill(1, 2451);                 // a real type-1 base
        Check(skill_engine::RequiredMP(t, 600) == 0,
              "type1 MP deferred -> 0 (not over-charged)");

        const auto th = MakeSkill(0, 0, 1, 999);
        Check(skill_engine::RequiredHP(th, 600) == 0, "type1 HP deferred -> 0");
    }

    if (g_fail == 0)
        std::cout << "test_skill_engine: all checks passed\n";
    return g_fail == 0 ? 0 : 1;
}
