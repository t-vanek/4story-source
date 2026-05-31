#pragma once

// Quest *definition* data — the read-only chart side of the quest
// system, loaded once at boot from TQUESTCHART / TQUESTTERMCHART /
// TQREWARDCHART. This is distinct from domain/quest.h, which carries
// the per-character *progress* rows (TQUESTTABLE / TQUESTTERMTABLE).
//
// Faithful to the legacy data model (TMapType.h tagQUESTTEMP /
// tagQUESTTERM / tagQUESTREWARD) and the enums in
// Lib/Own/TProtocol/include/NetCode.h. This slice models the
// kill-count quest path (QT_DIEMON + QTT_HUNT + QT_COMPLETE); the
// other action / term / trigger types are enumerated for completeness
// and for the loader, but only the hunt + complete subset is evaluated
// by quest_engine.h.

#include <cstdint>
#include <vector>

namespace tmapsvr {

// Quest action type — tagQUESTTEMP::m_bType. NetCode.h QUEST_TYPE.
enum QuestType : std::uint8_t
{
    QT_NONE        = 0,
    QT_DEFTALK     = 1,
    QT_GIVESKILL   = 2,
    QT_GIVEITEM    = 3,
    QT_DROPITEM    = 4,
    QT_SPAWNMON    = 5,
    QT_TELEPORT    = 6,
    QT_COMPLETE    = 7,
    QT_MISSION     = 8,
    QT_ROUTING     = 9,
    QT_NPCTALK     = 10,
    QT_DROPQUEST   = 11,
    QT_CHAPTERMSG  = 12,
    QT_SWITCH      = 13,
    QT_DIEMON      = 14,
    QT_DEFENDSKILL = 15,
    QT_DELETEITEM  = 16,
    QT_SENDPOST    = 17,
    QT_CRAFT       = 18,
    QT_REGEN       = 19,
    QT_GUILD       = 20,
    QT_COUNT       = 21,
};

// Quest term (objective) type — tagQUESTTERM::m_bTermType. NetCode.h
// TERM_TYPE. QTT_HUNT is the kill-monster objective this slice tracks;
// QTT_COMPQUEST names the quest a QT_COMPLETE action turns in.
enum TermType : std::uint8_t
{
    QTT_COMPQUEST      = 1,
    QTT_GETITEM        = 2,
    QTT_HUNT           = 3,
    QTT_SKILLID        = 4,
    QTT_ITEMID         = 5,
    QTT_TIMER          = 6,
    QTT_MONID          = 7,
    QTT_MAPID          = 8,
    QTT_LEFT           = 9,
    QTT_TOP            = 10,
    QTT_RIGHT          = 11,
    QTT_BOTTOM         = 12,
    QTT_TALK           = 13,
    QTT_HEIGHT         = 14,
    QTT_SWITCH         = 15,
    QTT_SPAWNID        = 16,
    QTT_USEITEM        = 17,
    QTT_QUESTCOMPLETED = 18,
    QTT_TSTART_POS     = 19,
    QTT_TCOMP_POS      = 20,
    QTT_SPAWNID_DEL    = 21,
};

// Quest trigger type — tagQUESTTEMP::m_bTriggerType. NetCode.h
// TRIGGER_TYPE. TT_KILLMON fires the kill hook in combat.
enum TriggerType : std::uint8_t
{
    TT_EXECQUEST = 1,
    TT_POSITION  = 2,
    TT_TALKNPC   = 3,
    TT_GETITEM   = 4,
    TT_KILLMON   = 5,
    TT_KILLUNIT  = 6,
    TT_RUNSWITCH = 7,
    TT_RUNGATE   = 8,
    TT_COMPLETE  = 9,
    TT_USEITEM   = 10,
    TT_LEAVEMAP  = 11,
    TT_LEVELUP   = 12,
    TT_WARREGION = 13,
};

// Per-term completion status sent in CS_QUESTUPDATE_ACK. NetCode.h.
enum QuestStatus : std::uint8_t
{
    QTS_RUN     = 0,
    QTS_SUCCESS = 1,
    QTS_FAILED  = 2,
};

// Quest action result sent in CS_QUESTCOMPLETE_ACK. NetCode.h
// TQUEST_RESULT.
enum QuestResult : std::uint8_t
{
    QR_SUCCESS       = 0,
    QR_TERM          = 1,   // a term is not yet complete
    QR_INVENTORYFULL = 2,
    QR_DROP          = 3,
};

// Reward type — tagQUESTREWARD::m_bRewardType (TQREWARDCHART.bRewardType).
// NetCode.h REWARD_TYPE. This slice grants RT_GOLD + RT_EXP; the rest are
// enumerated for the loader and future waves.
enum RewardType : std::uint8_t
{
    RT_GOLD      = 1,
    RT_ITEM      = 2,
    RT_SKILL     = 3,
    RT_SKILLUP   = 4,
    RT_CHGCLASS  = 5,
    RT_EXP       = 6,
    RT_MAGICITEM = 7,
    RT_TITLE     = 8,
    RT_SOUL      = 9,
    RT_POINT     = 10,
};

// Reward take-method — tagQUESTREWARD::m_bTakeMethod. NetCode.h
// REWARD_METHOD. This slice treats every reward as always-granted;
// RM_SELECT / RM_PROB / RM_RANDOM are follow-ups.
enum RewardMethod : std::uint8_t
{
    RM_DEFAULT = 1,
    RM_SELECT  = 2,
    RM_PROB    = 3,
    RM_RANDOM  = 4,
};

// One objective row — TQUESTTERMCHART (dwID, dwQuestID, bTermType,
// dwTermID, bCount). dwTermID is the objective target: for QTT_HUNT it
// is the monster *kind* (matched against TMONSTERCHART.wKind on kill);
// bGoalCount is how many are required.
struct QuestTermDef
{
    std::uint32_t dwTermID   = 0;
    std::uint8_t  bTermType  = 0;   // TermType
    std::uint8_t  bGoalCount = 0;
};

// One reward row — TQREWARDCHART (the subset this slice reads).
// bRewardType is RT_GOLD / RT_ITEM / RT_SKILL / …; dwRewardID is the
// gold amount or the item template id.
struct QuestRewardDef
{
    std::uint8_t  bRewardType = 0;
    std::uint32_t dwRewardID  = 0;
    std::uint8_t  bTakeMethod = 0;   // RM_DEFAULT / RM_SELECT / RM_PROB / RM_RANDOM
    std::uint8_t  bTakeData   = 0;   // probability % for RM_PROB
    std::uint8_t  bCount      = 0;   // item count
};

// A whole quest definition — TQUESTCHART header plus its joined terms
// and rewards. Faithful to tagQUESTTEMP (TMapType.h:2016) minus the
// condition vector (conditions are an accept-time gate; this slice
// keeps only the level gate, on bLevel).
struct QuestDef
{
    std::uint32_t dwQuestID    = 0;
    std::uint32_t dwParentID   = 0;
    std::uint8_t  bType        = 0;   // QuestType
    std::uint8_t  bTriggerType = 0;   // TriggerType
    std::uint32_t dwTriggerID  = 0;
    std::uint8_t  bCountMax    = 0;   // repeat cap
    std::uint8_t  bLevel       = 0;   // min level to accept
    std::vector<QuestTermDef>   terms;
    std::vector<QuestRewardDef> rewards;
};

} // namespace tmapsvr
