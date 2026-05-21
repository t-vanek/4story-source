#include "soci_skill_service.h"

#include "db/queries.h"
#include "db/row_helpers.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace tmapsvr {

SociSkillService::SociSkillService(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

std::vector<SkillRow>
SociSkillService::LoadSkills(std::uint32_t char_id)
{
    std::vector<SkillRow> out;
    try
    {
        auto lease = m_pool.Acquire();
        auto& sql  = *lease;

        std::int32_t row_skill_id = 0, row_level = 0, row_remain = 0;
        soci::statement st = (sql.prepare << queries::SkillsByCharId,
            soci::use(static_cast<std::int32_t>(char_id), "cid"),
            soci::into(row_skill_id),
            soci::into(row_level),
            soci::into(row_remain));
        st.execute(false);

        while (st.fetch())
        {
            SkillRow r;
            r.wSkillID     = db::Narrow16(row_skill_id);
            r.bLevel       = db::Narrow8 (row_level);
            r.dwRemainTick = db::Narrow32(row_remain);
            out.push_back(r);
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_skill_service: LoadSkills({}) threw: {} — "
                      "returning empty",
            char_id, ex.what());
        out.clear();
    }
    return out;
}

} // namespace tmapsvr
