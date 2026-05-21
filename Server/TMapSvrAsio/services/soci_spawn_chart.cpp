#include "soci_spawn_chart.h"

#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace tmapsvr {

SociSpawnChart::SociSpawnChart(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();
    auto& sql  = *lease;

    std::int32_t row_id = 0, row_group = 0, row_local = 0, row_map = 0,
                 row_dir = 0, row_country = 0, row_count = 0, row_range = 0,
                 row_area = 0, row_link = 0, row_prob = 0, row_roam = 0;
    double       row_x = 0.0, row_y = 0.0, row_z = 0.0;

    soci::statement st = (sql.prepare <<
        "SELECT wID, wGroup, wLocalID, wMapID, fPosX, fPosY, fPosZ, "
        "  wDir, bCountry, bCount, bRange, bArea, bLink, bProb, bRoamType "
        "FROM TMONSPAWNCHART",
        soci::into(row_id),      soci::into(row_group),
        soci::into(row_local),   soci::into(row_map),
        soci::into(row_x),       soci::into(row_y),
        soci::into(row_z),       soci::into(row_dir),
        soci::into(row_country), soci::into(row_count),
        soci::into(row_range),   soci::into(row_area),
        soci::into(row_link),    soci::into(row_prob),
        soci::into(row_roam));
    st.execute(false);

    while (st.fetch())
    {
        SpawnPoint p;
        p.wID       = static_cast<std::uint16_t>(row_id);
        p.wGroup    = static_cast<std::uint16_t>(row_group);
        p.wLocalID  = static_cast<std::uint16_t>(row_local);
        p.wMapID    = static_cast<std::uint16_t>(row_map);
        p.fPosX     = static_cast<float>(row_x);
        p.fPosY     = static_cast<float>(row_y);
        p.fPosZ     = static_cast<float>(row_z);
        p.wDir      = static_cast<std::uint16_t>(row_dir);
        p.bCountry  = static_cast<std::uint8_t> (row_country);
        p.bCount    = static_cast<std::uint8_t> (row_count);
        p.bRange    = static_cast<std::uint8_t> (row_range);
        p.bArea     = static_cast<std::uint8_t> (row_area);
        p.bLink     = static_cast<std::uint8_t> (row_link);
        p.bProb     = static_cast<std::uint8_t> (row_prob);
        p.bRoamType = static_cast<std::uint8_t> (row_roam);
        m_rows.push_back(p);
    }

    spdlog::info("soci_spawn_chart: loaded {} row(s) from TMONSPAWNCHART",
        m_rows.size());
}

} // namespace tmapsvr
