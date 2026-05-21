#pragma once

#include "monster_chart.h"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociMonsterChart final : public IMonsterChart
{
public:
    explicit SociMonsterChart(fourstory::db::SessionPool& pool);

    std::optional<MonsterTemplate>
        Find(std::uint16_t template_id) const override;

    std::size_t Size() const override { return m_rows.size(); }

private:
    std::unordered_map<std::uint16_t, MonsterTemplate> m_rows;
};

} // namespace tmapsvr
