#include "soci_quest_chart.h"

#include "db/queries.h"
#include "db/row_helpers.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <utility>

namespace tmapsvr {

SociQuestChart::SociQuestChart(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();
    auto& sql  = *lease;

    // 1) Quest headers — TQUESTCHART.
    {
        std::int32_t q = 0, parent = 0, type = 0, trig = 0, trig_id = 0,
                     cmax = 0, lvl = 0;
        soci::statement st = (sql.prepare << queries::AllQuestChart,
            soci::into(q),     soci::into(parent), soci::into(type),
            soci::into(trig),  soci::into(trig_id), soci::into(cmax),
            soci::into(lvl));
        st.execute(false);
        while (st.fetch())
        {
            QuestDef d;
            d.dwQuestID    = static_cast<std::uint32_t>(q);
            d.dwParentID   = static_cast<std::uint32_t>(parent);
            d.bType        = db::Narrow8(type);
            d.bTriggerType = db::Narrow8(trig);
            d.dwTriggerID  = static_cast<std::uint32_t>(trig_id);
            d.bCountMax    = db::Narrow8(cmax);
            d.bLevel       = db::Narrow8(lvl);
            m_defs[d.dwQuestID] = std::move(d);
        }
    }

    // 2) Objective terms — TQUESTTERMCHART, attached to their quest.
    std::size_t term_rows = 0;
    {
        std::int32_t qid = 0, term_type = 0, term_id = 0, cnt = 0;
        soci::statement st = (sql.prepare << queries::AllQuestTermChart,
            soci::into(qid), soci::into(term_type), soci::into(term_id),
            soci::into(cnt));
        st.execute(false);
        while (st.fetch())
        {
            const auto it = m_defs.find(static_cast<std::uint32_t>(qid));
            if (it == m_defs.end()) continue;   // term for an unknown quest
            QuestTermDef t;
            t.dwTermID   = static_cast<std::uint32_t>(term_id);
            t.bTermType  = db::Narrow8(term_type);
            t.bGoalCount = db::Narrow8(cnt);
            it->second.terms.push_back(t);
            ++term_rows;
        }
    }

    // 3) Rewards — TQREWARDCHART, attached to their quest.
    std::size_t reward_rows = 0;
    {
        std::int32_t qid = 0, rtype = 0, rid = 0, method = 0, data = 0, cnt = 0;
        soci::statement st = (sql.prepare << queries::AllQuestRewardChart,
            soci::into(qid),    soci::into(rtype), soci::into(rid),
            soci::into(method), soci::into(data),  soci::into(cnt));
        st.execute(false);
        while (st.fetch())
        {
            const auto it = m_defs.find(static_cast<std::uint32_t>(qid));
            if (it == m_defs.end()) continue;
            QuestRewardDef rw;
            rw.bRewardType = db::Narrow8(rtype);
            rw.dwRewardID  = static_cast<std::uint32_t>(rid);
            rw.bTakeMethod = db::Narrow8(method);
            rw.bTakeData   = db::Narrow8(data);
            rw.bCount      = db::Narrow8(cnt);
            it->second.rewards.push_back(rw);
            ++reward_rows;
        }
    }

    spdlog::info("soci_quest_chart: loaded {} quest(s), {} term(s), {} "
                 "reward(s) from TQUESTCHART/TQUESTTERMCHART/TQREWARDCHART",
                 m_defs.size(), term_rows, reward_rows);
}

} // namespace tmapsvr
