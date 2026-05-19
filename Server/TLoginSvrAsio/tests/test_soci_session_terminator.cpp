// Integration test for SociSessionTerminator against a live PostgreSQL.
// Validates the two-step cleanup (TCURRENTUSER delete + TLOG.timeLOGOUT
// stamp) and the MapHandoff exception path (TCURRENTUSER stays alive).

#include "fourstory/db/session_pool.h"
#include "../services/soci_session_terminator.h"

#include <soci/soci.h>

#if defined(_WIN32)
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

struct Seeded
{
    int user_id;
    int session_key;
};

// INSERT TCURRENTUSER row with the full NOT-NULL column set the
// legacy MSSQL schema requires (dwCharID/bGroupID/bChannel/wPort/
// bLocked are NOT NULL with no default). PG dev fixture has
// DEFAULT 0 on those columns, so the legacy short shape also
// worked there. The function returns the auto-generated dwKEY
// (MSSQL: OUTPUT INSERTED, PG: RETURNING).
int InsertCurrentUser(soci::session& sql,
                      fourstory::db::Backend backend,
                      int user_id,
                      int char_id = 0,
                      int group_id = 0,
                      int channel = 0)
{
    const bool is_mssql = (backend == fourstory::db::Backend::Odbc);
    int session_key = 0;
    if (is_mssql)
    {
        sql << "INSERT INTO \"TCURRENTUSER\" "
               "(\"dwUserID\", \"dwCharID\", \"bGroupID\", \"bChannel\", "
               " \"wPort\", \"bLocked\", \"szLoginIP\") "
               "OUTPUT INSERTED.\"dwKEY\" "
               "VALUES (:u, :c, :g, :ch, 0, 0, '127.0.0.1')",
            soci::use(user_id), soci::use(char_id),
            soci::use(group_id), soci::use(channel),
            soci::into(session_key);
    }
    else
    {
        sql << "INSERT INTO \"TCURRENTUSER\" "
               "(\"dwUserID\", \"dwCharID\", \"bGroupID\", \"bChannel\", "
               " \"wPort\", \"bLocked\", \"szLoginIP\") "
               "VALUES (:u, :c, :g, :ch, 0, 0, '127.0.0.1') "
               "RETURNING \"dwKEY\"",
            soci::use(user_id), soci::use(char_id),
            soci::use(group_id), soci::use(channel),
            soci::into(session_key);
    }
    return session_key;
}

// INSERT TLOG row with the full NOT-NULL column set (dwCharID,
// bGroupID, bChannel, timeLOGIN, timeLOGOUT). timeLOGOUT explicitly
// set to 1970-01-02 so the test can later assert "Terminate bumped
// timeLOGOUT to now".
void InsertLog(soci::session& sql, int session_key, int user_id)
{
    sql << "INSERT INTO \"TLOG\" "
           "(\"dwKEY\", \"dwUserID\", \"dwCharID\", "
           " \"bGroupID\", \"bChannel\", "
           " \"timeLOGIN\", \"timeLOGOUT\") "
           "VALUES (:k, :u, 0, 0, 0, "
           "        CURRENT_TIMESTAMP, '1970-01-02 00:00:00')",
        soci::use(session_key), soci::use(user_id);
}

// Insert one TCURRENTUSER row + one TLOG row and return the assigned
// dwKEY / dwUserID. Returns the values for the test to assert against.
Seeded SeedSession(soci::session& sql,
                   fourstory::db::Backend backend,
                   int user_id)
{
    const int session_key = InsertCurrentUser(sql, backend, user_id);
    InsertLog(sql, session_key, user_id);
    return Seeded{ user_id, session_key };
}

