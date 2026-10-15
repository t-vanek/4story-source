#pragma once
#include "services/skill_effect.h"
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace tmapsvr::native_cure {
// GetValue is signed independently of the combine: DECREASE can turn a
// negative value into a positive cure. Keep the generic skill engine intact.
// Call only when PerformSkill runs; MISS/empty must not evaluate delta/pow.
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
    else value=0; // source GetValue's default for an actual unknown BYTE mode
    return value;
}
inline std::int32_t Delta(const SkillDataRow& row,const SkillTemplate& skill,std::uint8_t rank,std::uint32_t base) {
    const auto value=SignedValue(row,skill,rank);
    // Source Win32 DWORD product/subtraction are modulo2^32. Project those
    // bits back to the Windows signed INT result explicitly, as skill_targets
    // already does, rather than using implementation-defined unsigned->INT.
    const auto product=[&] {
        const auto wide=static_cast<std::uint64_t>(base)*static_cast<std::uint32_t>(value);
        return static_cast<std::uint32_t>(wide);
    };
    const auto difference=[&](std::uint32_t projected) {
        return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(projected-base));
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
    default:result=0;break; // Calculate calls GetValue first, then keeps nValue0
    }
    // Source variance multiplies the FINAL signed INT delta by rand()%16.
    constexpr std::int64_t limit=std::numeric_limits<std::int32_t>::max()/15;
    if(result<-limit||result>limit)
        throw std::runtime_error("Cure delta outside supported signed source variance domain");
    return static_cast<std::int32_t>(result);
}
}
