#pragma once
#include "skill_targets.h"
#include "native_skill_state.h"

namespace tmapsvr {
// Shared by fresh/transfer hydration and the locked consumption validator.
// Source query order and known-empty results come from the same boot snapshot.
inline std::optional<SkillMultiAttack> ReadNativeMultiAttack(const CharacterPayload& p,
    const SkillTemplate& t,std::uint8_t rank,std::uint8_t target_hit) {
    return skill_targets::Derive(NativePlayerSkillData(p,t.wID),t,rank,target_hit);
}
}
