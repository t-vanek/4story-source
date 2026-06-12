#include "soci_stat_chart.h"

#include "db/queries.h"
#include "db/row_helpers.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <cstdint>

namespace tmapsvr {

namespace {

// Load a class/race base-stat table (same column shape) into `dst`.
void LoadStatBlocks(soci::session& sql, const char* query,
                    std::unordered_map<std::uint8_t, StatBlock>& dst)
{
    std::int32_t id = 0, str = 0, dex = 0, con = 0, intl = 0, wis = 0, men = 0;
    soci::statement st = (sql.prepare << query,
        soci::into(id),  soci::into(str), soci::into(dex), soci::into(con),
        soci::into(intl), soci::into(wis), soci::into(men));
    st.execute(false);
    while (st.fetch())
    {
        StatBlock s;
        s.wSTR = db::Narrow16(str);
        s.wDEX = db::Narrow16(dex);
        s.wCON = db::Narrow16(con);
        s.wINT = db::Narrow16(intl);
        s.wWIS = db::Narrow16(wis);
        s.wMEN = db::Narrow16(men);
        dst[db::Narrow8(id)] = s;
    }
}

} // namespace

SociStatChart::SociStatChart(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();
    auto& sql  = *lease;

    // 1) TFORMULACHART — bID keys the formula directly (legacy keys its
    //    map on the raw bID, which equals the FTYPE_ ordinal).
    {
        std::int32_t bid = 0, init = 0;
        double ratex = 0.0, ratey = 0.0;   // float columns → bind as double
        soci::statement st = (sql.prepare << queries::AllFormula,
            soci::into(bid), soci::into(init), soci::into(ratex),
            soci::into(ratey));
        st.execute(false);
        while (st.fetch())
        {
            FormulaRow r;
            r.dwInit = static_cast<std::uint32_t>(init);
            r.fRateX = db::NarrowF(ratex);
            r.fRateY = db::NarrowF(ratey);
            m_formula[db::Narrow8(bid)] = r;
        }
    }

    // 2) TCLASSCHART + 3) TRACECHART — identical column shape.
    LoadStatBlocks(sql, queries::AllClassStats, m_class);
    LoadStatBlocks(sql, queries::AllRaceStats,  m_race);

    spdlog::info("soci_stat_chart: loaded {} formula row(s), {} class(es), "
                 "{} race(s) from TFORMULACHART/TCLASSCHART/TRACECHART",
                 m_formula.size(), m_class.size(), m_race.size());
}

} // namespace tmapsvr
