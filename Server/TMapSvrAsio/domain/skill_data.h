#pragma once

// Skill-effect data — one TSKILLDATA row describes one effect a skill
// applies (heal, damage modifier, status, …). A skill owns N rows; the
// legacy CTSkillTemp::m_vData vector (TMapType.h:1945, loaded in
// TMapSvr.cpp). Enums verbatim from Lib/Own/TProtocol/include/NetCode.h —
// 4Story's wire + chart encoding, do not renumber.

#include <cstdint>

namespace tmapsvr {

// NetCode.h:352 (TSKILL_RESULT) — CS_SKILLUSE_ACK result codes. Only the
// ones the modern server emits are named; the wire carries the raw byte.
enum SkillResult : std::uint8_t
{
    SKILL_SUCCESS   = 0,
    SKILL_NOTFOUND  = 1,
    SKILL_SPEEDYUSE = 6,
    SKILL_NEEDMP    = 7,
    SKILL_NEEDHP    = 8,
};

// NetCode.h:1521 (SKILL_DATA_INC) — how a computed value combines with the
// base quantity in CTSkillTemp::Calculate. NOTE: starts at 1.
enum SkillValueInc : std::uint8_t
{
    SVI_INCREASE = 1,
    SVI_DECREASE = 2,
    SVI_MULTIPLY = 3,
    SVI_DIVIDE   = 4,
    SVI_PRECENT  = 5,   // sic — legacy spelling
};

// NetCode.h:1530 (SKILL_ACTION) — when the effect runs.
enum SkillAction : std::uint8_t
{
    SA_ONCE     = 0,
    SA_CONTINUE = 1,
    SA_DOT      = 2,
    SA_BUFF     = 3,
    SA_PASSIVE  = 4,
};

// NetCode.h:1540 (SKILL_DATA_TYPE) — the effect family (row.bType).
enum SkillDataType : std::uint8_t
{
    SDT_EQUIP   = 0,
    SDT_ABILITY = 1,
    SDT_RECALL  = 2,
    SDT_TRANS   = 3,
    SDT_TRAP    = 4,
    SDT_CURE    = 5,
    SDT_STATUS  = 6,
    SDT_AI      = 7,
    SDT_ITEM    = 8,
};

// NetCode.h:1554 (SKILL_CURE_TYPE) — SDT_CURE sub-type (row.bExec).
// Only the subset the cure wave handles is named; values are canonical.
enum SkillCureType : std::uint8_t
{
    SCT_REVIVAL = 1,
    SCT_HP      = 8,
    SCT_HPTRANS = 15,
    SCT_MPTRANS = 16,
    SCT_MP      = 19,
};

// One TSKILLDATA row (tagTSKILLDATA, TMapType.h:1945). wSkillID is the
// grouping key and lives on the chart, not the row.
struct SkillDataRow
{
    std::uint8_t  bAction    = 0;   // SkillAction — once / DOT / buff / …
    std::uint8_t  bType      = 0;   // SkillDataType — SDT_CURE / SDT_ABILITY / …
    std::uint8_t  bAttr      = 0;   // attack attribute (physic / magic school)
    std::uint8_t  bExec      = 0;   // sub-type within bType (SCT_HP, MTYPE_…)
    std::uint8_t  bInc       = 0;   // SkillValueInc — how the value combines
    std::uint16_t wValue     = 0;   // base magnitude
    std::uint16_t wValueInc  = 0;   // per-level increment (calc modes 1/3)
    std::uint8_t  bCalc      = 0;   // 0 const / 1 linear / 2 exponential / 3 falling
};

} // namespace tmapsvr
