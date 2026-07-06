#include "soci_skill_data_chart.h"

#include "db/queries.h"
#include "db/row_helpers.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace tmapsvr {

SociSkillDataChart::SociSkillDataChart(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();
    auto& sql  = *lease;

    std::int32_t row_skill = 0, row_action = 0, row_type = 0, row_attr = 0,
                 row_exec = 0, row_inc = 0, row_value = 0, row_value_inc = 0,
                 row_calc = 0;

    soci::statement st = (sql.prepare << queries::AllSkillData,
        soci::into(row_skill),  soci::into(row_action),
        soci::into(row_type),   soci::into(row_attr),
        soci::into(row_exec),   soci::into(row_inc),
        soci::into(row_value),  soci::into(row_value_inc),
        soci::into(row_calc));
    st.execute(false);

    while (st.fetch())
    {
        SkillDataRow d;
        d.bAction   = db::Narrow8 (row_action);
        d.bType     = db::Narrow8 (row_type);
        d.bAttr     = db::Narrow8 (row_attr);
        d.bExec     = db::Narrow8 (row_exec);
        d.bInc      = db::Narrow8 (row_inc);
        d.wValue    = db::Narrow16(row_value);
        d.wValueInc = db::Narrow16(row_value_inc);
        d.bCalc     = db::Narrow8 (row_calc);
        m_rows[db::Narrow16(row_skill)].push_back(d);
        ++m_total;
    }

    spdlog::info("soci_skill_data_chart: loaded {} effect row(s) from "
                 "TSKILLDATA across {} skill(s)", m_total, m_rows.size());
}

} // namespace tmapsvr
