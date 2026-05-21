#pragma once

// Quest progress — per-char quest state. F12 service loads from
// TQUESTTABLE (one row per quest) and groups in TQUESTTERMTABLE
// term rows under each parent.

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
    std::uint8_t               bCompleteCount  = 0;     // repeatable hit count
    std::uint8_t               bTriggerCount   = 0;     // trigger-fire count
    std::vector<QuestTermRow>  terms;
};

} // namespace tmapsvr
