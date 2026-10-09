#pragma once

// TObjBase::Calc2ndAbility/GetAtkSpeed/GetAtkSpeedRate and CTSkill::GetReuseDelay.
// Pure arithmetic; catalog hydration supplies actual formula, equipment and
// passive values. Dynamic buff/disarm/disguise timing is a separate contract.
#include "domain/skill.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace tmapsvr::skill_timing {
inline std::uint32_t BaseAttackDelay(std::uint32_t initial,float x,float y,
                                     std::uint16_t race_stat,std::uint16_t class_stat) {
    if(!std::isfinite(x)||!std::isfinite(y))throw std::domain_error("Invalid attack speed formula");
    const float product=static_cast<float>(1U+race_stat+class_stat)*x;
    const float value=std::min(static_cast<float>(initial),std::max(y-product,0.0f));
    if(!std::isfinite(value)||static_cast<double>(value)>std::numeric_limits<std::uint32_t>::max())
        throw std::domain_error("Attack speed outside original DWORD range");
    return static_cast<std::uint32_t>(value);
}
inline SkillAttackTiming AttackTiming(std::uint32_t base,std::int64_t weapon_delta,
                                      std::int32_t passive_delta,std::uint32_t item_reduction) {
    const auto delay=static_cast<std::int64_t>(base)+weapon_delta;
    const auto rate=100LL+passive_delta;
    if(delay<std::numeric_limits<std::int32_t>::min()||delay>std::numeric_limits<std::int32_t>::max()||
       rate>std::numeric_limits<std::int32_t>::max())throw std::domain_error("Attack speed outside original INT range");
    SkillAttackTiming result{static_cast<std::uint32_t>(std::max<std::int64_t>(0,delay)),
                             static_cast<std::uint32_t>(std::max(0LL,rate))};
    if(item_reduction)result.rate=static_cast<std::uint32_t>(std::uint64_t(result.rate)*(100-std::min(item_reduction,100U)))/100;
    return result;
}
inline std::uint32_t ReuseDelay(const SkillTemplate& t,std::uint8_t rank,SkillAttackTiming timing) {
    const auto increment=(static_cast<std::int64_t>(rank)-1)*t.nReuseDelayInc;
    if(increment<std::numeric_limits<std::int32_t>::min()||increment>std::numeric_limits<std::int32_t>::max())
        throw std::domain_error("Skill reuse increment outside original INT range");
    // Original DWORD additions and multiplication wrap before integer /100.
    const std::uint32_t delay=t.dwReuseDelay+static_cast<std::uint32_t>(increment)+timing.delay;
    return static_cast<std::uint32_t>(std::uint64_t(delay)*timing.rate)/100;
}
} // namespace tmapsvr::skill_timing
