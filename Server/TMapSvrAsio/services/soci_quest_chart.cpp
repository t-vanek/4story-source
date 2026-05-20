// SociQuestChart implementation.
//
// Loading strategy:
//   1. SELECT top-level quests (dwParentID=0) from TQUESTCHART.
//   2. For each quest: load term rows from TQUESTTERMCHART; merge consecutive
//      qualifier rows (QTT_MONID=7, QTT_ITEMID=5) into the preceding primary
//      term row to produce a single QuestTermTemplate with target_id.
//   3. For each quest: load reward rows from TQREWARDCHART; aggregate
//      RT_EXP and RT_ITEM rewards into QuestRewardTemplate.
//
// This matches DBAccess.h:CTBLQuestChart, CTBLQuestTermChart, CTBLQuestRewardChart.

#include "soci_quest_chart.h"

#include <spdlog/spdlog.h>

#include <soci/soci.h>

namespace tmapsvr {

// ---------------------------------------------------------------------------
// LoadTerms
// ---------------------------------------------------------------------------

void SociQuestChart::LoadTerms(soci::session& sql,
                                std::uint32_t  quest_id,
                                QuestTemplate& tmpl)
{
    // Columns: dwTermID, bTermType, bCount (ORDER BY dwID asc)
    // Source: DBAccess.h:2225-2231
    int    db_term_id  = 0;
    int    db_term_type = 0;
    int    db_count    = 0;

    soci::rowset<soci::row> rs = (sql.prepare
        << "SELECT dwTermID, bTermType, bCount "
           "FROM TQUESTTERMCHART WITH (NOLOCK) "
           "WHERE dwQuestID = :qid ORDER BY dwID",
           soci::use(static_cast<int>(quest_id)));

    QuestTermTemplate* pending = nullptr;  // primary term awaiting qualifier

    std::uint32_t auto_id = quest_id * 1000u;

    for (const auto& row : rs)
    {
        db_term_id   = row.get<int>(0, 0);
        db_term_type = row.get<int>(1, 0);
        db_count     = row.get<int>(2, 0);

        const auto tt = static_cast<std::uint8_t>(db_term_type);

        if (tt == QuestTermType::MonId || tt == QuestTermType::ItemId)
        {
            // Qualifier row — attach target_id to the pending primary term.
            if (pending)
                pending->target_id = static_cast<std::uint32_t>(db_term_id);
            pending = nullptr;
            continue;
        }

        // Primary term row (Hunt, GetItem, Talk, …)
        QuestTermTemplate t{};
        t.term_id   = static_cast<std::uint32_t>(db_term_id);
        if (t.term_id == 0) t.term_id = ++auto_id;   // guard against 0 IDs
        t.term_type = tt;
        t.count     = static_cast<std::uint8_t>(db_count ? db_count : 1);
        t.target_id = 0;    // may be set by next qualifier row

        tmpl.terms.push_back(t);
        pending = &tmpl.terms.back();
    }
}

// ---------------------------------------------------------------------------
// LoadRewards
// ---------------------------------------------------------------------------

void SociQuestChart::LoadRewards(soci::session& sql,
                                  std::uint32_t  quest_id,
                                  QuestTemplate& tmpl)
{
    // Columns: bRewardType, dwRewardID, bCount
    // Source: DBAccess.h:2113-2121
    int db_reward_type = 0;
    int db_reward_id   = 0;
    int db_count       = 0;

    soci::rowset<soci::row> rs = (sql.prepare
        << "SELECT bRewardType, dwRewardID, bCount "
           "FROM TQREWARDCHART WITH (NOLOCK) "
           "WHERE dwQuestID = :qid",
           soci::use(static_cast<int>(quest_id)));

    for (const auto& row : rs)
    {
        db_reward_type = row.get<int>(0, 0);
        db_reward_id   = row.get<int>(1, 0);
        db_count       = row.get<int>(2, 0);

        const auto rt = static_cast<std::uint8_t>(db_reward_type);

        if (rt == RewardType::Exp)
        {
            tmpl.reward.exp_reward = static_cast<std::uint32_t>(db_reward_id);
        }
        else if (rt == RewardType::Item)
        {
            tmpl.reward.item_id    = static_cast<std::uint16_t>(db_reward_id);
            tmpl.reward.item_count = static_cast<std::uint8_t>(
                db_count ? db_count : 1);
        }
        else if (rt == RewardType::Gold)
        {
            tmpl.reward.gold = static_cast<std::uint32_t>(db_reward_id);
        }
        // Other reward types (skill, class change, …) are not implemented yet.
    }
}

// ---------------------------------------------------------------------------
// LoadSync
// ---------------------------------------------------------------------------

void SociQuestChart::LoadSync(soci::session& sql)
{
    m_quests.clear();
    m_npc_quests.clear();

    // Top-level quests: dwParentID=0
    // Source: DBAccess.h:2047-2057
    soci::rowset<soci::row> rs = (sql.prepare
        << "SELECT dwQuestID, dwTriggerID, bCountMax, bType "
           "FROM TQUESTCHART "
           "WHERE dwParentID = 0 "
           "ORDER BY dwQuestID");

    std::vector<std::uint32_t> quest_ids;

    for (const auto& row : rs)
    {
        const auto quest_id   = static_cast<std::uint32_t>(row.get<int>(0, 0));
        const auto trigger_id = static_cast<std::uint32_t>(row.get<int>(1, 0));
        const auto count_max  = static_cast<std::uint8_t> (row.get<int>(2, 1));
        const auto quest_type = static_cast<std::uint8_t> (row.get<int>(3, 0));

        if (quest_id == 0) continue;

        QuestTemplate tmpl{};
        tmpl.quest_id    = quest_id;
        tmpl.trigger_npc = trigger_id;
        tmpl.count_max   = count_max;
        tmpl.quest_type  = quest_type;

        m_quests.emplace(quest_id, std::move(tmpl));
        if (trigger_id != 0)
            m_npc_quests[trigger_id].push_back(quest_id);

        quest_ids.push_back(quest_id);
    }

    // Load terms and rewards in a second pass to avoid nested rowsets.
    for (auto qid : quest_ids)
    {
        auto it = m_quests.find(qid);
        if (it == m_quests.end()) continue;
        LoadTerms  (sql, qid, it->second);
        LoadRewards(sql, qid, it->second);
    }

    spdlog::info("SociQuestChart: loaded {} quests from TQUESTCHART",
        m_quests.size());
}

// ---------------------------------------------------------------------------
// GetQuest / GetNpcQuests
// ---------------------------------------------------------------------------

const QuestTemplate* SociQuestChart::GetQuest(std::uint32_t quest_id) const
{
    auto it = m_quests.find(quest_id);
    return it != m_quests.end() ? &it->second : nullptr;
}

std::vector<const QuestTemplate*>
SociQuestChart::GetNpcQuests(std::uint32_t npc_id) const
{
    std::vector<const QuestTemplate*> result;
    auto it = m_npc_quests.find(npc_id);
    if (it == m_npc_quests.end()) return result;
    for (auto qid : it->second)
    {
        auto qit = m_quests.find(qid);
        if (qit != m_quests.end())
            result.push_back(&qit->second);
    }
    return result;
}

} // namespace tmapsvr
