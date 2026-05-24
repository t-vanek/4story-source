#pragma once

// soulmate_constants — SOULMATE_RESULT (NetCode.h:728) + the
// matchmaking level window (TWorldType.h:150).

#include <cstdint>

namespace tworldsvr::soulmate {

inline constexpr std::uint8_t kSuccess      = 0; // SOULMATE_SUCCESS
inline constexpr std::uint8_t kFail         = 1; // SOULMATE_FAIL
inline constexpr std::uint8_t kSilence      = 2;
inline constexpr std::uint8_t kNotFound     = 3; // SOULMATE_NOTFOUND
inline constexpr std::uint8_t kNeedMoney    = 4;
inline constexpr std::uint8_t kAlready      = 5;
inline constexpr std::uint8_t kNpcCallError = 6;
inline constexpr std::uint8_t kInvalidPos   = 7;

// SOULMATE_LEVEL — candidates must be within this many levels.
inline constexpr std::uint8_t kLevelWindow = 10;

} // namespace tworldsvr::soulmate
