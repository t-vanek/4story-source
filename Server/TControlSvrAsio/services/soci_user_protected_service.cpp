#include "soci_user_protected_service.h"

#include "fourstory/db/orm/sp_call.h"

#include <spdlog/spdlog.h>

namespace tcontrolsvr {

SociUserProtectedService::SociUserProtectedService(
    fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

std::uint8_t
SociUserProtectedService::AddBan(const std::string& user_id,
                                 std::uint32_t duration_days,
                                 const std::string& reason,
                                 std::uint8_t permanent,
                                 const std::string& operator_id)
{
    using fourstory::db::orm::SpCall;
    try
    {
        auto lease = m_pool.Acquire();
        auto r = SpCall("TUserProtectedAdd")
            .In("szUserID",    user_id)
            .In("dwDuration",  static_cast<int>(duration_days))
            .In("szReason",    reason)
            .In("bPermanent",  static_cast<int>(permanent))
            .In("szOperator",  operator_id)
            .WithReturn()
            .Execute(*lease);
        if (!r.Ok()) return 0;
        const auto ret = r.ReturnCode();
        if (ret < 0 || ret > 255) return 0;
        return static_cast<std::uint8_t>(ret);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_user_protected: TUserProtectedAdd('{}') failed: {}",
            user_id, ex.what());
        return 0;
    }
}

} // namespace tcontrolsvr
