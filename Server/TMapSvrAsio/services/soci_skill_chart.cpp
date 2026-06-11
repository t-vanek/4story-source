#include "soci_skill_chart.h"

#include "db/queries.h"
#include "db/row_helpers.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace tmapsvr {

SociSkillChart::SociSkillChart(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();
    auto& sql  = *lease;

    std::int32_t row_id = 0, row_reuse = 0,
                 row_usemp = 0, row_mptype = 0,
                 row_usehp = 0, row_hptype = 0;

    soci::statement st = (sql.prepare << queries::AllSkillTemplate,
        soci::into(row_id),    soci::into(row_reuse),
        soci::into(row_usemp), soci::into(row_mptype),
        soci::into(row_usehp), soci::into(row_hptype));
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
        m_rows[s.wID]  = s;
    }

    spdlog::info("soci_skill_chart: loaded {} skill template(s) "
                 "(reuse + MP/HP cost) from TSKILLCHART", m_rows.size());
}

} // namespace tmapsvr
