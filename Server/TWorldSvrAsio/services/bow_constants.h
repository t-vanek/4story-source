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
inline constexpr std::uint8_t kCountryD = 0;   // TCONTRY_D — Derion
inline constexpr std::uint8_t kCountryC = 1;   // TCONTRY_C — Valorian

} // namespace tworldsvr::bow
