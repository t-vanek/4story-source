#include "soci_companion_service.h"

#include "db/queries.h"
#include "db/row_helpers.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <string>

namespace tmapsvr {

SociCompanionService::SociCompanionService(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

std::vector<CompanionRow>
SociCompanionService::LoadCompanions(std::uint32_t char_id)
{
    std::vector<CompanionRow> out;
    try
    {
        auto lease = m_pool.Acquire();
        auto& sql  = *lease;

        std::int32_t row_slot = 0, row_mon = 0, row_level = 0, row_exp = 0,
                     row_life = 0, row_status_points = 0, row_effect = 0,
                     row_str = 0, row_dex = 0, row_con = 0, row_int = 0,
                     row_wis = 0, row_men = 0, row_bonus = 0;
        std::string  row_name;
        soci::indicator name_ind = soci::i_null;

        soci::statement st = (sql.prepare << queries::CompanionsByCharId,
            soci::use(static_cast<std::int32_t>(char_id), "cid"),
            soci::into(row_slot), soci::into(row_mon),
            soci::into(row_level), soci::into(row_name, name_ind),
            soci::into(row_exp), soci::into(row_life),
            soci::into(row_status_points), soci::into(row_effect),
            soci::into(row_str), soci::into(row_dex), soci::into(row_con),
            soci::into(row_int), soci::into(row_wis), soci::into(row_men),
            soci::into(row_bonus));
        st.execute(false);

        while (st.fetch())
        {
            CompanionRow c;
            c.bSlot         = db::Narrow8 (row_slot);
            c.dwMonID       = db::Narrow32(row_mon);
            c.bLevel        = db::Narrow8 (row_level);
            c.strName       = db::SafeString(row_name, name_ind);
            c.dwExp         = db::Narrow32(row_exp);
            c.wLife         = db::Narrow16(row_life);
            c.bStatusPoints = db::Narrow8 (row_status_points);
            c.bEffect       = db::Narrow8 (row_effect);
            c.wSTR          = db::Narrow16(row_str);
            c.wDEX          = db::Narrow16(row_dex);
            c.wCON          = db::Narrow16(row_con);
            c.wINT          = db::Narrow16(row_int);
            c.wWIS          = db::Narrow16(row_wis);
            c.wMEN          = db::Narrow16(row_men);
            c.wBonusID      = db::Narrow16(row_bonus);
            out.push_back(std::move(c));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_companion_service: LoadCompanions({}) threw: {} — "
                      "returning empty",
            char_id, ex.what());
        out.clear();
    }
    return out;
}

} // namespace tmapsvr
