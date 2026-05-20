#pragma once

// SociQuestChart — loads quest templates from TQUESTCHART + TQUESTTERMCHART
// + TQREWARDCHART at startup via a synchronous SOCI query (called from a
// CoOffload thread in main()).
//
// Schema references (DBAccess.h:2047, 2113, 2225):
//   TQUESTCHART       : dwQuestID, dwTriggerID, bTriggerType, bCountMax, bType
//   TQUESTTERMCHART   : dwQuestID, dwTermID, bTermType, bCount
//   TQREWARDCHART     : dwQuestID, bRewardType, dwRewardID, bCount
//
// Term pairing: consecutive rows in TQUESTTERMCHART for the same quest where
// the first row is a primary term (Hunt=3, GetItem=2, Talk=13) and the
// next row is a qualifier (MonId=7, ItemId=5) are merged into one
// QuestTermTemplate with target_id = qualifier's dwTermID.

#include "quest_engine.h"

#include <soci/soci.h>
#include <unordered_map>
#include <vector>

namespace tmapsvr {

class SociQuestChart : public IQuestChart
{
public:
    // Load all top-level quests from the database. Call once at startup
    // inside a CoOffload block (blocking; soci::session is not thread-safe).
    void LoadSync(soci::session& sql);

    const QuestTemplate* GetQuest(std::uint32_t quest_id) const override;

    std::vector<const QuestTemplate*>
        GetNpcQuests(std::uint32_t npc_id) const override;

    std::size_t Size() const { return m_quests.size(); }

private:
    void LoadTerms(soci::session& sql,
                   std::uint32_t  quest_id,
                   QuestTemplate& tmpl);
    void LoadRewards(soci::session& sql,
                     std::uint32_t  quest_id,
                     QuestTemplate& tmpl);

    std::unordered_map<std::uint32_t, QuestTemplate>              m_quests;
    std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> m_npc_quests;
};

} // namespace tmapsvr
