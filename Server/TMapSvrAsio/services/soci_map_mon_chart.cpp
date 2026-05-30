#include "soci_map_mon_chart.h"

#include "db/queries.h"
#include "db/row_helpers.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace tmapsvr {

SociMapMonChart::SociMapMonChart(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();
    auto& sql  = *lease;

    std::int32_t row_spawn = 0, row_mon = 0, row_essential = 0,
                 row_leader = 0, row_prob = 0;

    soci::statement st = (sql.prepare << queries::AllMapMon,
        soci::into(row_spawn),     soci::into(row_mon),
        soci::into(row_essential), soci::into(row_leader),
        soci::into(row_prob));
    st.execute(false);

    while (st.fetch())
    {
        MapMonEntry e;
        e.wSpawnID   = db::Narrow16(row_spawn);
        e.wMonID     = db::Narrow16(row_mon);
        e.bEssential = db::Narrow8 (row_essential);
        e.bLeader    = db::Narrow8 (row_leader);
        e.bProb      = db::Narrow8 (row_prob);
        m_rows[e.wSpawnID].push_back(e);
        ++m_count;
    }

    spdlog::info("soci_map_mon_chart: loaded {} row(s) from TMAPMONCHART "
                 "across {} spawn point(s)", m_count, m_rows.size());
}

} // namespace tmapsvr
