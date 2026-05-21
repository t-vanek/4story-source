#pragma once

#include <cstdint>

#ifdef BR_COMPILE_MODE

namespace tmapsvr::br {

inline constexpr std::uint8_t  MaxLifes               = 5;
inline constexpr std::uint32_t MaxNotIngameDurationMs = 5 * 60 * 1000;

inline constexpr std::uint8_t  KillerBpReward = 150;
inline constexpr std::uint8_t  PartyBpReward  = 50;

inline constexpr std::uint16_t FirstPlaceTitleId = 160;

inline constexpr std::uint8_t  KillsToLife = 8;

inline constexpr std::uint16_t WinningTeamReward = 10000;

} // namespace tmapsvr::br

#endif // BR_COMPILE_MODE
