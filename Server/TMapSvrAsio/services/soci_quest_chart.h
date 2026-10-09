#pragma once

#include "quest_chart.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

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
        return it == m_defs.end() ? nullptr : &it->second;
    }

    std::size_t Size() const override { return m_defs.size(); }
    std::size_t UnresolvedTerms() const { return m_unresolved_terms; }
    std::size_t UnresolvedRewards() const { return m_unresolved_rewards; }

private:
    std::unordered_map<std::uint32_t, QuestDef> m_defs;
    std::size_t m_unresolved_terms = 0;
    std::size_t m_unresolved_rewards = 0;
};

} // namespace tmapsvr
