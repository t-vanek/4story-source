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

    std::int32_t row_id = 0, row_reuse = 0;

    soci::statement st = (sql.prepare << queries::AllSkillReuse,
        soci::into(row_id), soci::into(row_reuse));
    st.execute(false);

    while (st.fetch())
    {
        SkillTemplate s;
        s.wID          = db::Narrow16(row_id);
        s.dwReuseDelay = db::Narrow32(row_reuse);
        m_rows[s.wID]  = s;
    }

    spdlog::info("soci_skill_chart: loaded {} skill template(s) (reuse delay) "
                 "from TSKILLCHART", m_rows.size());
}

} // namespace tmapsvr
