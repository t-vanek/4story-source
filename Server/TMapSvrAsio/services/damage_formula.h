#pragma once

// Physical-damage formula — a faithful port of the core hit math from
// legacy TObjBase.cpp::CalcDamage (lines 392-398, the SDT_ABILITY /
// MTYPE_DAMAGE physical branch):
//
//     dwDefendPower = shieldDefend + GetDefendPower();
//     dwA = max(int(physMin - defendPower), 5);
//     dwB = max(int(physMax - defendPower), 7);
//     if (hit == HT_NORMAL) value = dwA + rand() % max(int(dwB - dwA), 1);
//     else /* crit */      value = dwB + dwB * (RateX(PCD)+rand()%Init(PCD)) / 100;
//
// The attacker's min/max physical power arrive on the wire in
// CS_DEFEND_REQ (the client computes them from weapon + stats); the
// server rolls the final number against the *defender's* defense, so this
// is all the server needs to make a normal hit real.
//
// Deliberately bounded to the normal-hit physical case — the
// overwhelmingly common outcome and the one that makes the grind loop
// real. Deferred to the skill/combat-refinement wave, each documented at
// its call site:
//   * crit / miss / block selection (legacy GetAtkHitType against the
//     defender's evasion + block, gated by the client's bCP crit flag),
//   * the crit damage multiplier (needs TFORMULACHART: GetFormulaRateX /
//     GetFormulaInit for FTYPE_PCD),
//   * magic damage (GetMagicDefPower + FTYPE_MCD) and the multi-entry
//     skill-data damage table (heals, %-max-HP nukes, transHP/MP, curse).

#include <algorithm>
#include <cstdint>
#include <functional>

namespace tmapsvr {

// Mirrors legacy NetCode.h HIT_TYPE so the wire value and the formula
// agree once crit/miss land.
enum class HitType : std::uint8_t
{
    Miss     = 0,   // HT_MISS
    Normal   = 1,   // HT_NORMAL
    Critical = 2,   // HT_CRITICAL
    Block    = 3,   // HT_BLOCK
};

// The [lo, hi) interval a normal hit samples after defense is applied.
// Both endpoints floor at the legacy 5 / 7 minimums so an over-armored
// target still takes a little. Pure → exactly unit-testable without RNG.
struct DamageBounds
{
    std::uint32_t lo = 0;
    std::uint32_t hi = 0;
};

inline DamageBounds PhysDamageBounds(std::uint32_t phys_min,
                                     std::uint32_t phys_max,
                                     std::uint32_t defend_power)
{
    // Signed subtraction then floor, matching legacy max(int(min-def), 5).
    const std::int64_t a = static_cast<std::int64_t>(phys_min) - defend_power;
    const std::int64_t b = static_cast<std::int64_t>(phys_max) - defend_power;
    DamageBounds d;
    d.lo = static_cast<std::uint32_t>(std::max<std::int64_t>(a, 5));
    d.hi = static_cast<std::uint32_t>(std::max<std::int64_t>(b, 7));
    return d;
}

// rand_below(n) returns a value in [0, n); callers pass a thin lambda over
// their RNG (production) or a deterministic stub (tests). Equivalent to
// the legacy `dwA + rand() % max(dwB - dwA, 1)`.
inline std::uint32_t RollPhysicalDamage(
    std::uint32_t phys_min,
    std::uint32_t phys_max,
    std::uint32_t defend_power,
    const std::function<std::uint32_t(std::uint32_t)>& rand_below)
{
    const DamageBounds d = PhysDamageBounds(phys_min, phys_max, defend_power);
    const std::uint32_t span = d.hi > d.lo ? d.hi - d.lo : 1u;   // legacy max(B-A,1)
    return d.lo + rand_below(span);
}

} // namespace tmapsvr
