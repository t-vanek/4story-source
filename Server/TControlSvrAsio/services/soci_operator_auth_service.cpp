#include "soci_operator_auth_service.h"

#include <soci/soci.h>
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
    OperatorAuthResult res{};
    try
    {
        auto lease = m_pool.Acquire();
        soci::session& sql = *lease;

        // ODBC call shape mirrors the legacy CSPOPLogin escape
        // sequence: `{ ? = CALL TOPLogin(?, ?) }`. SOCI's prepared
        // statement layer accepts the same string verbatim for
        // ODBC; the leading `?` is the SP return code (= bAuthority
        // for TOPLogin).
        int authority = 0;
        soci::statement st = (sql.prepare <<
            "{ ? = CALL TOPLogin(?, ?) }",
            soci::into(authority),
            soci::use(user_id),
            soci::use(password));
        st.execute(true);

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
