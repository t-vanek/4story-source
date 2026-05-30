#pragma once

#include "map_mon_chart.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociMapMonChart final : public IMapMonChart
{
public:
    explicit SociMapMonChart(fourstory::db::SessionPool& pool);

    const std::vector<MapMonEntry>&
        ForSpawn(std::uint16_t spawn_id) const override
    {
        const auto it = m_rows.find(spawn_id);
        if (it == m_rows.end()) return m_empty;
        return it->second;
    }

    std::size_t Size() const override { return m_count; }

private:
    std::unordered_map<std::uint16_t, std::vector<MapMonEntry>> m_rows;
    std::vector<MapMonEntry>                                    m_empty;
    std::size_t                                                 m_count = 0;
};

} // namespace tmapsvr
