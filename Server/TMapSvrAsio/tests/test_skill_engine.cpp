#include "services/skill_engine.h"
#include "services/skill_chart.h"   // SkillTemplate (via domain/skill.h)

#include <cstdint>
#include <iostream>
#include <limits>

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
int checks = 0;
void Check(bool ok, const char* what)
{
    ++checks;
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

    // Literal source FLOAT rounding and learned-rank exponent cases.
    {
        auto t=MakeSkill(1,1000,1,2500);t.f1stRateX=2.0f;t.bStartLevel=1;t.bNextLevel=1;
        Check(skill_engine::RequiredMP(t,600,1)==20,"rank1 flat cost uses start level");
        Check(skill_engine::RequiredMP(t,600,3)==80,"rank3 flat cost includes rank increment");
        Check(skill_engine::RequiredHP(t,600,3)==200,"HP uses the same source rank scaling");
        Check(skill_engine::RequiredMP(t,600,0)==10,"source rank-zero branch uses exponent zero");
        t.dwUseMP=3432;t.f1stRateX=1.03f;t.bStartLevel=16;t.bNextLevel=16;
        Check(skill_engine::RequiredMP(t,600,1)==55,"backup skill134 rank1 golden cost");
        Check(skill_engine::RequiredMP(t,600,2)==88,"backup skill134 rank2 golden cost");
        t=MakeSkill(1,999);t.f1stRateX=1.0f;
        Check(skill_engine::RequiredMP(t,600,255)==9,"flat cost truncates fractional result");
        t=MakeSkill(2,2);
        Check(skill_engine::RequiredMP(t,0xffffffffU,255)==42949672,"ratio multiplication retains source DWORD wrap before division");
        t=MakeSkill(1,1000);t.f1stRateX=2.0f;t.bStartLevel=255;t.bNextLevel=255;
        bool rejected=false;try{(void)skill_engine::RequiredMP(t,600,255);}catch(const std::domain_error&){rejected=true;}
        Check(rejected,"unrepresentable exponential cannot become a free cast");
        t.bStartLevel=0;t.bNextLevel=0;t.f1stRateX=std::numeric_limits<float>::quiet_NaN();
        rejected=false;try{(void)skill_engine::RequiredMP(t,600,1);}catch(const std::domain_error&){rejected=true;}
        Check(rejected,"invalid formula growth is refused");
        t=MakeSkill(3,1);rejected=false;
        try{(void)skill_engine::RequiredMP(t,600,1);}catch(const std::domain_error&){rejected=true;}
        Check(rejected,"unsupported resource selector is refused");
    }

    if (g_fail == 0)
        std::cout << "test_skill_engine: " << checks << " checks passed\n";
    return g_fail == 0 ? 0 : 1;
}
