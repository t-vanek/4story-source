#pragma once
#include "services/skill_effect.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace tmapsvr::native_cure {
// GetValue is signed independently of the combine: DECREASE can turn a
// negative value into a positive cure. Keep the generic skill engine intact.
// Call only when PerformSkill runs; MISS/empty must not evaluate magnitude/pow.
inline std::int32_t SignedValue(const SkillDataRow& row,const SkillTemplate& skill,std::uint8_t rank) {
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
    }else if(row.bCalc==0||row.bCalc==1||row.bCalc==3)value=skill_effect::GetValue(row,skill,rank);
    else throw std::runtime_error("Unsupported source cure calculation mode");
    return value;
}
inline std::int32_t Magnitude(const SkillDataRow& row,const SkillTemplate& skill,std::uint8_t rank,std::uint32_t base) {
    const auto value=SignedValue(row,skill,rank);
    // Source converts the signed factor to DWORD for these operations. Bound
    // product/subtraction to nonwrapping values before performing either one.
    const auto product=[&] {
        const auto wide=static_cast<std::uint64_t>(base)*static_cast<std::uint32_t>(value);
        if(wide>std::numeric_limits<std::uint32_t>::max())
            throw std::runtime_error("Cure product outside nonwrapping source DWORD domain");
        return static_cast<std::uint32_t>(wide);
    };
    const auto difference=[&](std::uint32_t projected) {
        if(projected<base)throw std::runtime_error("Cure subtraction requires unsupported wrapped DWORD arithmetic");
        return static_cast<std::int64_t>(projected)-base;
    };
    std::int64_t result;
    switch(row.bInc) {
    case SVI_INCREASE:result=value;break;
    case SVI_DECREASE:
        if(value==std::numeric_limits<std::int32_t>::min())throw std::runtime_error("Cure negation outside source INT domain");
        result=-static_cast<std::int64_t>(value);break;
    case SVI_MULTIPLY:result=difference(product());break;
    case SVI_DIVIDE: {
        const auto divisor=static_cast<std::uint32_t>(value<=0?1:value);
        result=difference(base/divisor);break;
    }
    case SVI_PRECENT: {
        // The DWORD product precedes promotion to double in source Calculate.
        const double scaled=product()/100.0;
        if(!std::isfinite(scaled)||scaled<std::numeric_limits<std::int32_t>::min()||scaled>std::numeric_limits<std::int32_t>::max())
            throw std::runtime_error("Cure percentage outside source INT domain");
        result=difference(static_cast<std::uint32_t>(static_cast<std::int32_t>(scaled)));break;
    }
    default:throw std::runtime_error("Unsupported source cure combine mode");
    }
    // Source variance multiplies the FINAL signed INT delta by rand()%16.
    if(result<0||result>std::numeric_limits<std::int32_t>::max()/15)
        throw std::runtime_error("Cure magnitude outside supported positive source variance domain");
    return static_cast<std::int32_t>(result);
}
}
