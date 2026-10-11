#include "soci_quest_chart.h"

#include "db/queries.h"
#include "db/row_helpers.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <utility>
#include <algorithm>
#include <functional>
#include <map>

namespace tmapsvr {

SociQuestChart::SociQuestChart(fourstory::db::SessionPool& pool)
    : SociQuestChart(*pool.Acquire())
{
    // The temporary Lease lives until the delegated constructor returns.
}

SociQuestChart::SociQuestChart(soci::session& sql)
{

    // 1) Quest headers — TQUESTCHART.
    {
        std::int32_t q = 0, parent = 0, type = 0, trig = 0, trig_id = 0,
                     cmax = 0, lvl = 0, force = 0, main = 0, condition_check = 0;
        soci::statement st = (sql.prepare << queries::AllQuestChart,
            soci::into(q),     soci::into(parent), soci::into(type),
            soci::into(trig),  soci::into(trig_id), soci::into(cmax),
            soci::into(lvl),soci::into(force),soci::into(main),soci::into(condition_check));
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
            d.force_run=db::Narrow8(force);d.main=db::Narrow8(main);d.condition_check=db::Narrow8(condition_check);
            d.source_order=m_defs.size();
            m_defs[d.dwQuestID] = std::move(d);
        }
    }

    // TMapSvr::Init/LoadQuestTemp registers all roots, then their immediate
    // children, recursing through the unsigned-ID child map. An orphan or a
    // disconnected parent cycle is archival data, not a registered quest.
    {
        std::map<std::uint32_t,std::vector<const QuestDef*>> children;
        for(const auto& [id,def]:m_defs)children[def.dwParentID].push_back(&def);
        for(auto& [parent,defs]:children)std::sort(defs.begin(),defs.end(),[](auto a,auto b){return a->source_order<b->source_order;});
        const auto add=[&](const QuestDef* def){
            if(!m_registered.insert(def->dwQuestID).second)throw std::runtime_error("Cyclic source quest hierarchy");
            m_order.push_back(def->dwQuestID);
        };
        std::function<void(const QuestDef*,unsigned)> load=[&](const QuestDef* parent,unsigned depth){
            if(depth>256)throw std::runtime_error("Quest hierarchy exceeds safe recursion depth");
            auto ordered=children[parent->dwQuestID];
            for(auto* child:ordered)add(child);
            std::sort(ordered.begin(),ordered.end(),[](auto a,auto b){return a->dwQuestID<b->dwQuestID;});
            for(auto* child:ordered)load(child,depth+1);
        };
        for(auto* root:children[0])add(root);
        for(auto* root:children[0])load(root,0);
    }

    {
        std::int32_t quest=0,id=0,type=0,count=0;
        soci::statement st=(sql.prepare << queries::AllQuestConditions,
            soci::into(quest),soci::into(id),soci::into(type),soci::into(count));
        st.execute(false);
        while(st.fetch()) {
            const auto found=m_defs.find(static_cast<std::uint32_t>(quest));
            // Source conditions whose parent definition is absent stay in the
            // archival catalog; they cannot become a runtime quest by themselves.
            if(found==m_defs.end())continue;
            found->second.conditions.push_back({static_cast<std::uint32_t>(id),db::Narrow8(type),db::Narrow8(count)});
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
            if (it == m_defs.end()) { ++m_unresolved_terms; continue; }
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
            if (it == m_defs.end()) { ++m_unresolved_rewards; continue; }
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
    if (m_unresolved_terms || m_unresolved_rewards)
        spdlog::warn("soci_quest_chart: retained source gaps: {} unknown-quest terms, {} unknown-quest rewards",
            m_unresolved_terms, m_unresolved_rewards);
    if(m_defs.size()!=m_order.size())
        spdlog::warn("soci_quest_chart: {} definitions have no source root registration",m_defs.size()-m_order.size());
}

} // namespace tmapsvr
