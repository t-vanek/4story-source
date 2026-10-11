#pragma once

#include "quest_chart.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace fourstory::db { class SessionPool; }
namespace soci { class session; }

namespace tmapsvr {

// Production IQuestChart — loads TQUESTCHART + TQUESTTERMCHART +
// TQREWARDCHART once at construction (boot) and joins them by quest id.
class SociQuestChart final : public IQuestChart
{
public:
    explicit SociQuestChart(fourstory::db::SessionPool& pool);
    // Shared boot transaction: all catalogs observe the same content release.
    explicit SociQuestChart(soci::session& sql);

    const QuestDef* Find(std::uint32_t quest_id) const override
    {
        const auto it = m_defs.find(quest_id);
        return it == m_defs.end() || !m_registered.contains(quest_id) ? nullptr : &it->second;
    }

    std::size_t Size() const override { return m_order.size(); }
    std::vector<const QuestDef*> All() const override {
        std::vector<const QuestDef*> out;out.reserve(m_defs.size());
        for(const auto id:m_order)out.push_back(&m_defs.at(id));
        return out;
    }
    std::size_t UnresolvedTerms() const { return m_unresolved_terms; }
    std::size_t UnresolvedRewards() const { return m_unresolved_rewards; }

private:
    std::unordered_map<std::uint32_t, QuestDef> m_defs;
    std::unordered_set<std::uint32_t> m_registered;
    std::vector<std::uint32_t> m_order;
    std::size_t m_unresolved_terms = 0;
    std::size_t m_unresolved_rewards = 0;
};

} // namespace tmapsvr
