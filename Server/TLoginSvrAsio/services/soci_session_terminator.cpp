// SociSessionTerminator implementation — SOCI-backed
// ISessionTerminator. Runs the per-session DB cleanup on disconnect.
//
// Terminate() executes the legacy TLogout SP body inline as two SOCI
// statements:
//   * DELETE FROM TCURRENTUSER WHERE dwUserID = :uid
//   * UPDATE TLOG SET timeLOGOUT = CURRENT_TIMESTAMP,
//                     dwCharID, bGroupID, bChannel = :…
//     WHERE dwKEY = :session_key
//
// MapHandoff branch: when reason == MapHandoff the TCURRENTUSER row is
// preserved (Map server validates the handoff dwKEY); only TLOG is
// stamped. See README's row #1 / #9 — the modes are wire-faithful to
// CSHandler.cpp:1428's handoff path.
//
// ClearStaleSessions() is the boot-time crash-recovery sweep — wipes
// TCURRENTUSER rows whose Login process is no longer alive, so a
// previous PID's session entries don't lock accounts into LR_DUPLICATE.
//
// Legacy parity: Server/TLoginSvr's TLogout SP +
// CTLoginSvrModule::OnEnter's CSPClearLoginUser call.

#include "soci_session_terminator.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>

#include <spdlog/spdlog.h>

namespace tloginsvr::services {

SociSessionTerminator::SociSessionTerminator(fourstory::db::SessionPool& pool)
    : m_pool(pool)
{
}

void SociSessionTerminator::Terminate(std::int32_t  user_id,
                                      std::uint32_t session_key,
                                      TerminationReason reason,
                                      std::int32_t  char_id)
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
        //
        // Legacy TLogout SP (TLogout.sql:38-51) reads dwCharID, bGroupID,
        // bChannel from TCURRENTUSER and copies them onto the matching
        // TLOG row. The login server never set them at INSERT time
        // (login happens before group/channel are known), so without
        // this copy the TLOG audit row stays at the schema defaults (0)
        // and BI queries filtered by group/channel return nothing.
        //
        // Read them inline. If the TCURRENTUSER row is already gone
        // (race with another Terminate, or early auth-failure cleanup)
        // we fall back to zeros — same outcome as legacy's
        // @@ROWCOUNT = 0 branch (RETURN 1 without writing TLOG).
        int session_group   = 0;
        int session_channel = 0;
        if (user_id != 0)
        {
            try
            {
                soci::statement st = (sql.prepare <<
                    "SELECT \"bGroupID\", \"bChannel\" FROM \"TCURRENTUSER\" "
                    "WHERE \"dwUserID\" = :u",
                    soci::use(user_id),
                    soci::into(session_group),
                    soci::into(session_channel));
                st.execute(true);
                // got_data() == false → leave group/channel at 0.
            }
            catch (const std::exception& ex)
            {
                spdlog::debug("session.Terminate TCURRENTUSER group/channel "
                              "lookup skipped: {}", ex.what());
            }
        }

        // If char_id is non-zero, also stamp TLOG.dwCharID — the
        // session was attached to that char at handoff time (set by
        // OnStartReq's MarkHandoffWithChar). Legacy CSPLogout's
        // m_dwCharID arg lands on the same row column.
        if (session_key != 0)
        {
            if (char_id != 0)
            {
                sql << "UPDATE \"TLOG\" SET "
                       "  \"timeLOGOUT\" = CURRENT_TIMESTAMP, "
                       "  \"dwCharID\"   = :c, "
                       "  \"bGroupID\"   = :g, "
                       "  \"bChannel\"   = :ch "
                       "WHERE \"dwKEY\" = :k",
                    soci::use(char_id),
                    soci::use(session_group),
                    soci::use(session_channel),
                    soci::use(static_cast<int>(session_key));
            }
            else
            {
                sql << "UPDATE \"TLOG\" SET "
                       "  \"timeLOGOUT\" = CURRENT_TIMESTAMP, "
                       "  \"bGroupID\"   = :g, "
                       "  \"bChannel\"   = :ch "
                       "WHERE \"dwKEY\" = :k",
                    soci::use(session_group),
                    soci::use(session_channel),
                    soci::use(static_cast<int>(session_key));
            }
        }

        // Stamp TUSERINFOTABLE.dwLastCharID so the next login's
        // CS_LOGIN_ACK.dwCharID pre-highlights the right slot.
        // Best-effort — if the column doesn't exist on the deployed
        // schema we silently skip (modern fallback path returns 0
        // and lobby UX degrades gracefully).
        if (char_id != 0 && user_id != 0)
        {
            try
            {
                sql << "UPDATE \"TUSERINFOTABLE\" SET \"dwLastCharID\" = :c "
                       "WHERE \"dwUserID\" = :u",
                    soci::use(char_id),
                    soci::use(user_id);
            }
            catch (const std::exception& ex)
            {
                spdlog::debug("session.Terminate TUSERINFOTABLE.dwLastCharID "
                              "skipped: {}", ex.what());
            }
        }

        // Only delete the TCURRENTUSER row on real disconnects. The
        // MapHandoff path leaves it so Map can validate dwKEY on the
        // incoming connection.
        if (reason != TerminationReason::MapHandoff && user_id != 0)
        {
            sql << "DELETE FROM \"TCURRENTUSER\" WHERE \"dwUserID\" = :u",
                soci::use(user_id);
        }

        spdlog::debug("session.Terminate uid={} key={} char={} reason={}",
            user_id, session_key, char_id, static_cast<int>(reason));
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
        // Mirror legacy CSPClearLoginUser (`TClearLoginCurrentUser` SP):
        //   DELETE TCURRENTUSER WHERE dwCharID = 0
        // Only wipe sessions that never made it past character select.
        // Rows with dwCharID != 0 represent users already handed off to
        // a map server; wiping them desynchronises global session state
        // from the live world-server in-memory state and breaks
        // cross-instance duplicate-kick (LR_DUPLICATE) on the next login.
        int before = 0;
        sql << "SELECT COUNT(*) FROM \"TCURRENTUSER\" WHERE \"dwCharID\" = 0",
            soci::into(before);
        if (before == 0) return 0;

        sql << "DELETE FROM \"TCURRENTUSER\" WHERE \"dwCharID\" = 0";
        spdlog::info("session.ClearStaleSessions wiped {} pre-handoff "
            "TCURRENTUSER row(s)", before);
        return before;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("session.ClearStaleSessions DB error: {}", ex.what());
        return -1;
    }
}

} // namespace tloginsvr::services
