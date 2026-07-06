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

} // namespace tworldsvr
