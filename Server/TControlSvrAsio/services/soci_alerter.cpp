#include "soci_alerter.h"

#include "fourstory/db/orm/sp_call.h"
#include "native_control.h"

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
    using fourstory::db::orm::SpCall;
    if(native_control::Enabled(m_pool)) {
        // Backup dbo.OPTool_SMSEmergency has its entire SMS implementation
        // commented out. Preserve the intentional no-op; no external SMS call.
        spdlog::info("peer status type={} id={:08x} status={} (source SMS procedure is a no-op)",
            svr_type,svr_id,svr_status);
        return;
    }
    try
    {
        auto lease = m_pool.Acquire();
        SpCall("OPTool_SMSEmergency")
            .In("bSvrType",   static_cast<int>(svr_type))
            .In("dwSvrID",    static_cast<int>(svr_id))
            .In("bSvrStatus", static_cast<int>(svr_status))
            .Execute(*lease);
        spdlog::info("alerter: fired OPTool_SMSEmergency type={} "
                     "id={:08x} status={}", svr_type, svr_id, svr_status);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("alerter: OPTool_SMSEmergency failed: {}", ex.what());
    }
}

} // namespace tcontrolsvr