void RunTests(fourstory::db::Backend backend, const std::string& conn)
{
    fourstory::db::SessionPool pool(backend, conn, /*pool_size=*/2);

    // PID-scoped user IDs — split per-backend tag too so PG and MSSQL
    // runs against the same physical host don't collide.
    const int backend_tag = (backend == fourstory::db::Backend::Odbc) ? 500000 : 0;
    const int base_uid = 3000000 + backend_tag + (::getpid() % 1000);

    // Cleanup any leftover state for our range.
    {
        auto lease = pool.Acquire();
        soci::session& sql = *lease;
        sql << "DELETE FROM \"TLOG\" WHERE \"dwUserID\" BETWEEN :a AND :b",
            soci::use(base_uid), soci::use(base_uid + 99);
        sql << "DELETE FROM \"TCURRENTUSER\" WHERE \"dwUserID\" BETWEEN :a AND :b",
            soci::use(base_uid), soci::use(base_uid + 99);
    }

    tloginsvr::services::SociSessionTerminator svc(pool);
    using tloginsvr::services::TerminationReason;

    // 1. No-op for never-authenticated session.
    {
        svc.Terminate(0, 0, TerminationReason::Disconnect);
        Check(true, "Terminate(0,0,Disconnect) → no-op (no exception)");
    }

    // 2. Disconnect path: deletes TCURRENTUSER, stamps TLOG.
    {
        Seeded s{};
        {
            auto lease = pool.Acquire();
            s = SeedSession(*lease, backend, base_uid + 1);
        }
        svc.Terminate(s.user_id, s.session_key, TerminationReason::Disconnect);

        auto lease = pool.Acquire();
        int row_hits = 0;
        *lease << "SELECT COUNT(*) FROM \"TCURRENTUSER\" WHERE \"dwKEY\" = :k",
            soci::use(s.session_key), soci::into(row_hits);
        Check(row_hits == 0, "Disconnect → TCURRENTUSER row deleted");

        int recent_hits = 0;
        *lease << "SELECT COUNT(*) FROM \"TLOG\" "
                  "WHERE \"dwKEY\" = :k "
                  "  AND \"timeLOGOUT\" > '1970-01-02 00:00:00'",
            soci::use(s.session_key), soci::into(recent_hits);
        Check(recent_hits == 1, "Disconnect → TLOG.timeLOGOUT stamped to now");
    }

    // 3. ClientRequest path: same behavior as Disconnect.
    {
        Seeded s{};
        {
            auto lease = pool.Acquire();
            s = SeedSession(*lease, backend, base_uid + 2);
        }
        svc.Terminate(s.user_id, s.session_key, TerminationReason::ClientRequest);

        auto lease = pool.Acquire();
        int row_hits = 0;
        *lease << "SELECT COUNT(*) FROM \"TCURRENTUSER\" WHERE \"dwKEY\" = :k",
            soci::use(s.session_key), soci::into(row_hits);
        Check(row_hits == 0, "ClientRequest → TCURRENTUSER row deleted");
    }

    // 4. MapHandoff: TCURRENTUSER stays, TLOG still stamped.
    {
        Seeded s{};
        {
            auto lease = pool.Acquire();
            s = SeedSession(*lease, backend, base_uid + 3);
        }
        svc.Terminate(s.user_id, s.session_key, TerminationReason::MapHandoff);

        auto lease = pool.Acquire();
        int row_hits = 0;
        *lease << "SELECT COUNT(*) FROM \"TCURRENTUSER\" WHERE \"dwKEY\" = :k",
            soci::use(s.session_key), soci::into(row_hits);
        Check(row_hits == 1, "MapHandoff → TCURRENTUSER row preserved");

        int recent_hits = 0;
        *lease << "SELECT COUNT(*) FROM \"TLOG\" "
                  "WHERE \"dwKEY\" = :k "
                  "  AND \"timeLOGOUT\" > '1970-01-02 00:00:00'",
            soci::use(s.session_key), soci::into(recent_hits);
        Check(recent_hits == 1, "MapHandoff → TLOG.timeLOGOUT stamped to now");
    }

    // 5. Terminate with user_id but no session_key (still cleans the row).
    {
        Seeded s{};
        {
            auto lease = pool.Acquire();
            s = SeedSession(*lease, backend, base_uid + 4);
        }
        // Note: passing session_key=0 means "no audit row to stamp",
        // but TCURRENTUSER cleanup still runs based on user_id alone.
        svc.Terminate(s.user_id, 0, TerminationReason::Disconnect);

        auto lease = pool.Acquire();
        int row_hits = 0;
        *lease << "SELECT COUNT(*) FROM \"TCURRENTUSER\" WHERE \"dwUserID\" = :u",
            soci::use(s.user_id), soci::into(row_hits);
        Check(row_hits == 0,
            "Terminate(uid, 0, Disconnect) → TCURRENTUSER cleared by uid");
    }

    // 5b. Group + channel stamping on TLOG. Legacy TLogout SP reads
    //     bGroupID + bChannel from TCURRENTUSER and copies them onto
    //     the matching TLOG row (TLogout.sql:46-51). Modern matches:
    //     seed a session with group=5, channel=3, char=42; on
    //     Terminate the TLOG row should carry those.
    {
        const int uid_gc = base_uid + 5;
        const int char_gc = 42;
        const int group_gc = 5;
        const int channel_gc = 3;
        int session_key = 0;
        {
            auto lease = pool.Acquire();
            session_key = InsertCurrentUser(
                *lease, backend, uid_gc, char_gc, group_gc, channel_gc);
            InsertLog(*lease, session_key, uid_gc);
        }
        svc.Terminate(uid_gc, session_key,
            TerminationReason::Disconnect, char_gc);

        auto lease = pool.Acquire();
        int tlog_group = -1;
        int tlog_channel = -1;
        int tlog_char = -1;
        *lease << "SELECT \"bGroupID\", \"bChannel\", \"dwCharID\" "
                  "FROM \"TLOG\" WHERE \"dwKEY\" = :k",
            soci::use(session_key),
            soci::into(tlog_group),
            soci::into(tlog_channel),
            soci::into(tlog_char);
        Check(tlog_group == group_gc,
            "Terminate → TLOG.bGroupID copied from TCURRENTUSER");
        Check(tlog_channel == channel_gc,
            "Terminate → TLOG.bChannel copied from TCURRENTUSER");
        Check(tlog_char == char_gc,
            "Terminate → TLOG.dwCharID set from arg");
    }

    // 5c. Same stamping path but char_id = 0 (pre-handoff disconnect).
    //     Group/channel come from TCURRENTUSER; dwCharID stays at the
    //     row's existing value (since the UPDATE skips it when char=0).
    {
        const int uid_gc = base_uid + 6;
        const int group_gc = 7;
        const int channel_gc = 2;
        int session_key = 0;
        {
            auto lease = pool.Acquire();
            session_key = InsertCurrentUser(
                *lease, backend, uid_gc,
                /*char_id=*/0, group_gc, channel_gc);
            InsertLog(*lease, session_key, uid_gc);
        }
        svc.Terminate(uid_gc, session_key,
            TerminationReason::Disconnect, /*char_id=*/0);

        auto lease = pool.Acquire();
        int tlog_group = -1;
        int tlog_channel = -1;
        *lease << "SELECT \"bGroupID\", \"bChannel\" "
                  "FROM \"TLOG\" WHERE \"dwKEY\" = :k",
            soci::use(session_key),
            soci::into(tlog_group),
            soci::into(tlog_channel);
        Check(tlog_group == group_gc,
            "Terminate(char=0) → TLOG.bGroupID still stamped");
        Check(tlog_channel == channel_gc,
            "Terminate(char=0) → TLOG.bChannel still stamped");
    }

    // 6. ClearStaleSessions — startup-recovery sweep. Seeds 3 pre-
    //    handoff rows (dwCharID = 0) plus 1 in-game row
    //    (dwCharID != 0) that simulates a user already handed off to
    //    a map server. Asserts only the pre-handoff rows are wiped;
    //    the in-game row must survive so global session state stays
    //    in sync with the live world-server state.
    //    Mirrors legacy CSPClearLoginUser / TClearLoginCurrentUser SP
    //    (`DELETE TCURRENTUSER WHERE dwCharID = 0`).
    {
        // Seed (don't bother SeedSession's TLOG insert — ClearStale
        // only touches TCURRENTUSER, and the cleanup at function end
        // deletes any uid range we touched).
        {
            auto lease = pool.Acquire();
            (void)InsertCurrentUser(*lease, backend, base_uid + 10);
            (void)InsertCurrentUser(*lease, backend, base_uid + 11);
            (void)InsertCurrentUser(*lease, backend, base_uid + 12);
            // In-game session: dwCharID != 0. Must survive sweep.
            (void)InsertCurrentUser(*lease, backend, base_uid + 13,
                /*char_id=*/99001);
        }

        // Count pre-handoff rows (the ones the sweep should claim).
        // Anything from other tests sharing the DB that's also
        // dwCharID=0 will also get wiped — that's fine, ClearStale
        // is a startup-only call and the test fixture rebuilds state.
        int prehandoff_before = 0;
        int handoff_before    = 0;
        {
            auto lease = pool.Acquire();
            *lease << "SELECT COUNT(*) FROM \"TCURRENTUSER\" "
                      "WHERE \"dwCharID\" = 0",
                soci::into(prehandoff_before);
            *lease << "SELECT COUNT(*) FROM \"TCURRENTUSER\" "
                      "WHERE \"dwCharID\" <> 0",
                soci::into(handoff_before);
        }
        Check(prehandoff_before >= 3,
            "ClearStaleSessions: seeded pre-handoff rows visible");
        Check(handoff_before >= 1,
            "ClearStaleSessions: seeded in-game row visible");

        const int cleared = svc.ClearStaleSessions();
        Check(cleared == prehandoff_before,
            "ClearStaleSessions: cleared count == pre-handoff rows");

        int prehandoff_after = 0;
        int handoff_after    = 0;
        {
            auto lease = pool.Acquire();
            *lease << "SELECT COUNT(*) FROM \"TCURRENTUSER\" "
                      "WHERE \"dwCharID\" = 0",
                soci::into(prehandoff_after);
            *lease << "SELECT COUNT(*) FROM \"TCURRENTUSER\" "
                      "WHERE \"dwCharID\" <> 0",
                soci::into(handoff_after);
        }
        Check(prehandoff_after == 0,
            "ClearStaleSessions: no pre-handoff rows left after sweep");
        Check(handoff_after == handoff_before,
            "ClearStaleSessions: in-game rows (dwCharID != 0) preserved");

        // Re-calling on a table with no pre-handoff rows returns 0.
        Check(svc.ClearStaleSessions() == 0,
            "ClearStaleSessions: idempotent when no pre-handoff rows remain");
    }

    // Cleanup.
    try
    {
        auto lease = pool.Acquire();
        soci::session& sql = *lease;
        sql << "DELETE FROM \"TLOG\" WHERE \"dwUserID\" BETWEEN :a AND :b",
            soci::use(base_uid), soci::use(base_uid + 99);
        sql << "DELETE FROM \"TCURRENTUSER\" WHERE \"dwUserID\" BETWEEN :a AND :b",
            soci::use(base_uid), soci::use(base_uid + 99);
    }
    catch (const std::exception& ex)
    {
        std::printf("  WARN  cleanup error (non-fatal): %s\n", ex.what());
    }
}

} // namespace

