#pragma once

// battle_constants — BATTLE_TYPE selector for the war-window enable
// broadcast (SM_BATTLESTATUS_REQ).
//
// The BATTLE_TYPE enum is **absent from this source tree**. The
// values here are reconstructed: BT_CASTLE = 1 matches the
// already-committed reconstruction in
// Server/TControlSvrAsio/handlers/handlers_patch.cpp
// (kBattleTypeCastle), and the rest follow the legacy declaration
// order (LOCAL, CASTLE, MISSION, SKYGARDEN) as a 0-based enum.

#include <cstdint>

namespace tworldsvr::battle {

inline constexpr std::uint8_t kTypeLocal     = 0; // BT_LOCAL
inline constexpr std::uint8_t kTypeCastle    = 1; // BT_CASTLE (confirmed)
inline constexpr std::uint8_t kTypeMission   = 2; // BT_MISSION
inline constexpr std::uint8_t kTypeSkyGarden = 3; // BT_SKYGARDEN (deferred)

} // namespace tworldsvr::battle
