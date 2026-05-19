// Integration test for SociSessionTerminator against a live PostgreSQL.
// Validates the two-step cleanup (TCURRENTUSER delete + TLOG.timeLOGOUT
// stamp) and the MapHandoff exception path (TCURRENTUSER stays alive).

#include "fourstory/db/session_pool.h"
#include "../services/soci_session_terminator.h"

#include <soci/soci.h>

#include <unistd.h>

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

// Insert one TCURRENTUSER row + one TLOG row and return the assigned
// dwKEY / dwUserID. Returns the values for the test to assert against.
Seeded SeedSession(soci::session& sql, int user_id)
{
    int session_key = 0;
    sql << "INSERT INTO \"TCURRENTUSER\" (\"dwUserID\", \"szLoginIP\") "
           "VALUES (:u, '127.0.0.1') RETURNING \"dwKEY\"",
        soci::use(user_id), soci::into(session_key);
    // timeLOGOUT defaults to CURRENT_TIMESTAMP at INSERT; for the
    // assertion we want a "pre-Terminate" timestamp that we can compare
    // against, so set it explicitly to 1970-01-02 (definitely older
    // than `now`).
    sql << "INSERT INTO \"TLOG\" (\"dwKEY\", \"dwUserID\", \"timeLOGOUT\") "
           "VALUES (:k, :u, '1970-01-02 00:00:00')",
        soci::use(session_key), soci::use(user_id);
    return Seeded{ user_id, session_key };
}

void RunTests(const std::string& conn)
{
    fourstory::db::SessionPool pool(
        fourstory::db::Backend::PostgreSQL, conn, /*pool_size=*/2);

    // PID-scoped user IDs.
    const int base_uid = 3000000 + (::getpid() % 1000);

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
            s = SeedSession(*lease, base_uid + 1);
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
            s = SeedSession(*lease, base_uid + 2);
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
            s = SeedSession(*lease, base_uid + 3);
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
            s = SeedSession(*lease, base_uid + 4);
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

    // 6. ClearStaleSessions — startup-recovery sweep. Seeds 3 rows
    //    that look like a previous process's crash leftovers, then
    //    calls the bulk clearer and asserts they're all gone. This
    //    is the legacy ClearLoginUser/CSPClearLoginUser equivalent.
    {
        // Seed (don't bother SeedSession's TLOG insert — ClearStale
        // only touches TCURRENTUSER, and the cleanup at function end
        // deletes any uid range we touched).
        {
            auto lease = pool.Acquire();
            soci::session& sql = *lease;
            sql << "INSERT INTO \"TCURRENTUSER\" (\"dwUserID\", \"szLoginIP\") "
                   "VALUES (:u, '127.0.0.1')", soci::use(base_uid + 10);
            sql << "INSERT INTO \"TCURRENTUSER\" (\"dwUserID\", \"szLoginIP\") "
                   "VALUES (:u, '127.0.0.1')", soci::use(base_uid + 11);
            sql << "INSERT INTO \"TCURRENTUSER\" (\"dwUserID\", \"szLoginIP\") "
                   "VALUES (:u, '127.0.0.1')", soci::use(base_uid + 12);
        }

        // ClearStaleSessions truncates EVERY TCURRENTUSER row, not
        // just our PID range. Capture the pre-count so we can assert
        // "at least the three we seeded" got wiped — anything else
        // belongs to other tests sharing the DB and isn't our
        // responsibility to defend against.
        int before = 0;
        {
            auto lease = pool.Acquire();
            *lease << "SELECT COUNT(*) FROM \"TCURRENTUSER\"",
                soci::into(before);
        }
        Check(before >= 3, "ClearStaleSessions: seeded rows visible");

        const int cleared = svc.ClearStaleSessions();
        Check(cleared == before,
            "ClearStaleSessions: return value equals pre-count");

        int after = 0;
        {
            auto lease = pool.Acquire();
            *lease << "SELECT COUNT(*) FROM \"TCURRENTUSER\"",
                soci::into(after);
        }
        Check(after == 0, "ClearStaleSessions: TCURRENTUSER is empty after sweep");

        // Re-calling on an empty table returns 0 immediately.
        Check(svc.ClearStaleSessions() == 0,
            "ClearStaleSessions: idempotent on empty table");
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

    const char* conn = std::getenv("TLOGINSVR_TEST_PG_CONN");
    if (conn == nullptr || conn[0] == '\0')
    {
        std::printf("  SKIP  TLOGINSVR_TEST_PG_CONN not set\n");
        std::printf("\nResults: 0 passed, 0 failed (skipped)\n");
        return 0;
    }

    try
    {
        RunTests(conn);
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception: %s\n", ex.what());
        ++g_failed;
    }

    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
