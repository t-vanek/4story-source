#pragma once

// Stat engine — the pure max-HP / max-MP derivation (no I/O, no
// services beyond the IStatChart it reads). Faithful to the legacy
// TObjBase.cpp formula:
//
//   stat_pure(type) = (BASE_STAT + race[type] + class[type])
//                       * pow(rateX_1ST, level - 1)            // GetBaseStatValue, :1468
//   MaxHP = init_HP + (CON_pure * rateX_HP)                    // GetPureMaxHP, :1991
//   MaxMP = init_MP + (MEN_pure * rateX_MP)                    // GetPureMaxMP, :2001
//
// This is the "pure" tier only — equipment magic options (MTYPE_MHP/
// MMP) and buff/skill bonuses (the GetMaxHP/GetMaxMP additions) are
// later waves and need the item-template + maintain-skill layers.
//
// The per-level growth base is TFORMULACHART[FTYPE_1ST].fRateX (≈1.03
// in the live DB); HP uses CON, MP uses MEN, both via
// TFORMULACHART[FTYPE_HP/MP].

#include "domain/stat.h"
#include "stat_chart.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tmapsvr::stat_engine {

// The level-growth multiplier pow(rateX_1ST, level-1). Level is clamped
// to >= 1 so level 0/1 both yield exponent 0 → multiplier 1.0 (matches
// the legacy `m_bLevel-1` with bLevel>=1).
inline double GrowthMul(const IStatChart& chart, std::uint8_t level)
{
    const double r1 = chart.Formula(FTYPE_1ST).fRateX;
    if (r1 <= 0.0) return 1.0;                 // no/:absent rate → flat
    const int exp = (level > 1) ? (level - 1) : 0;
    return std::pow(r1, exp);
}

// One base stat, pure (class + race + BASE_STAT floor), grown by level.
// `pick` selects the CON / MEN / … field out of a StatBlock. Faithful
// to GetBaseStatValue: max(BASE_STAT, base) is NOT applied per-field in
// the HP/MP path (the legacy floor uses wBaseMIN=0 there); we keep the
// BASE_STAT(+1) additive floor that GetBaseStatValue starts from.
template <typename Pick>
inline double PureStat(const IStatChart& chart, std::uint8_t class_id,
                       std::uint8_t race_id, std::uint8_t level, Pick pick)
{
    const StatBlock cls = chart.ClassStats(class_id);
    const StatBlock rce = chart.RaceStats(race_id);
    const double base = static_cast<double>(kBaseStat) +
                        static_cast<double>(pick(rce)) +
                        static_cast<double>(pick(cls));
    return base * GrowthMul(chart, level);
}

// MaxHP = init_HP + CON_pure * rateX_HP  (floored at 1 so a data hole
// never yields a 0-HP — unrevivable — character).
inline std::uint32_t MaxHP(const IStatChart& chart, std::uint8_t class_id,
                           std::uint8_t race_id, std::uint8_t level)
{
    const FormulaRow f = chart.Formula(FTYPE_HP);
    const double con = PureStat(chart, class_id, race_id, level,
                               [](const StatBlock& s) { return s.wCON; });
    const double v = static_cast<double>(f.dwInit) + con * f.fRateX;
    const long r = std::lround(v);
    return static_cast<std::uint32_t>(std::max<long>(1, r));
}

// MaxMP = init_MP + MEN_pure * rateX_MP  (floored at 1).
inline std::uint32_t MaxMP(const IStatChart& chart, std::uint8_t class_id,
                           std::uint8_t race_id, std::uint8_t level)
{
    const FormulaRow f = chart.Formula(FTYPE_MP);
    const double men = PureStat(chart, class_id, race_id, level,
                               [](const StatBlock& s) { return s.wMEN; });
    const double v = static_cast<double>(f.dwInit) + men * f.fRateX;
    const long r = std::lround(v);
    return static_cast<std::uint32_t>(std::max<long>(1, r));
}

} // namespace tmapsvr::stat_engine
