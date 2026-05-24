#pragma once

// br_constants — wire enums for the Battle Royale subsystem,
// mirroring Lib/Own/TProtocol/include/NetCode.h. Single source of
// truth, same pattern as bow_/guild_/party_constants.

#include <cstdint>

namespace tworldsvr::br {

// TEAMADD_RESULT (NetCode.h:189) — bResult byte of
// MW_BRTEAMMATEADD_ACK + MW_BRTEAMMATEADDRESULT_ACK.
inline constexpr std::uint8_t kTeamAddSuccess       = 0; // TEAMADD_SUCCESS
inline constexpr std::uint8_t kTeamAddNotFound      = 1; // TEAMADD_NOTFOUND
inline constexpr std::uint8_t kTeamAddBusy          = 2; // TEAMADD_BUSY
inline constexpr std::uint8_t kTeamAddRefuse        = 3; // TEAMADD_REFUSE
inline constexpr std::uint8_t kTeamAddAlreadyInTeam = 4; // TEAMADD_ALREADYINTEAM

// BR_SERVER_ID (NetCode.h:146).
inline constexpr std::uint8_t kBrServerId = 50;

// BR battle modes + the team cap derived from the mode.
// BR_3V3 (NetCode.h:2745) — chief + 2 mates = 3 total.
inline constexpr std::uint8_t kModeBr3v3            = 0;
inline constexpr std::uint8_t kTeamMaxCount3v3      = 3;

// BR_RESULT — the legacy BR functions reuse the BOWREG_* enum
// (BowSystem.cpp:225 / BRSystem.cpp:225 both return BOWREG_*).
// We re-export the same numeric values under the BR namespace so
// the handler logs / tests read coherently next to other BR code.
inline constexpr std::uint8_t kSuccess        = 0; // BOWREG_SUCCESS
inline constexpr std::uint8_t kAlreadyInQueue = 2; // BOWREG_ALREADYINQUEUE
inline constexpr std::uint8_t kFail           = 3; // BOWREG_FAIL

} // namespace tworldsvr::br
