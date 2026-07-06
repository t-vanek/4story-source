#pragma once

// Quest engine — the pure, data-driven evaluation core (no I/O, no
// services). Operates on a player's QuestProgressRow (domain/quest.h)
// against a QuestDef (domain/quest_def.h). This is the register-based,
// DB-sourced approach from QUEST_ENGINE.md: no scripting — just the
// fixed term semantics parameterised by the chart data.
//
// This slice implements the kill-count path faithfully:
//   - AdvanceHunt: a QTT_HUNT term advances when a monster of the
//     matching kind dies (legacy CQuest::FindRunningTerm + the m_bCount
//     increment, Quest.cpp:204-250, driven from TMonster.cpp:683's
//     CheckQuest(..., m_wKind, QTT_HUNT, TT_KILLMON, 1)).
//   - IsComplete / FirstIncompleteTerm: a quest's terms are all met
//     (legacy CQuest::CheckComplete → NULL when done,
//     QuestComplete.cpp:45).
// Item-collect (QTT_GETITEM), talk, switch, and timer terms are not yet
// evaluated; until their subsystems land they only count as complete if
// a progress row already meets the goal.

#include "domain/quest.h"
#include "domain/quest_def.h"

#include <cstdint>

namespace tmapsvr::quest_engine {

// The QTT_HUNT term in `def` whose target monster kind is `mon_kind`,
// or nullptr when this quest doesn't hunt that monster.
inline const QuestTermDef*
FindHuntTerm(const QuestDef& def, std::uint32_t mon_kind)
{
    for (const auto& t : def.terms)
        if (t.bTermType == QTT_HUNT && t.dwTermID == mon_kind)
            return &t;
    return nullptr;
}

// The progress term row matching (term_id, term_type) in `progress`,
// or nullptr. The lookup half of CQuest::FindRunningTerm.
inline QuestTermRow*
FindProgressTerm(QuestProgressRow& progress,
                 std::uint32_t term_id, std::uint8_t term_type)
{
    for (auto& t : progress.terms)
        if (t.dwTermID == term_id && t.bTermType == term_type)
            return &t;
    return nullptr;
}

struct AdvanceResult
{
    bool          advanced = false;   // a hunt term took this kill
    std::uint32_t term_id  = 0;
    std::uint8_t  count    = 0;       // count after the increment
    std::uint8_t  goal     = 0;       // required count
    std::uint8_t  status   = QTS_RUN; // QTS_SUCCESS once count >= goal
};

// A monster of `mon_kind` died — advance the matching hunt term in
// `progress` by one (capped at the goal). Returns advanced=false when
// this quest has no hunt term for that monster. Faithful to the legacy
// kill path: lazily materialises the running term at 0, counts up to
// the goal, flags SUCCESS at the goal.
inline AdvanceResult
AdvanceHunt(QuestProgressRow& progress, const QuestDef& def,
            std::uint32_t mon_kind)
{
    AdvanceResult out;
    const QuestTermDef* def_term = FindHuntTerm(def, mon_kind);
    if (!def_term) return out;            // this quest doesn't want it

    QuestTermRow* row = FindProgressTerm(progress, def_term->dwTermID, QTT_HUNT);
    if (!row)
    {
        QuestTermRow fresh;               // CQuest::FindRunningTerm lazy-add
        fresh.dwTermID  = def_term->dwTermID;
        fresh.bTermType = QTT_HUNT;
        fresh.bCount    = 0;
        progress.terms.push_back(fresh);
        row = &progress.terms.back();
    }

    if (row->bCount < def_term->bGoalCount)
        ++row->bCount;

    out.advanced = true;
    out.term_id  = def_term->dwTermID;
    out.count    = row->bCount;
    out.goal     = def_term->bGoalCount;
    out.status   = (row->bCount >= def_term->bGoalCount) ? QTS_SUCCESS : QTS_RUN;
    return out;
}

// The current progress count for a def term (0 if no progress row yet).
inline std::uint8_t
CountFor(const QuestProgressRow& progress, const QuestTermDef& dt)
{
    for (const auto& pt : progress.terms)
        if (pt.dwTermID == dt.dwTermID && pt.bTermType == dt.bTermType)
            return pt.bCount;
    return 0;
}

// Is `progress` complete against `def`? Every objective term must have
// a progress count >= its goal. Mirrors CheckComplete (NULL → done).
// QTT_COMPQUEST is the turn-in link, not a countable objective — skip
// it.
inline bool
IsComplete(const QuestProgressRow& progress, const QuestDef& def)
{
    for (const auto& dt : def.terms)
    {
        if (dt.bTermType == QTT_COMPQUEST) continue;
        if (CountFor(progress, dt) < dt.bGoalCount) return false;
    }
    return true;
}

// The first def objective term not yet satisfied (for the QR_TERM
// payload of CS_QUESTCOMPLETE_ACK), or nullptr when complete.
inline const QuestTermDef*
FirstIncompleteTerm(const QuestProgressRow& progress, const QuestDef& def)
{
    for (const auto& dt : def.terms)
    {
        if (dt.bTermType == QTT_COMPQUEST) continue;
        if (CountFor(progress, dt) < dt.bGoalCount) return &dt;
    }
    return nullptr;
}

} // namespace tmapsvr::quest_engine
