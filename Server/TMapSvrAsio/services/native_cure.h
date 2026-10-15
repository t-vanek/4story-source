#pragma once
#include "services/skill_effect.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace tmapsvr::native_cure {
// Positive SVI_INCREASE cure magnitude only. Keep the generic skill engine's
// other combine modes unchanged. Call only when PerformSkill actually runs;
// MISS and empty FINISH must not evaluate exponential magnitude/pow.
inline std::int32_t Magnitude(const SkillDataRow& row,const SkillTemplate& skill,std::uint8_t rank) {
    std::int32_t value;
    if(row.bCalc==2) {
        if(!std::isfinite(skill.f1stRateX))throw std::runtime_error("Cure growth coefficient is not finite");
        const int exponent=rank?skill.bStartLevel+(rank-1)*skill.bNextLevel:0;
        const double power=std::pow(static_cast<double>(skill.f1stRateX),exponent);
        const double product=static_cast<double>(row.wValue)*power;
        const double scaled=product/100.0;
        if(!std::isfinite(power)||!std::isfinite(product)||!std::isfinite(scaled)||
           scaled<std::numeric_limits<std::int32_t>::min()||scaled>std::numeric_limits<std::int32_t>::max())
            throw std::runtime_error("Cure growth result outside source INT domain");
        value=static_cast<std::int32_t>(scaled); // source truncation, including negative fractions to0
    }else value=skill_effect::GetValue(row,skill,rank);
    // Source variance multiplies signed INT nValue by rand()%16. Require all
    // possible rolls to be representable before drawing or adding resources.
    if(value<0||value>std::numeric_limits<std::int32_t>::max()/15)
        throw std::runtime_error("Cure magnitude outside supported positive source variance domain");
    return value;
}
}
