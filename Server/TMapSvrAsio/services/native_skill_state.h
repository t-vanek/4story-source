#pragma once
#include "domain/maintained_effect.h"
#include "domain/skill_data.h"
#include <cstdint>
#include <span>

namespace tmapsvr {
struct NativeMonsterCatalog;
struct NativeMonsterSlot;
struct CharacterPayload;
bool NativePlayerSkillAvailable(const CharacterPayload&,std::uint16_t);
const std::vector<SkillDataRow>& NativePlayerSkillData(const CharacterPayload&,std::uint16_t);
// Load/receive only. An unobserved catalog is an error, an absent source
// template is the original ignore branch, a present-empty query stays present.
bool FilterNativePlayerSkills(CharacterPayload&);
const std::vector<SkillDataRow>& NativeMonsterSkillData(const NativeMonsterCatalog&,std::uint16_t);
bool NativeMonsterAttackAllowed(const NativeMonsterCatalog&,const NativeMonsterSlot&);
std::uint8_t NativeHaveDisguiseBuff(const NativeMonsterCatalog&,std::span<const MaintainedEffect>);
std::uint8_t NativeDisguisedCountry(const NativeMonsterCatalog&,std::uint8_t raw_country,
                                  std::span<const MaintainedEffect>);
} // namespace tmapsvr
