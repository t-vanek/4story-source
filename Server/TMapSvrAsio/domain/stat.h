#pragma once

// Character stat data — the read-only chart side of the stat system,
// loaded once at boot from TFORMULACHART / TCLASSCHART / TRACECHART.
// This slice models ONLY the max-HP / max-MP derivation (Wave: stat
// layer, max-HP/MP). The derived combat stats (AP/WAP/DP/…) and
// equipment / buff contributions are later waves.
//
// Faithful to the legacy formula (TObjBase.cpp GetPureMaxHP/MP +
// GetBaseStatValue) and the FORMULA_TYPE enum in
// Lib/Own/TProtocol/include/NetCode.h (FTYPE_*, 0-based, no explicit
// values). TFORMULACHART.bID == the FTYPE_ ordinal, so the chart is a
// direct bID→formula lookup (legacy TMapSvr.cpp:2664 keys its map on
// the raw bID).

#include <cstdint>

namespace tmapsvr {

// FORMULA_TYPE — Lib/Own/TProtocol/include/NetCode.h:1302. Only the
// members this slice needs are named; the gaps are real ordinals kept
// so FTYPE_HP/MP/1ST land on their true values (8 / 19 / 34). The
// loader keys TFORMULACHART rows by these.
enum FormulaType : std::uint8_t
{
    FTYPE_NONE = 0,
    FTYPE_HP   = 8,    // MaxHP = init + CON_pure * rateX
    FTYPE_MP   = 19,   // MaxMP = init + MEN_pure * rateX
    FTYPE_1ST  = 34,   // per-level stat growth base: pow(rateX, level-1)
    FTYPE_COUNT = 35,
};

// One TFORMULACHART row (the subset this slice reads): the init term +
// the primary rate. rateY is loaded for completeness (some formulas use
// it as an exponent base) but max-HP/MP don't.
struct FormulaRow
{
    std::uint32_t dwInit = 0;
    float         fRateX = 0.f;
    float         fRateY = 0.f;
};

// Per-class or per-race base stat block — TCLASSCHART / TRACECHART
// (bID, wSTR/wDEX/wCON/wINT/wWIS/wMEN). Only CON + MEN feed max-HP/MP;
// the rest are carried for the combat-stat wave.
struct StatBlock
{
    std::uint16_t wSTR = 0;
    std::uint16_t wDEX = 0;
    std::uint16_t wCON = 0;
    std::uint16_t wINT = 0;
    std::uint16_t wWIS = 0;
    std::uint16_t wMEN = 0;
};

// Legacy BASE_STAT constant (TMapType.h:14) — the +1 floor every base
// stat starts from before class/race are added.
inline constexpr std::uint16_t kBaseStat = 1;

} // namespace tmapsvr
