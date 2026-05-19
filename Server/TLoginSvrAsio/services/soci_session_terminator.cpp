#include "soci_session_terminator.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>

#include <spdlog/spdlog.h>

namespace tloginsvr::services {

SociSessionTerminator::SociSessionTerminator(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

void SociSessionTerminator::Terminate(std::int32_t user_id,
                                      std::uint32_t session_key,
                                      TerminationReason reason)
{
    if (user_id == 0 && session_key == 0)
    {
        // Never-authenticated session — no row exists. Matches the
        // in-memory impl's contract.
        return;
    }

    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    try
    {
        // Stamp logout time on the audit row regardless of reason — ops
        // wants to see the session-end timestamp even on Map handoffs
        // (it's the boundary between "Login holds the session" and
        // "Map holds the session").
        if (session_key != 0)
        {
            sql << "UPDATE \"TLOG\" SET \"timeLOGOUT\" = CURRENT_TIMESTAMP "
                   "WHERE \"dwKEY\" = :k",
                soci::use(static_cast<int>(session_key));
        }

        // Only delete the TCURRENTUSER row on real disconnects. The
        // MapHandoff path leaves it so Map can validate dwKEY on the
        // incoming connection.
        if (reason != TerminationReason::MapHandoff && user_id != 0)
        {
            sql << "DELETE FROM \"TCURRENTUSER\" WHERE \"dwUserID\" = :u",
                soci::use(user_id);
        }

        spdlog::debug("session.Terminate uid={} key={} reason={}",
            user_id, session_key, static_cast<int>(reason));
    }
    catch (const std::exception& ex)
    {
        // Best-effort: the connection's already gone at this point;
        // log + swallow so we don't crash the cleanup chain. The next
        // login attempt for this user will encounter LR_DUPLICATE and
        // trigger the stale-row cleanup path in SociAuthService.
        spdlog::error("session.Terminate uid={} key={} DB error: {}",
            user_id, session_key, ex.what());
    }
}

int SociSessionTerminator::ClearStaleSessions()
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    try
    {
        int before = 0;
        sql << "SELECT COUNT(*) FROM \"TCURRENTUSER\"", soci::into(before);
        if (before == 0) return 0;

        sql << "DELETE FROM \"TCURRENTUSER\"";
        spdlog::info("session.ClearStaleSessions wiped {} stale TCURRENTUSER row(s)",
            before);
        return before;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("session.ClearStaleSessions DB error: {}", ex.what());
        return -1;
    }
}

} // namespace tloginsvr::services
