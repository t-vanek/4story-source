#pragma once

// Money — the three-tier gold / silver / cooper purse arithmetic and the
// monster money-drop roll, faithful to the legacy.
//
//   * tier ratios: legacy NetCode.h MONEY_MULTIPLY = 1000, so
//     1 silver = 1000 cooper and 1 gold = 1000 silver = 1 000 000 cooper.
//     CalcMoney / SplitMoney mirror NetCode.h:157-167 (the same shape the
//     TWorldSvrAsio guild port already uses).
//   * the drop roll mirrors TMonster.cpp:1085 — a bMoneyProb/100 chance to
//     drop a value in [dwMinMoney, dwMaxMoney). The amount is in cooper
//     (the base unit monster money + the wire purse all use).

#include "domain/character.h"

#include <cstdint>
#include <functional>

namespace tmapsvr {

inline constexpr std::int64_t kCooperPerSilver = 1000;                 // MONEY_MULTIPLY
inline constexpr std::int64_t kCooperPerGold   = kCooperPerSilver * 1000;

// Combine a purse into a single cooper total (legacy CalcMoney combine).
inline std::int64_t CalcMoney(std::uint32_t gold, std::uint32_t silver,
                              std::uint32_t cooper)
{
    return static_cast<std::int64_t>(gold)   * kCooperPerGold
         + static_cast<std::int64_t>(silver) * kCooperPerSilver
         + static_cast<std::int64_t>(cooper);
}

// Split a cooper total back into tiers (legacy CalcMoney split).
inline void SplitMoney(std::int64_t total, std::uint32_t& gold,
                       std::uint32_t& silver, std::uint32_t& cooper)
{
    if (total < 0) total = 0;
    gold   = static_cast<std::uint32_t>(total / kCooperPerGold);
    total %= kCooperPerGold;
    silver = static_cast<std::uint32_t>(total / kCooperPerSilver);
    cooper = static_cast<std::uint32_t>(total % kCooperPerSilver);
}

// Add `cooper` to a char's purse, carrying across tiers. Mirrors the legacy
// "total = CalcMoney(purse) + amount; CalcMoney(total -> purse)" pattern.
inline void AddMoneyToChar(CharSnapshot& s, std::int64_t cooper)
{
    const std::int64_t total =
        CalcMoney(s.dwGold, s.dwSilver, s.dwCooper) + cooper;
    SplitMoney(total, s.dwGold, s.dwSilver, s.dwCooper);
}

// Monster money drop (legacy TMonster.cpp:1085):
//   if (bMoneyProb > rand()%100 && dwMaxMoney > dwMinMoney)
//       money = dwMinMoney + rand()%(dwMaxMoney - dwMinMoney);
// Returns cooper (0 = nothing dropped). rand_below(n) → [0, n).
inline std::uint32_t RollMoneyDrop(
    std::uint8_t money_prob, std::uint32_t min_money, std::uint32_t max_money,
    const std::function<std::uint32_t(std::uint32_t)>& rand_below)
{
    if (money_prob > rand_below(100) && max_money > min_money)
        return min_money + rand_below(max_money - min_money);
    return 0;
}

} // namespace tmapsvr
