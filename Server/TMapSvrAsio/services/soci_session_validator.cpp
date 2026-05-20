#include "soci_session_validator.h"

#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <cstdint>

namespace tmapsvr {

SociMapSessionValidator::SociMapSessionValidator(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

bool SociMapSessionValidator::Validate(const MapSessionLookup& lookup)
{
    // Legacy CSHandler.cpp:305-313 (`CSPCheckMapChar`):
    //   IF EXISTS(SELECT 1 FROM TCURRENTUSER
    //             WHERE dwUserID = @user AND dwCharID = @char
    //               AND dwKEY  = @key)  RETURN 0;   -- success
    //   ELSE                                        RETURN 1;   -- fail
    //
    // Inlined verbatim. The result column count is the cheap form —
    // we don't care about any of the row's other columns, only that
    // the (uid, cid, key) triple is present.
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    const int    uid = static_cast<int>(lookup.user_id);
    const int    cid = static_cast<int>(lookup.char_id);
    const long long key64 = static_cast<long long>(lookup.dw_key);

    int hits = 0;
    try
    {
        soci::statement st = (sql.prepare <<
            "SELECT COUNT(*) FROM \"TCURRENTUSER\" "
            "WHERE \"dwUserID\" = :u "
            "  AND \"dwCharID\" = :c "
            "  AND \"dwKEY\"    = :k",
            soci::use(uid),
            soci::use(cid),
            soci::use(key64),
            soci::into(hits));
        st.execute(true);
    }
    catch (const std::exception& ex)
    {
        // Treat as deny — legacy CSHandler.cpp:316 (`bRet = TRUE`)
        // does the same on `query->Call()` failure. Logging at warn
        // surfaces real DB outages without spamming on every probe
        // (the row-missing case below logs at info).
        spdlog::warn("session_validator: TCURRENTUSER probe uid={} char={} "
                     "DB error: {} — denying", uid, cid, ex.what());
        return false;
    }
    const bool ok = hits > 0;
    if (!ok)
    {
        spdlog::info("session_validator: no TCURRENTUSER row uid={} char={} "
                     "key=0x{:08x} — denying", uid, cid,
                     static_cast<std::uint32_t>(lookup.dw_key));
    }
    return ok;
}

} // namespace tmapsvr
