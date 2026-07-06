#pragma once

#include "skill_data_chart.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociSkillDataChart final : public ISkillDataChart
{
public:
    explicit SociSkillDataChart(fourstory::db::SessionPool& pool);

    const std::vector<SkillDataRow>&
        ForSkill(std::uint16_t skill_id) const override
    {
        static const std::vector<SkillDataRow> kEmpty;
        const auto it = m_rows.find(skill_id);
        return it == m_rows.end() ? kEmpty : it->second;
    }

    std::size_t Size() const override { return m_total; }

private:
    std::unordered_map<std::uint16_t, std::vector<SkillDataRow>> m_rows;
    std::size_t m_total = 0;
};

} // namespace tmapsvr
