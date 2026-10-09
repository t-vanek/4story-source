#include "services/skill_engine.h"
#include "services/skill_targets.h"
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

    {
        SkillTemplate t;SkillDataRow effect{SA_ONCE,SDT_ABILITY,2,36,SVI_INCREASE,3,1,1};
        std::vector<SkillDataRow> data{effect};
        for(std::uint8_t rank=1;rank<=5;++rank)
            Check(skill_targets::Derive(data,t,rank,3)->count==rank+2,"source skill324/412 rank scaling");
        auto multi=skill_targets::Derive(data,t,2,3);
        Check(multi&&multi->count==4&&multi->target_hit==3,"multi projection retains budget and random per-target limit");
        std::vector<int> request{11,22};std::vector<unsigned> draws{3,0};std::size_t n=0;
        const auto distributed=skill_targets::Expand(request,multi,[&](unsigned bound){Check(bound==4,"original random modulus includes target_hit endpoint");return draws[n++];});
        Check(distributed==std::vector<int>({11,11,11,22}),"golden random expansion exhausts budget in original target order");
        Check(skill_targets::Expand(request,multi,[](unsigned){return 0;})==std::vector<int>({11,22,11,11}),"unused hits repeat the first flagged defender");
        Check(skill_targets::Expand(std::vector<int>{42},multi,[](unsigned){return 3;})==std::vector<int>({42,42,42,42}),"one flagged target always receives the whole multi-attack budget");
        Check(skill_targets::Expand(std::vector<int>{},multi,[](unsigned){throw std::runtime_error("unexpected RNG");return 0;}).empty(),"unflagged-only request creates no defenders and draws no random numbers");
        for(unsigned budget=1;budget<=16;++budget)for(unsigned roll=0;roll<=3;++roll) {
            auto result=skill_targets::Expand(std::vector<int>(20,17),SkillMultiAttack{static_cast<std::uint8_t>(budget),3},[&](unsigned){return roll;});
            Check(result.size()==budget,"bounded source expansion neither loses nor invents hits");
        }
        Check(skill_targets::Expand(std::vector<int>(20,17),{},[](unsigned){throw std::runtime_error("unexpected RNG");return 0;}).size()==16,"ordinary target cap is unchanged and does not draw random numbers");
        data[0].bAction=SA_BUFF;auto zero=skill_targets::Derive(data,t,1,3);
        Check(zero&&zero->count==0&&skill_targets::Expand(request,zero,[](unsigned){return 0;}).empty(),"IsMultiAttack sees non-once data while hit calculation excludes it");
        data[0]=effect;data.push_back(effect);data.push_back({SA_PASSIVE,SDT_ABILITY,1,36,SVI_INCREASE,99,0,0});
        Check(skill_targets::Derive(data,t,1,3)->count==6,"all once ability rows contribute, passive row does not");
        data={effect};Check(skill_targets::Derive(data,t,255,3)->count==1,"source narrows summed INT hit count to BYTE");
        data[0].wValue=6;Check(skill_targets::Derive(data,t,1,1)->count==6,"backup skill1407 has six hits with per-target cap one");
        data[0].bExec=30;Check(!skill_targets::Derive(data,t,1,3),"ordinary damage data does not enable multi-attack");
        data={effect};data[0].bCalc=2;data[0].wValue=100;t.f1stRateX=2;t.bStartLevel=1;t.bNextLevel=1;
        Check(skill_targets::Derive(data,t,3,3)->count==8,"multi-attack exponential mode follows original double calculation");
        data[0].bInc=SVI_MULTIPLY;Check(skill_targets::Derive(data,t,3,3)->count==7,"multiply returns delta against source base one");
        data[0].bInc=SVI_DIVIDE;Check(skill_targets::Derive(data,t,3,3)->count==255,"divide delta uses source BYTE narrowing");
        data[0].bInc=SVI_PRECENT;data[0].bCalc=0;data[0].wValue=200;
        Check(skill_targets::Derive(data,t,1,3)->count==1,"percent returns delta against base one");
        data[0].bCalc=3;data[0].wValue=5;data[0].wValueInc=2;data[0].bInc=SVI_DECREASE;
        Check(skill_targets::Derive(data,t,2,3)->count==253,"falling negative delta retains source signed-to-BYTE conversion");
        bool refused=false;try{skill_targets::Expand(request,SkillMultiAttack{17,3},[](unsigned){return 3;});}catch(...){refused=true;}
        Check(refused,"unsupported source budgets that may exceed MAX_TARGET fail before mutation");
        refused=false;try{skill_targets::Expand(request,multi,[](unsigned){return 4;});}catch(...){refused=true;}
        Check(refused,"out-of-range RNG injection is refused");
        data[0].bCalc=2;t.f1stRateX=std::numeric_limits<float>::infinity();refused=false;
        try{skill_targets::Derive(data,t,255,3);}catch(...){refused=true;}
        Check(refused,"non-finite multi-attack calculation cannot convert to an integer");
    }
    if (g_fail == 0)
        std::cout << "test_skill_engine: " << checks << " checks passed\n";
    return g_fail == 0 ? 0 : 1;
}
