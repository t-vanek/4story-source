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

// Skill template (TSKILLCHART) — the static definition of a skill: the
// reuse cooldown, the MP/HP cost columns, and the level-scale
// coefficients. The skill-data effect entries live on the separate
// TSKILLDATA chart (domain/skill_data.h).
struct SkillTemplate
{
    std::uint16_t  wID          = 0;
    std::uint32_t  dwReuseDelay = 0;   // ms between uses (legacy m_dwReuseDelay)

    // ---- resource cost (Wave 4b), faithful to CTSkill::GetRequiredMP/HP ----
    // Encoding: 0 = none, 1 = flat x per-rank level-scale, 2 = %-of-max.
    // Type-1 stays deferred (skill_engine.h): the level-scale coefficients
    // are loaded now (below), but the caster's per-skill RANK isn't
    // tracked yet.
    std::uint8_t   bUseMPType   = 0;
    std::uint32_t  dwUseMP      = 0;
    std::uint8_t   bUseHPType   = 0;
    std::uint32_t  dwUseHP      = 0;

    // ---- level-scale coefficients (Wave 4c) ----
    // The exponential calc mode (TSKILLDATA bCalc=2; later the type-1 cost)
    // raises f1stRateX to bStartLevel + (lvl-1)*bNextLevel
    // (TSkillTemp.cpp:71). bStartLevel/bNextLevel/bMaxLevel come from
    // TSKILLCHART.bLevel/bNextLevel/bMaxLevel (TMapSvr.cpp:2705-2707);
    // f1stRateX is NOT a chart column — it's TFORMULACHART[FTYPE_1ST].fRateX
    // stamped onto every template at load (TMapSvr.cpp:2673+2730; live 1.03).
    std::uint8_t   bStartLevel  = 0;
    std::uint8_t   bNextLevel   = 0;
    std::uint8_t   bMaxLevel    = 0;
    float          f1stRateX    = 1.0f;
};

} // namespace tmapsvr
