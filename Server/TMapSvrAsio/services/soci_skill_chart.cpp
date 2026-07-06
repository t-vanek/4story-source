#include "soci_skill_chart.h"

#include "db/queries.h"
#include "db/row_helpers.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace tmapsvr {

SociSkillChart::SociSkillChart(fourstory::db::SessionPool& pool,
                               float f1st_rate_x)
{
    auto lease = pool.Acquire();
    auto& sql  = *lease;

    std::int32_t row_id = 0, row_reuse = 0,
                 row_usemp = 0, row_mptype = 0,
                 row_usehp = 0, row_hptype = 0,
                 row_start = 0, row_next = 0, row_max = 0;

    soci::statement st = (sql.prepare << queries::AllSkillTemplate,
        soci::into(row_id),    soci::into(row_reuse),
        soci::into(row_usemp), soci::into(row_mptype),
        soci::into(row_usehp), soci::into(row_hptype),
        soci::into(row_start), soci::into(row_next),
        soci::into(row_max));
    st.execute(false);

    while (st.fetch())
    {
        SkillTemplate s;
        s.wID          = db::Narrow16(row_id);
        s.dwReuseDelay = db::Narrow32(row_reuse);
        s.dwUseMP      = db::Narrow32(row_usemp);
        s.bUseMPType   = db::Narrow8 (row_mptype);
        s.dwUseHP      = db::Narrow32(row_usehp);
        s.bUseHPType   = db::Narrow8 (row_hptype);
        s.bStartLevel  = db::Narrow8 (row_start);
        s.bNextLevel   = db::Narrow8 (row_next);
        s.bMaxLevel    = db::Narrow8 (row_max);
        s.f1stRateX    = f1st_rate_x;
        m_rows[s.wID]  = s;
    }

    spdlog::info("soci_skill_chart: loaded {} skill template(s) "
                 "(reuse + MP/HP cost + level scale, f1stRateX={}) "
                 "from TSKILLCHART", m_rows.size(), f1st_rate_x);
}

} // namespace tmapsvr
