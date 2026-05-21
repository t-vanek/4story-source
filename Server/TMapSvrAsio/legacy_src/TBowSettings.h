#pragma once

#include <cstdint>

#if defined(BOW_COMPILE_MODE) || defined(BR_COMPILE_MODE)

namespace tmapsvr::bow {

inline constexpr std::uint8_t  InventoryId = 16;

inline constexpr std::uint8_t  BpPer10Sec  = 10;

inline constexpr std::uint16_t DefugelStatue  = 32454;
inline constexpr std::uint16_t CraxionStatue  = 32455;
inline constexpr std::uint16_t GuardSpawnId   = 32456;

inline constexpr std::uint16_t SwitchId = 476;

inline constexpr std::uint8_t  ItemMinLevel = 19;

inline constexpr std::uint8_t  KillerSpReward = 5;
inline constexpr std::uint8_t  PartySpReward  = 2;
inline constexpr std::uint8_t  StatueSpReward = 8;
inline constexpr std::uint16_t KillerBpReward = KillerSpReward * 20;
inline constexpr std::uint16_t PartyBpReward  = PartySpReward  * 20;
inline constexpr std::uint16_t StatueBpReward = StatueSpReward * 20;

inline constexpr std::uint8_t  RespawnMedalBase  = 20;
inline constexpr std::uint8_t  RespawnMedalAdder = 5;

inline constexpr std::uint8_t  MedalBpRate = 100;

inline constexpr std::uint8_t  DefugelEquipEffect = 9;
inline constexpr std::uint8_t  CraxionEquipEffect = 7;

inline constexpr std::uint8_t  MinSpToGetReward   = 20;
inline constexpr std::uint16_t RewardItemId       = 7983;
inline constexpr std::uint8_t  RewardMaxLogoutMin = 3;

inline constexpr std::uint16_t FirstPlaceTitleId = 150;

inline constexpr std::uint16_t FailsafeSpawnId = 15003;
inline constexpr std::uint16_t BpBase          = 500;

} // namespace tmapsvr::bow

#if defined(BR_COMPILE_MODE)
#include "BRSettings.h"
#endif

#endif // BOW_COMPILE_MODE || BR_COMPILE_MODE
