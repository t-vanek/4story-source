#pragma once

// Per-character quest progress service.
//
// Two tables in the legacy schema:
//   TQUESTTABLE     — one row per quest the char has on file
//                     (dwQuestID, dwTick, bCompleteCount, bTriggerCount).
//   TQUESTTERMTABLE — N rows per quest tracking per-term progress
//                     (kill counts, item-collect counts, talk-flags, …).
//
// F12 wraps both tables behind a single IQuestService::LoadProgress
// that returns the quest rows with their nested term rows attached.
// The quest section appended to DM_LOADCHAR_ACK encodes them in
// legacy wire order. Quest definitions themselves (TQUESTCHART,
// TQREWARDCHART, etc.) are gameplay-policy data — the quest engine
// that evaluates triggers / awards rewards lands in a later phase
// alongside CS_QUESTEXEC_REQ's actual side-effects.

#include <cstdint>
#include <vector>

namespace tmapsvr {

struct QuestTermRow
{
    std::uint32_t  dwTermID    = 0;
    std::uint8_t   bTermType   = 0;
    std::uint8_t   bCount      = 0;
};

struct QuestProgressRow
{
    std::uint32_t              dwQuestID       = 0;
    std::uint32_t              dwTick          = 0;     // accept timestamp
    std::uint8_t               bCompleteCount  = 0;     // how many times completed (repeatables)
    std::uint8_t               bTriggerCount   = 0;     // how many times trigger fired
    std::vector<QuestTermRow>  terms;
};

class IQuestService
{
public:
    virtual ~IQuestService() = default;

    // All TQUESTTABLE rows for `char_id`, with TQUESTTERMTABLE rows
    // grouped under each quest. Empty vector on missing rows or DB
    // error.
    virtual std::vector<QuestProgressRow>
        LoadProgress(std::uint32_t char_id) = 0;
};

} // namespace tmapsvr
