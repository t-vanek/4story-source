#pragma once
#include "native_skill_state.h"
#include <cstdint>
#include <optional>

namespace tmapsvr {
// CTSkillTemp::GetItemProbBuff: first matching raw WORD in the original
// m_vData order. A matching zero is distinct from no matching status row.
inline std::optional<std::uint16_t> NativeItemBuffValue(const CharacterPayload& p,std::uint16_t skill,int status) {
    for(const auto& d:NativePlayerSkillData(p,skill))
        if(d.bType==SDT_STATUS&&d.bExec==status)return d.wValue;
    return {};
}
}
