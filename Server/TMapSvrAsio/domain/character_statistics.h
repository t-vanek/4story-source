#pragma once
#include "domain/skill.h"
#include <array>
#include <cstdint>

namespace tmapsvr {
// Source CS_CHARSTATINFO_ACK values. Metadata is server-only and prevents a
// cached sheet from being used after level/aftermath changes without rehydration.
struct CharacterStatistics {
    std::uint8_t level{}, aftermath{};
    std::array<std::uint16_t,6> primary{};
    std::uint32_t min_physical{},max_physical{},physical_defense{},min_ranged{},max_ranged{};
    std::array<SkillAttackTiming,3> timing{};
    std::uint16_t attack_level{},defense_level{};
    std::uint8_t physical_critical{};
    std::uint32_t min_magic{},max_magic{},magic_defense{};
    std::uint16_t magic_attack_level{},magic_defense_level{};
    std::uint8_t charge_speed{},charge_probability{},magic_critical{};
};
}
