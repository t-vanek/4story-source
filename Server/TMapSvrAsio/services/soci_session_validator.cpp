#include "soci_session_validator.h"

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

        sql << "SELECT dwUserID, dwKEY, bGroupID, bChannel, szLoginIP, bLocked "
               "FROM TCURRENTUSER WHERE dwUserID = :uid AND dwKEY = :key",
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
        info.dwUserID  = static_cast<std::uint32_t>(row_user_id);
        info.dwKEY     = static_cast<std::uint32_t>(row_key);
        info.bGroupID  = static_cast<std::uint8_t>(row_group);
        info.bChannel  = static_cast<std::uint8_t>(row_channel);
        info.szLoginIP = (ind == soci::i_ok) ? row_login_ip : std::string{};
        info.bLocked   = row_locked != 0;
        return info;
    }
    catch (const std::exception& ex)
    {
        // SOCI throws on driver / connection trouble. Treat as
        // "lookup failed" so the handler refuses the session rather
        // than crashing the io_context. The boot-time schema check
        // already proved the columns exist; runtime failures here
        // are typically transient (network blip, pool exhaustion).
        spdlog::error("soci_session_validator: lookup uid={} threw: {}",
            user_id, ex.what());
        return std::nullopt;
    }
}

} // namespace tmapsvr
