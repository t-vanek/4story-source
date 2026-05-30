#pragma once

#include "mon_item_chart.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociMonItemChart final : public IMonItemChart
{
public:
    explicit SociMonItemChart(fourstory::db::SessionPool& pool);

    const std::vector<MonItemEntry>&
        ForMon(std::uint16_t mon_id) const override
    {
        const auto it = m_rows.find(mon_id);
        if (it == m_rows.end()) return m_empty;
        return it->second;
    }

    std::size_t Size() const override { return m_count; }

private:
    std::unordered_map<std::uint16_t, std::vector<MonItemEntry>> m_rows;
    std::vector<MonItemEntry>                                    m_empty;
    std::size_t                                                  m_count = 0;
};

} // namespace tmapsvr
