#pragma once
#include "services/native_skill_value.h"

namespace tmapsvr::native_cure {
// Shared signed source math; cure alone retains its variance-product bound.
// Call during PerformSkill only: MISS/empty branches must not evaluate pow.
inline std::int32_t SignedValue(const SkillDataRow& row,const SkillTemplate& skill,std::uint8_t rank) {
    return native_skill_value::SignedValue(row,skill,rank);
}
inline std::int32_t Delta(const SkillDataRow& row,const SkillTemplate& skill,std::uint8_t rank,std::uint32_t base) {
    const auto result=native_skill_value::Calculate(row,skill,rank,base);
    // Source variance multiplies the FINAL signed INT delta by rand()%16.
    constexpr std::int64_t limit=std::numeric_limits<std::int32_t>::max()/15;
    if(result<-limit||result>limit)
        throw std::runtime_error("Cure delta outside supported signed source variance domain");
    return static_cast<std::int32_t>(result);
}
}
