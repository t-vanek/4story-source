#pragma once

// CTSkill::GetRequiredMP/HP (TSkill.cpp:185-217). The flat cost scales by
// learned rank using TFORMULACHART[FTYPE_1ST], stamped on each template by
// TMapSvr.cpp. Preserve its FLOAT rounding before the final DWORD truncation.
#include "services/skill_chart.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace tmapsvr::skill_engine {
inline std::uint32_t RequiredResource(const SkillTemplate& t,std::uint8_t type,
                                     std::uint32_t use,std::uint32_t maximum,std::uint8_t rank) {
    switch(type) {
    case 0:return 0;
    case 1:{
        if(!std::isfinite(t.f1stRateX)||t.f1stRateX<0)
            throw std::domain_error("Invalid skill cost growth factor");
        const int exponent=rank?int(t.bStartLevel)+(int(rank)-1)*t.bNextLevel:0;
        const float rate=static_cast<float>(std::pow(static_cast<double>(t.f1stRateX),exponent)/100.0);
        const float cost=static_cast<float>(use)*rate;
        // The original conversion is undefined outside DWORD range. Refuse
        // invalid catalog/rank arithmetic instead of granting a free cast.
        if(!std::isfinite(cost)||cost<0||static_cast<double>(cost)>std::numeric_limits<std::uint32_t>::max())
            throw std::domain_error("Skill cost outside original DWORD range");
        return static_cast<std::uint32_t>(cost);
    }
    case 2:
        // Both operands are original DWORDs: multiplication wraps before /100.
        return static_cast<std::uint32_t>(static_cast<std::uint64_t>(maximum)*use)/100U;
    default:throw std::domain_error("Unsupported skill resource cost type");
    }
}
inline std::uint32_t RequiredMP(const SkillTemplate& t,std::uint32_t max_mp,std::uint8_t rank=1) {
    return RequiredResource(t,t.bUseMPType,t.dwUseMP,max_mp,rank);
}
inline std::uint32_t RequiredHP(const SkillTemplate& t,std::uint32_t max_hp,std::uint8_t rank=1) {
    return RequiredResource(t,t.bUseHPType,t.dwUseHP,max_hp,rank);
}
} // namespace tmapsvr::skill_engine
