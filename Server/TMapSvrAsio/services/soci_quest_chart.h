#pragma once

#include "quest_chart.h"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

// Production IQuestChart — loads TQUESTCHART + TQUESTTERMCHART +
// TQREWARDCHART once at construction (boot) and joins them by quest id.
class SociQuestChart final : public IQuestChart
{
public:
    explicit SociQuestChart(fourstory::db::SessionPool& pool);

    const QuestDef* Find(std::uint32_t quest_id) const override
    {
        const auto it = m_defs.find(quest_id);
        return it == m_defs.end() ? nullptr : &it->second;
    }

    std::size_t Size() const override { return m_defs.size(); }

private:
    std::unordered_map<std::uint32_t, QuestDef> m_defs;
};

} // namespace tmapsvr
