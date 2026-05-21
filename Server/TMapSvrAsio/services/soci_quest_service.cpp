#include "soci_quest_service.h"

#include "db/queries.h"
#include "db/row_helpers.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <unordered_map>

namespace tmapsvr {

SociQuestService::SociQuestService(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

std::vector<QuestProgressRow>
SociQuestService::LoadProgress(std::uint32_t char_id)
{
    std::vector<QuestProgressRow> out;
    try
    {
        auto lease = m_pool.Acquire();
        auto& sql  = *lease;

        // Pass 1 — TQUESTTABLE: one row per accepted quest.
        std::unordered_map<std::uint32_t, std::size_t> by_quest;
        {
            std::int32_t row_qid = 0, row_tick = 0, row_cc = 0, row_tc = 0;
            soci::statement st = (sql.prepare << queries::QuestsByCharId,
                soci::use(static_cast<std::int32_t>(char_id), "cid"),
                soci::into(row_qid), soci::into(row_tick),
                soci::into(row_cc),  soci::into(row_tc));
            st.execute(false);

            while (st.fetch())
            {
                QuestProgressRow q;
                q.dwQuestID      = db::Narrow32(row_qid);
                q.dwTick         = db::Narrow32(row_tick);
                q.bCompleteCount = db::Narrow8 (row_cc);
                q.bTriggerCount  = db::Narrow8 (row_tc);
                by_quest[q.dwQuestID] = out.size();
                out.push_back(std::move(q));
            }
        }

        if (out.empty())
            return out;

        // Pass 2 — TQUESTTERMTABLE: 0..N term rows per quest. Group
        // into the parent row collected in pass 1.
        {
            std::int32_t row_qid = 0, row_tid = 0, row_type = 0, row_count = 0;
            soci::statement st = (sql.prepare << queries::QuestTermsByCharId,
                soci::use(static_cast<std::int32_t>(char_id), "cid"),
                soci::into(row_qid),
                soci::into(row_tid),
                soci::into(row_type),
                soci::into(row_count));
            st.execute(false);

            while (st.fetch())
            {
                const auto qid = db::Narrow32(row_qid);
                const auto it  = by_quest.find(qid);
                if (it == by_quest.end()) continue; // orphan term row
                QuestTermRow t;
                t.dwTermID  = db::Narrow32(row_tid);
                t.bTermType = db::Narrow8 (row_type);
                t.bCount    = db::Narrow8 (row_count);
                out[it->second].terms.push_back(t);
            }
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_quest_service: LoadProgress({}) threw: {} — "
                      "returning empty",
            char_id, ex.what());
        out.clear();
    }
    return out;
}

} // namespace tmapsvr
