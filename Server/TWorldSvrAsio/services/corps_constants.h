#pragma once

// corps_constants — wire enums for the corps subsystem, mirroring
// Lib/Own/TProtocol/include/NetCode.h. Same single-source-of-truth
// rationale as guild_constants.h / party_constants.h.

#include <cstdint>

namespace tworldsvr::corps {

// CORPS_RESULT (NetCode.h:604) — the result byte of
// MW_CORPSREPLY_REQ (+ MW_PARTYMOVE_REQ, MW_CHGCORPSCOMMANDER_REQ).
inline constexpr std::uint8_t kSuccess      = 0;  // CORPS_SUCCESS
inline constexpr std::uint8_t kDeny         = 1;  // CORPS_DENY
inline constexpr std::uint8_t kBusy         = 2;  // CORPS_BUSY (arena)
inline constexpr std::uint8_t kNoParty      = 3;  // CORPS_NO_PARTY
inline constexpr std::uint8_t kNotCommander = 4;  // CORPS_NOT_COMMANDER
inline constexpr std::uint8_t kWrongTarget  = 5;  // CORPS_WRONG_TARGET
inline constexpr std::uint8_t kTargetNoParty= 6;  // CORPS_TARGET_NO_PARTY
inline constexpr std::uint8_t kAlready      = 7;  // CORPS_ALREADY
inline constexpr std::uint8_t kChgCommander = 8;  // CORPS_CHGCOMMANDER
inline constexpr std::uint8_t kDead         = 9;  // CORPS_DEAD
inline constexpr std::uint8_t kMaxParty     = 10; // CORPS_MAX_PARTY

// MAX_CORPS_PARTY (TWorldType.h:151) — the squad cap per corps.
inline constexpr std::uint8_t kMaxCorpsParty = 7;

} // namespace tworldsvr::corps
