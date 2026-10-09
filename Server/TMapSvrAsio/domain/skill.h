#pragma once

// Skill row — per-char learned skill from TSKILLTABLE.

#include <cstdint>

namespace tmapsvr {
enum class SkillItemGate : std::uint8_t { Unsupported, Allowed, Unsuitable, Reagent, Ammunition };

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
    // Native resource gates use the loaded per-character rank.
    std::uint8_t   bUseMPType   = 0;
    std::uint32_t  dwUseMP      = 0;
    std::uint8_t   bUseHPType   = 0;
    std::uint32_t  dwUseHP      = 0;

    // ---- level-scale coefficients (Wave 4c) ----
    // The exponential calc mode (TSKILLDATA bCalc=2; later the type-1 cost)
    // raises f1stRateX to bStartLevel + (rank-1)*bNextLevel
    // (TSkillTemp.cpp:71). bStartLevel/bNextLevel/bMaxLevel come from
    // TSKILLCHART.bLevel/bNextLevel/bMaxLevel (TMapSvr.cpp:2705-2707);
    // f1stRateX is NOT a chart column — it's TFORMULACHART[FTYPE_1ST].fRateX
    // stamped onto every template at load (TMapSvr.cpp:2673+2730; live 1.03).
    std::uint8_t   bStartLevel  = 0;
    std::uint8_t   bNextLevel   = 0;
    std::uint8_t   bMaxLevel    = 0;
    float          f1stRateX    = 1.0f;
    std::int32_t   nReuseDelayInc = 0;
    std::uint32_t  dwKindDelay = 0;
    std::uint8_t   bKind = 0;
    std::uint8_t   bSpeedApply = 0; // TAD_NONE / PHYSICAL / LONG / MAGIC
    std::uint32_t  dwLoopDelay = 0;
    std::uint16_t  wTargetActiveID = 0;
    // Per-character UseSkillItem projection. Unsupported must never be a free cast.
    SkillItemGate items = SkillItemGate::Unsupported;
    std::uint16_t  wPrevActiveID = 0; // normal use; loop uses wTargetActiveID
    std::uint16_t  wMapID = 0xffff;  // source INVALID_MAPID: unrestricted
    std::uint16_t  wUseItem = 0;
    std::uint8_t bAmmoKind = 0; // pinned first compatible weapon, single-hit consumption
};

struct SkillAttackTiming {
    std::uint32_t delay = 0;
    std::uint32_t rate = 100;
};

} // namespace tmapsvr
