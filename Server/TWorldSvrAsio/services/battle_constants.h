#pragma once

// battle_constants — BATTLE_TYPE selector for the war-window enable
// broadcast (SM_BATTLESTATUS_REQ).
//
// Source Server/TWorldSvr/TWorldType.h::BATTLE_TYPE. Tournament occupies2;
// neither MISSION nor SKYGARDEN may be renumbered to omit that slot.

#include <cstdint>

namespace tworldsvr::battle {

inline constexpr std::uint8_t kTypeLocal     = 0; // BT_LOCAL
inline constexpr std::uint8_t kTypeCastle    = 1; // BT_CASTLE (confirmed)
inline constexpr std::uint8_t kTypeTournament= 2; // BT_TOURNAMENT
inline constexpr std::uint8_t kTypeMission   = 3; // BT_MISSION
inline constexpr std::uint8_t kTypeSkyGarden = 4; // BT_SKYGARDEN (deferred)
inline constexpr std::uint8_t kTypeBow       = 5; // BT_BOW
inline constexpr std::uint8_t kTypeBr        = 6; // BT_BR
inline constexpr std::uint8_t kTypeCount     = 7; // BT_COUNT

} // namespace tworldsvr::battle
