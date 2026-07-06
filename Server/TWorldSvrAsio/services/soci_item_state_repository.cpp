#include "services/soci_item_state_repository.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <exception>

namespace tworldsvr {

bool SociItemStateRepository::ChangeState(std::uint16_t item_id,
                                          std::uint8_t  init_state)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        const int id    = static_cast<int>(item_id);
        const int state = static_cast<int>(init_state);
        soci::statement st = (sql.prepare <<
            "UPDATE \"TITEMCHART\" SET \"bInitState\" = :s "
            "WHERE \"wItemID\" = :i",
            soci::use(state), soci::use(id));
        st.execute(true);
        // 0 rows touched == the legacy SP's "no such wItemID" RETURN 1.
        return st.get_affected_rows() > 0;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociItemStateRepository::ChangeState({},{}) "
                      "failed: {}", item_id, init_state, ex.what());
        return false;
    }
}

std::vector<ItemFindRow>
SociItemStateRepository::FindItems(std::uint16_t item_id,
                                   const std::string& name_pattern)
{
    std::vector<ItemFindRow> out;
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        const int id = static_cast<int>(item_id);
        soci::rowset<soci::row> rs =
            (sql.prepare <<
                "SELECT \"wItemID\", \"bInitState\", \"szName\" "
                "FROM \"TITEMCHART\" "
                "WHERE \"szName\" LIKE :n OR \"wItemID\" = :i",
             soci::use(name_pattern), soci::use(id));
        for (const auto& row : rs)
        {
            ItemFindRow r{};
            r.item_id    = static_cast<std::uint16_t>(row.get<int>(0));
            r.init_state = static_cast<std::uint8_t>(row.get<int>(1));
            r.name       = row.get<std::string>(2);
            out.push_back(std::move(r));
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SociItemStateRepository::FindItems({},'{}') "
                      "failed: {}", item_id, name_pattern, ex.what());
        out.clear();
    }
    return out;
}

} // namespace tworldsvr
