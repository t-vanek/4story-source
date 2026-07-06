#pragma once

// Skill resource-cost engine — pure, header-only (like stat_engine.h).
//
// Faithful to CTSkill::GetRequiredMP / GetRequiredHP
// (Server/TMapSvr/TSkill.cpp:185-217):
//
//     fRate = pow(m_f1stRateX, startLevel + (level-1)*nextLevel) / 100
//     type 1 (flat ): cost = (DWORD)(dwUse * fRate)
//     type 2 (ratio): cost = maxResource * dwUse / 100
//     type 0        : free
//
// SCOPE — this slice implements the **type-2 (%-of-max) path exactly** and
// **defers type-1 to 0** (no charge), on purpose:
//
//   * Type 2 needs only the char's max-HP/MP (the stat layer) and the
//     chart's dwUse*/bUse*Type — all available — so it is byte-faithful.
//
//   * Type 1's level-scale factor `m_f1stRateX` is **not a TSKILLCHART
//     column** (verified against the live schema — the table has
//     dwUseMP/bUseMPType/dwUseHP/bUseHPType/dwReuseDelay/bLevel/bMaxLevel/
//     bNextLevel but no f1stRateX / bStartLevel). The legacy template
//     sources it elsewhere, and the formula also needs the char's *per-skill
//     rank* (m_bLevel), which the modern server does not track yet. The
//     stored dwUseMP for a type-1 skill is a level-scaled base (e.g. 2451),
//     not a literal cost, so charging it flat would massively over-bill.
//     Returning 0 (the pre-gate behaviour) is the honest choice until the
//     f1stRateX source + a learned-skills rank layer land — then this
//     function gains a `rank` parameter and the type-1 branch.
//
// Keeping the math in one pure place mirrors stat_engine and keeps the
// handler glue thin + unit-testable without a live socket.

#include "services/skill_chart.h"

#include <cstdint>

namespace tmapsvr::skill_engine {

// MP the skill costs for a caster whose maximum MP is `max_mp`.
inline std::uint32_t RequiredMP(const SkillTemplate& t,
                                std::uint32_t max_mp) noexcept
{
    switch (t.bUseMPType)
    {
    case 2:  // %-of-max — faithful: dwMaxMP * dwUseMP / 100
        return static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(max_mp) * t.dwUseMP / 100u);
    case 1:  // flat × per-rank level-scale — deferred (see header note)
    default:
        return 0;
    }
}

// HP the skill costs for a caster whose maximum HP is `max_hp`. Same
// encoding as the MP cost over the bUseHPType / dwUseHP columns.
inline std::uint32_t RequiredHP(const SkillTemplate& t,
                                std::uint32_t max_hp) noexcept
{
    switch (t.bUseHPType)
    {
    case 2:
        return static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(max_hp) * t.dwUseHP / 100u);
    case 1:
    default:
        return 0;
    }
}

} // namespace tmapsvr::skill_engine
