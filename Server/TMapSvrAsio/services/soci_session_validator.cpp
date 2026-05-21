#include "soci_session_validator.h"

#include "db/queries.h"
#include "db/row_helpers.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <string>

namespace tmapsvr {

SociMapSessionValidator::SociMapSessionValidator(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

std::optional<MapSessionInfo>
SociMapSessionValidator::LookupSession(std::uint32_t user_id, std::uint32_t key)
{
    // Row is keyed by (dwUserID, dwKEY) — TLoginSvrAsio writes both
    // values when it hands off the session to a map server, and only
    // one row per user is current. The columns selected here are the
    // ones tmapsvr::db::ValidateUserSchema requires.
    try
    {
        auto lease = m_pool.Acquire();
        auto& sql  = *lease;

        std::int32_t row_user_id   = 0;
        std::int32_t row_key       = 0;
        std::int32_t row_group     = 0;
        std::int32_t row_channel   = 0;
        std::string  row_login_ip;
        std::int32_t row_locked    = 0;
        soci::indicator ind        = soci::i_null;

        sql << queries::SessionByUserKey,
            soci::use(static_cast<std::int32_t>(user_id), "uid"),
            soci::use(static_cast<std::int32_t>(key),     "key"),
            soci::into(row_user_id),
            soci::into(row_key),
            soci::into(row_group),
            soci::into(row_channel),
            soci::into(row_login_ip, ind),
            soci::into(row_locked);

        if (!sql.got_data())
            return std::nullopt;

        MapSessionInfo info;
        info.dwUserID  = db::Narrow32(row_user_id);
        info.dwKEY     = db::Narrow32(row_key);
        info.bGroupID  = db::Narrow8 (row_group);
        info.bChannel  = db::Narrow8 (row_channel);
        info.szLoginIP = db::SafeString(row_login_ip, ind);
        info.bLocked   = row_locked != 0;
        return info;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("soci_session_validator: lookup uid={} threw: {}",
            user_id, ex.what());
        return std::nullopt;
    }
}

} // namespace tmapsvr
