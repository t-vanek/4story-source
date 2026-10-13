#pragma once
#include "domain/main_transfer.h"
#include "skill_cooldown.h"
namespace tmapsvr::transfer {
// Effect sampling is explicit: gameplay must first run its expiry semantics.
CharSnapshot PersistenceSnapshot(const CharSnapshot&,std::uint32_t key,const SkillCooldownTracker&,std::uint64_t now_ms,bool sample_effects=false);
// Capture a frozen native character. The caller must stop gameplay mutations
// before invoking this function and retain that snapshot until prepare resolves.
State Capture(const CharSnapshot&,std::uint32_t key,const SkillCooldownTracker&,std::uint64_t now_ms);
}
