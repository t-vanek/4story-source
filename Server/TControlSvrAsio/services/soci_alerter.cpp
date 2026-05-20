#include "soci_alerter.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

namespace tcontrolsvr {

SociAlerter::SociAlerter(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

void SociAlerter::Notify(std::uint8_t svr_type,
                         std::uint32_t svr_id,
                         std::uint8_t svr_status)
{
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;
        int t = svr_type;
        int id = static_cast<int>(svr_id);
        int s = svr_status;
        soci::statement st = (sql.prepare <<
            "{ CALL OPTool_SMSEmergency(?, ?, ?) }",
            soci::use(t), soci::use(id), soci::use(s));
        st.execute(true);
        spdlog::info("alerter: fired OPTool_SMSEmergency type={} "
                     "id={:08x} status={}", svr_type, svr_id, svr_status);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("alerter: OPTool_SMSEmergency failed: {}", ex.what());
    }
}

} // namespace tcontrolsvr
