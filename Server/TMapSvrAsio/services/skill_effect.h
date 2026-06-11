#pragma once

// Skill-effect value engine — pure, header-only (like stat_engine /
// skill_engine). Faithful port of CTSkillTemp::GetValue / Calculate /
// CalcValue (legacy_src/TSkillTemp.cpp:49-105) plus the SDT_CURE
// application roll from TObjBase::PerformSkill (TObjBase.cpp:3555):
//
//     heal = CalcValue(...) ;  applied = heal + heal * (rand()%16) / 100
//
// GetValue calc modes (TSkillTemp.cpp:62):
//     0: wValue                                   (constant)
//     1: wValue + (lvl-1) * wValueInc             (linear)
//     2: wValue * pow(f1stRateX, start+(lvl-1)*next) / 100   (exponential)
//     3: wValue - (lvl-1) * wValueInc             (falling, signed)
//
// Mode 2's coefficients are template-level: f1stRateX is the
// TFORMULACHART[FTYPE_1ST].fRateX coefficient (TMapSvr.cpp:2673 caches it
// while loading the formula chart and stamps every skill template with it,
// :2730) and start/next are TSKILLCHART.bLevel / bNextLevel (:2705/:2707).
// The legacy `(bLevel == NULL) ? NULL : …` quirk means level 0 yields
// exponent 0 → wValue/100; preserved.
//
// Calculate's SVI_ combine modes (TSkillTemp.cpp:77): MULTIPLY / DIVIDE /
// PRECENT return the DELTA vs the base quantity (legacy subtracts dwValue),
// DIVIDE guards a non-positive divisor to 1, PRECENT rounds through double.

#include "domain/skill.h"
#include "domain/skill_data.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace tmapsvr::skill_effect {

// CTSkillTemp::GetValue — the level-scaled magnitude of one data row.
inline int GetValue(const SkillDataRow& d, const SkillTemplate& t,
                    std::uint8_t level) noexcept
{
    switch (d.bCalc)
    {
    case 0:
        return d.wValue;
    case 1:
        return d.wValue + (level - 1) * d.wValueInc;
    case 2:
    {
        const int exponent = level == 0
            ? 0
            : t.bStartLevel + (level - 1) * t.bNextLevel;
        return static_cast<int>(
            (d.wValue * std::pow(t.f1stRateX, exponent)) / 100.0);
    }
    case 3:
        return static_cast<int>(d.wValue)
             - (static_cast<int>(level) - 1) * static_cast<int>(d.wValueInc);
    }
    return 0;
}

// CTSkillTemp::Calculate — combine the row magnitude with the base
// quantity `base` (max-HP for SCT_HP, max-MP for SCT_MP, …).
inline int Calculate(const SkillDataRow& d, const SkillTemplate& t,
                     std::uint8_t level, std::uint32_t base) noexcept
{
    int calc = GetValue(d, t, level);

    switch (d.bInc)
    {
    case SVI_INCREASE: return calc;
    case SVI_DECREASE: return -calc;
    case SVI_MULTIPLY: return static_cast<int>(base) * calc
                            - static_cast<int>(base);
    case SVI_DIVIDE:
        if (calc <= 0) calc = 1;
        return static_cast<int>(base) / calc - static_cast<int>(base);
    case SVI_PRECENT:  return static_cast<int>(base * calc / 100.0)
                            - static_cast<int>(base);
    default:           return 0;
    }
}

// CTSkillTemp::CalcValue — sum every row matching (type, exec).
inline int CalcValue(const std::vector<SkillDataRow>& rows,
                     const SkillTemplate& t, std::uint8_t level,
                     std::uint8_t type, std::uint8_t exec,
                     std::uint32_t base) noexcept
{
    int inc = 0;
    for (const auto& d : rows)
        if (d.bType == type && d.bExec == exec)
            inc += Calculate(d, t, level, base);
    return inc;
}

// TObjBase::PerformSkill SCT_HP/SCT_MP roll — the applied amount carries a
// +0..15 % variance: `nValue + nValue * (rand()%16) / 100`. `rand_below`
// is the caller's RNG (rand_below(16) → [0,16)), keeping this testable.
template <typename RandBelow>
inline int CureRoll(int value, RandBelow&& rand_below)
{
    return value + value * static_cast<int>(rand_below(16u)) / 100;
}

} // namespace tmapsvr::skill_effect
