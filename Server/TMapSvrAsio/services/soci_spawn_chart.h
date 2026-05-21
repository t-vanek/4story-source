#pragma once

#include "spawn_chart.h"

#include <vector>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociSpawnChart final : public ISpawnChart
{
public:
    explicit SociSpawnChart(fourstory::db::SessionPool& pool);

    const std::vector<SpawnPoint>& All() const override { return m_rows; }

    std::size_t Size() const override { return m_rows.size(); }

private:
    std::vector<SpawnPoint> m_rows;
};

} // namespace tmapsvr
