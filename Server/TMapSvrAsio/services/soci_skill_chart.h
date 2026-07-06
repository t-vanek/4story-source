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
    // `f1st_rate_x` is TFORMULACHART[FTYPE_1ST].fRateX — the stat chart
    // loads it; main passes it through so every template carries the
    // level-scale base the way TMapSvr.cpp:2730 stamps it. Defaults to 1
    // (no growth) for the no-stat-chart path.
    explicit SociSkillChart(fourstory::db::SessionPool& pool,
                            float f1st_rate_x = 1.0f);

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
