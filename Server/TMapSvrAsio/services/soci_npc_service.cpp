#include "soci_npc_service.h"

#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <string>

namespace tmapsvr {

SociNpcService::SociNpcService(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();
    auto& sql  = *lease;

    std::int32_t row_id = 0, row_type = 0, row_country = 0, row_local = 0,
                 row_condition = 0, row_disc = 0, row_addprob = 0,
                 row_item = 0, row_map = 0;
    double       row_x = 0.0, row_y = 0.0, row_z = 0.0;
    std::string  row_name;
    soci::indicator name_ind = soci::i_null;

    soci::statement st = (sql.prepare <<
        "SELECT wID, szName, bType, bCountryID, wLocalID, bCondition, "
        "  bDiscountRate, bAddProb, wItemID, wMapID, fPosX, fPosY, fPosZ "
        "FROM TNPCCHART",
        soci::into(row_id),
        soci::into(row_name, name_ind),
        soci::into(row_type),
        soci::into(row_country),
        soci::into(row_local),
        soci::into(row_condition),
        soci::into(row_disc),
        soci::into(row_addprob),
        soci::into(row_item),
        soci::into(row_map),
        soci::into(row_x),
        soci::into(row_y),
        soci::into(row_z));
    st.execute(false);

    while (st.fetch())
    {
        NpcRow r;
        r.wID           = static_cast<std::uint16_t>(row_id);
        r.szName        = (name_ind == soci::i_ok) ? row_name : std::string{};
        r.bType         = static_cast<std::uint8_t> (row_type);
        r.bCountryID    = static_cast<std::uint8_t> (row_country);
        r.wLocalID      = static_cast<std::uint16_t>(row_local);
        r.bCondition    = static_cast<std::uint8_t> (row_condition);
        r.bDiscountRate = static_cast<std::uint8_t> (row_disc);
        r.bAddProb      = static_cast<std::uint8_t> (row_addprob);
        r.wItemID       = static_cast<std::uint16_t>(row_item);
        r.wMapID        = static_cast<std::uint16_t>(row_map);
        r.fPosX         = static_cast<float>(row_x);
        r.fPosY         = static_cast<float>(row_y);
        r.fPosZ         = static_cast<float>(row_z);
        m_rows[r.wID] = std::move(r);
    }

    spdlog::info("soci_npc_service: loaded {} NPC row(s) from TNPCCHART",
        m_rows.size());
}

std::optional<NpcRow>
SociNpcService::FindNpc(std::uint16_t npc_id) const
{
    const auto it = m_rows.find(npc_id);
    if (it == m_rows.end()) return std::nullopt;
    return it->second;
}

} // namespace tmapsvr
