#include "soci_mon_item_chart.h"

#include "db/queries.h"
#include "db/row_helpers.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace tmapsvr {

SociMonItemChart::SociMonItemChart(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();
    auto& sql  = *lease;

    std::int32_t row_mon = 0, row_chart = 0, row_item = 0, row_min = 0,
                 row_max = 0, row_weight = 0, row_lvl_min = 0, row_lvl_max = 0,
                 row_n1 = 0, row_n2 = 0, row_n3 = 0, row_n4 = 0,
                 row_m = 0, row_s = 0, row_r = 0,
                 row_magic_opt = 0, row_rare_opt = 0;

    soci::statement st = (sql.prepare << queries::AllMonItems,
        soci::into(row_mon),     soci::into(row_chart),
        soci::into(row_item),    soci::into(row_min),
        soci::into(row_max),     soci::into(row_weight),
        soci::into(row_lvl_min), soci::into(row_lvl_max),
        soci::into(row_n1),      soci::into(row_n2),
        soci::into(row_n3),      soci::into(row_n4),
        soci::into(row_m),       soci::into(row_s),
        soci::into(row_r),       soci::into(row_magic_opt),
        soci::into(row_rare_opt));
    st.execute(false);

    while (st.fetch())
    {
        MonItemEntry e;
        e.wMonID                 = db::Narrow16(row_mon);
        e.bChartType             = db::Narrow8 (row_chart);
        e.wItemID                = db::Narrow16(row_item);
        e.wItemIDMin             = db::Narrow16(row_min);
        e.wItemIDMax             = db::Narrow16(row_max);
        e.wWeight                = db::Narrow16(row_weight);
        e.bLevelMin              = db::Narrow8 (row_lvl_min);
        e.bLevelMax              = db::Narrow8 (row_lvl_max);
        e.bItemProb[MIP_NORMAL1] = db::Narrow8 (row_n1);
        e.bItemProb[MIP_NORMAL2] = db::Narrow8 (row_n2);
        e.bItemProb[MIP_NORMAL3] = db::Narrow8 (row_n3);
        e.bItemProb[MIP_NORMAL4] = db::Narrow8 (row_n4);
        e.bItemProb[MIP_MAGIC]   = db::Narrow8 (row_m);
        e.bItemProb[MIP_SET]     = db::Narrow8 (row_s);
        e.bItemProb[MIP_RARE]    = db::Narrow8 (row_r);
        e.bItemMagicOpt          = db::Narrow8 (row_magic_opt);
        e.bItemRareOpt           = db::Narrow8 (row_rare_opt);
        m_rows[e.wMonID].push_back(e);
        ++m_count;
    }

    spdlog::info("soci_mon_item_chart: loaded {} row(s) from TMONITEMCHART "
                 "across {} monster(s)", m_count, m_rows.size());
}

} // namespace tmapsvr
