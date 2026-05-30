#pragma once

// Skill row — per-char learned skill from TSKILLTABLE.

#include <cstdint>

namespace tmapsvr {

struct SkillRow
{
    std::uint16_t  wSkillID     = 0;
    std::uint8_t   bLevel       = 0;
    std::uint32_t  dwRemainTick = 0;   // cooldown remaining (0 = ready)
};

// Skill template (TSKILLCHART) — the static definition of a skill. This
// subset carries the reuse cooldown the cooldown gate needs; the MP/HP
// cost columns (m_dwUseMP/m_bUseMPType + the per-level m_f1stRateX rate)
// and the skill-data effect entries (TSKILLDATA) land with the MP-cost /
// effect waves, which also need the char's max-MP (not yet modelled).
struct SkillTemplate
{
    std::uint16_t  wID          = 0;
    std::uint32_t  dwReuseDelay = 0;   // ms between uses (legacy m_dwReuseDelay)
};

} // namespace tmapsvr
