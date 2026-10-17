#pragma once
#include "domain/maintained_effect.h"
#include <cstdint>
#include <span>

namespace tmapsvr {
struct NativeMonsterCatalog;
struct NativeMonsterSlot;
bool NativeMonsterAttackAllowed(const NativeMonsterCatalog&,const NativeMonsterSlot&);
std::uint8_t NativeHaveDisguiseBuff(const NativeMonsterCatalog&,std::span<const MaintainedEffect>);
std::uint8_t NativeDisguisedCountry(const NativeMonsterCatalog&,std::uint8_t raw_country,
                                  std::span<const MaintainedEffect>);
} // namespace tmapsvr
