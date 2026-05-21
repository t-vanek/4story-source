#include "soci_operator_auth_service.h"

#include "fourstory/db/orm/sp_call.h"

#include <spdlog/spdlog.h>

namespace tcontrolsvr {

SociOperatorAuthService::SociOperatorAuthService(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

OperatorAuthResult
SociOperatorAuthService::Authenticate(const std::string& user_id,
                                      const std::string& password)
{
    using fourstory::db::orm::SpCall;

    OperatorAuthResult res{};
    try
    {
        // Legacy contract: TOPLogin's EXEC return code is the operator's
        // bAuthority value (0 = reject, 1 = MANAGER_ALL, 2 = SERVICE, …).
        // We capture it via WithReturn() — no OUT parameters needed.
        auto lease = m_pool.Acquire();
        auto r = SpCall("TOPLogin")
            .In("szUserID", user_id)
            .In("szPasswd", password)
            .WithReturn()
            .Execute(*lease);

        if (!r.Ok())
            return res;

        const auto authority = r.ReturnCode();
        if (authority < 0 || authority > 255)
        {
            spdlog::warn("soci_operator_auth: TOPLogin returned out-of-range "
                         "authority={} for id='{}'", authority, user_id);
            return res;
        }
        if (authority == 0)
            return res;  // legacy contract: 0 = reject

        res.ok        = true;
        res.authority = static_cast<std::uint8_t>(authority);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_operator_auth: TOPLogin('{}') DB error: {}",
            user_id, ex.what());
    }
    return res;
}

} // namespace tcontrolsvr