int main()
{
    std::printf("=== tloginsvr_asio SOCI session-terminator test ===\n");

    const char* pg_conn    = std::getenv("TLOGINSVR_TEST_PG_CONN");
    const char* mssql_conn = std::getenv("TLOGINSVR_TEST_MSSQL_CONN");

    if ((pg_conn    == nullptr || pg_conn[0]    == '\0') &&
        (mssql_conn == nullptr || mssql_conn[0] == '\0'))
    {
        std::printf("  SKIP  neither TLOGINSVR_TEST_PG_CONN nor "
                    "TLOGINSVR_TEST_MSSQL_CONN set\n");
        std::printf("\nResults: 0 passed, 0 failed (skipped)\n");
        return 0;
    }

    if (pg_conn != nullptr && pg_conn[0] != '\0')
    {
        std::printf("\n--- backend: postgresql ---\n");
        try
        {
            RunTests(fourstory::db::Backend::PostgreSQL, pg_conn);
        }
        catch (const std::exception& ex)
        {
            std::printf("  FAIL  pg exception: %s\n", ex.what());
            ++g_failed;
        }
    }

    if (mssql_conn != nullptr && mssql_conn[0] != '\0')
    {
        std::printf("\n--- backend: odbc (MSSQL) ---\n");
        try
        {
            RunTests(fourstory::db::Backend::Odbc, mssql_conn);
        }
        catch (const std::exception& ex)
        {
            std::printf("  FAIL  mssql exception: %s\n", ex.what());
            ++g_failed;
        }
    }

    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
