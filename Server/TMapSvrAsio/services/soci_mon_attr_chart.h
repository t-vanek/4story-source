#pragma once

#include "mon_attr_chart.h"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociMonAttrChart final : public IMonAttrChart
{
public:
    explicit SociMonAttrChart(fourstory::db::SessionPool& pool);

    std::optional<MonsterAttr>
        Find(std::uint16_t mon_id, std::uint8_t level) const override
    {
        const auto it = m_rows.find(MonAttrKey(mon_id, level));
        if (it == m_rows.end()) return std::nullopt;
        return it->second;
    }

    std::size_t Size() const override { return m_rows.size(); }

private:
    std::unordered_map<std::uint32_t, MonsterAttr> m_rows;
};

} // namespace tmapsvr
