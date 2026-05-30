#include "soci_mon_attr_chart.h"

#include "db/queries.h"
#include "db/row_helpers.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace tmapsvr {

SociMonAttrChart::SociMonAttrChart(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();
    auto& sql  = *lease;

    std::int32_t row_id = 0, row_level = 0, row_maxhp = 0, row_maxmp = 0,
                 row_ap = 0, row_map = 0, row_dp = 0, row_mdp = 0,
                 row_minwap = 0, row_maxwap = 0, row_atkspeed = 0;

    soci::statement st = (sql.prepare << queries::AllMonAttr,
        soci::into(row_id),     soci::into(row_level),
        soci::into(row_maxhp),  soci::into(row_maxmp),
        soci::into(row_ap),     soci::into(row_map),
        soci::into(row_dp),     soci::into(row_mdp),
        soci::into(row_minwap), soci::into(row_maxwap),
        soci::into(row_atkspeed));
    st.execute(false);

    while (st.fetch())
    {
        MonsterAttr a;
        a.wID        = db::Narrow16(row_id);
        a.bLevel     = db::Narrow8 (row_level);
        a.dwMaxHP    = db::Narrow32(row_maxhp);
        a.dwMaxMP    = db::Narrow32(row_maxmp);
        a.wAP        = db::Narrow16(row_ap);
        a.wMAP       = db::Narrow16(row_map);
        a.wDP        = db::Narrow16(row_dp);
        a.wMDP       = db::Narrow16(row_mdp);
        a.wMinWAP    = db::Narrow16(row_minwap);
        a.wMaxWAP    = db::Narrow16(row_maxwap);
        a.dwAtkSpeed = db::Narrow32(row_atkspeed);
        m_rows[MonAttrKey(a.wID, a.bLevel)] = a;
    }

    spdlog::info("soci_mon_attr_chart: loaded {} (monster,level) stat row(s) "
                 "from TMONATTRCHART", m_rows.size());
}

} // namespace tmapsvr
