#pragma once
#include "domain/skill.h"
#include "domain/skill_data.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace tmapsvr::skill_targets {
// CTSkill::CalcAbilityValue(SA_ONCE,1,MTYPE_EFC) sums signed INT deltas,
// then GetCountMultiAttack narrows the result to BYTE. IsMultiAttack itself
// considers every action. Arithmetic outside the source INT domain is refused.
inline std::optional<SkillMultiAttack> Derive(std::span<const SkillDataRow> rows,
    const SkillTemplate& t,std::uint8_t rank,std::uint8_t target_hit) {
    bool multi=false;std::int64_t total=0;
    for(const auto& row:rows)if(row.bType==SDT_ABILITY&&row.bExec==36) {
        multi=true;if(row.bAction!=SA_ONCE)continue;
        std::int64_t value=0;
        switch(row.bCalc) {
        case 0:value=row.wValue;break;
        case 1:value=row.wValue+(int(rank)-1)*row.wValueInc;break;
        case 2:{
            const int exponent=rank?int(t.bStartLevel)+(int(rank)-1)*t.bNextLevel:0;
            const double scaled=row.wValue*std::pow(static_cast<double>(t.f1stRateX),exponent)/100;
            if(!std::isfinite(t.f1stRateX)||t.f1stRateX<0||!std::isfinite(scaled)||
               scaled<std::numeric_limits<std::int32_t>::min()||scaled>std::numeric_limits<std::int32_t>::max())
                throw std::runtime_error("Multi-attack value outside source INT range");
            value=static_cast<std::int32_t>(scaled);break;
        }
        case 3:value=int(row.wValue)-(int(rank)-1)*int(row.wValueInc);break;
        default:break; // original GetValue returns zero for unknown modes
        }
        std::int64_t delta=0;
        switch(row.bInc) {
        case SVI_INCREASE:delta=value;break;
        case SVI_DECREASE:delta=-value;break;
        case SVI_MULTIPLY:delta=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value)-1U);break;
        case SVI_DIVIDE:delta=value<=1?0:-1;break;
        case SVI_PRECENT:
            // Source base is DWORD 1: multiplication converts a negative INT
            // to unsigned before floating division, then subtracts DWORD 1.
            delta=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<double>(static_cast<std::uint32_t>(value))/100.0)-1U);break;
        default:break;
        }
        total+=delta;
        if(total<std::numeric_limits<std::int32_t>::min()||total>std::numeric_limits<std::int32_t>::max())
            throw std::runtime_error("Multi-attack sum outside source INT range");
    }
    if(!multi)return {};
    return SkillMultiAttack{static_cast<std::uint8_t>(total),target_hit};
}

// CSHandler ordinary/loop vDEFEND construction. Input is already flagged and
// capped at 16; later entries cannot contribute within the supported budget.
// Valid pinned ranks need at most seven hits. Larger derived budgets are refused
// because the original inner random loop can exceed MAX_TARGET in that domain.
template<class Target,class RandBelow>
std::vector<Target> Expand(const std::vector<Target>& requested,
    const std::optional<SkillMultiAttack>& multi,RandBelow&& random_below) {
    if(!multi)return {requested.begin(),requested.begin()+std::min<std::size_t>(requested.size(),16)};
    if(multi->count>16)throw std::runtime_error("Unsupported multi-attack budget above MAX_TARGET");
    std::vector<Target> targets;auto remaining=multi->count;
    for(const auto& target:requested) {
        if(!remaining||targets.size()>=16)break;
        targets.push_back(target);--remaining;
        const auto bound=std::uint32_t(multi->target_hit)+1;
        const auto roll=random_below(bound);
        if(roll>=bound)throw std::runtime_error("Invalid multi-attack random draw");
        for(unsigned hit=1;remaining&&hit<roll;++hit){targets.push_back(target);--remaining;}
    }
    while(remaining&&!targets.empty()&&targets.size()<16){targets.push_back(targets.front());--remaining;}
    return targets;
}
}
