#pragma once

#include "skill_chart.h"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociSkillChart final : public ISkillTemplateChart
{
public:
    explicit SociSkillChart(fourstory::db::SessionPool& pool);

    std::optional<SkillTemplate>
        Find(std::uint16_t skill_id) const override
    {
        const auto it = m_rows.find(skill_id);
        if (it == m_rows.end()) return std::nullopt;
        return it->second;
    }

    std::size_t Size() const override { return m_rows.size(); }

private:
    std::unordered_map<std::uint16_t, SkillTemplate> m_rows;
};

} // namespace tmapsvr
