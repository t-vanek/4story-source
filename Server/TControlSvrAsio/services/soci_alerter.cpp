#include "soci_alerter.h"

#include "fourstory/db/orm/sp_call.h"

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
