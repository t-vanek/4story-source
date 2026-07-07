#pragma once

// bow_constants — wire enums for the Bow battleground subsystem,
// mirroring Lib/Own/TProtocol/include/NetCode.h. Same single-source-
// of-truth rationale as the guild_/party_/friend_constants files.

#include <cstdint>

namespace tworldsvr::bow {

// BOWREG_RESULT (NetCode.h:891) — the result byte of
// MW_ADDTOBOWQUEUE_ACK / MW_CANCELBOWQUEUE_ACK.
inline constexpr std::uint8_t kSuccess        = 0; // BOWREG_SUCCESS
inline constexpr std::uint8_t kCountry        = 1; // BOWREG_COUNTRY
inline constexpr std::uint8_t kAlreadyInQueue = 2; // BOWREG_ALREADYINQUEUE
inline constexpr std::uint8_t kFail           = 3; // BOWREG_FAIL

// BOW_SERVER_ID / BOW_MAP_ID (NetCode.h:143-144).
inline constexpr std::uint8_t  kBowServerId = 30;
inline constexpr std::uint16_t kBowMapId    = 3000;

// Subset of TCONTRY_TYPE (NetCode.h:1091) the legacy AddPlayerToQueue
// gate checks. Eligible Bow countries are D (0) and C (1); anything
// > TCONTRY_C is rejected with kCountry (the gate uses aid_country
// when the primary country is past TCONTRY_C — covers B/N players
// whose aid_country may still be D/C).
inline constexpr std::uint8_t kCountryD       = 0; // TCONTRY_D — Derion
inline constexpr std::uint8_t kCountryC       = 1; // TCONTRY_C — Valorian
inline constexpr std::uint8_t kCountryNeutral = 3; // TCONTRY_N

// W6-54 — the scheduler surface.

// BATTLE_STATUS values the Bow window walks (NetCode.h:2030).
inline constexpr std::uint8_t kBsNormal   = 0;  // BS_NORMAL
inline constexpr std::uint8_t kBsBattle   = 1;  // BS_BATTLE
inline constexpr std::uint8_t kBsAlarm    = 2;  // BS_ALARM
inline constexpr std::uint8_t kBsPeace    = 4;  // BS_PEACE
inline constexpr std::uint8_t kBsBodPeace = 10; // BS_BODPEACE

// TBOW_COMMANDS (NetCode.h:2714) — MW_BOWCOMMANDEXEC_REQ payload.
inline constexpr std::uint8_t kCmdStart = 0;    // BOW_START
inline constexpr std::uint8_t kCmdEnd   = 1;    // BOW_END

// BOW_WINNER (NetCode.h:2044).
inline constexpr std::uint8_t kWinnerDefugel = 0;
inline constexpr std::uint8_t kWinnerCraxion = 1;
inline constexpr std::uint8_t kWinnerTie     = 2;

// BOWMATCH_RESULT (NetCode.h:885).
inline constexpr std::uint8_t kMatchSuccess = 0;
inline constexpr std::uint8_t kMatchFail    = 1;

// BowSystem.cpp:12-13.
inline constexpr std::uint8_t kTeamCount = 2;   // BOW_TEAM_COUNT
inline constexpr std::uint8_t kMaxPoints = 10;  // BOW_MAX_POINTS

// The BODPEACE end countdown (BowSystem.cpp:441-442): 12 seconds
// from the peace tick, then EndBattle.
inline constexpr std::uint32_t kBodPeaceMs = 12000;

// The undocumented AddPlayerToQueue "joined the running match late"
// result (BowSystem.cpp:124 `return BOWREG_FAIL + 1`).
inline constexpr std::uint8_t kLateJoin = 4;

} // namespace tworldsvr::bow
