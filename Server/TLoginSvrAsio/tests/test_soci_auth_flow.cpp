// SOCI-backed auth flow test. Direct test against SociAuthService
// (does not exercise the wire protocol — that's covered by the
// in-memory test_auth_flow.cpp suite, and the two implementations
// share the IAuthService interface, so wire-format behavior is
// already validated upstream of the DB).
//
// Runs against two backends — whichever env vars are set:
//   TLOGINSVR_TEST_PG_CONN    → PostgreSQL via SOCI's native backend
//   TLOGINSVR_TEST_MSSQL_CONN → SQL Server via SOCI's ODBC backend
//                               (FreeTDS or msodbcsql18 driver)
// Both unset → the test exits with 0 passed / 0 failed (skipped).
//
// Local invocation:
//   export TLOGINSVR_TEST_PG_CONN="host=localhost port=5432 \
//       dbname=tloginsvr_dev user=tloginsvr password=devpass"
//   export TLOGINSVR_TEST_MSSQL_CONN="DSN=TLOGINSVR_MSSQL;DATABASE=tloginsvr_dev;UID=sa;PWD=DevPassword123!"
//   ctest -R tloginsvr_asio_soci_auth -V
//
// The test seeds a unique user prefix per run (process pid) so parallel
// runs don't collide and so the DB doesn't accumulate state across runs
// (cleanup at start + end via DELETE WHERE szUserID LIKE 'soci_test_%').

#include "fourstory/db/session_pool.h"
#include "../services/soci_auth_service.h"

#include <soci/soci.h>

#if defined(_WIN32)
#include <process.h>  // _getpid
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

// Wipe + seed deterministic fixtures. Idempotent; safe to re-run.
//
// Note: SOCI's `soci::use()` captures its argument by reference, so the
// bound value must outlive the statement. Pass named locals — not
// `prefix + "%"` temporaries — or the param gets read after the
// std::string destructor and you see garbage on the wire.
void SeedFixtures(soci::session& sql,
                  const std::string& prefix,
                  fourstory::db::Backend backend)
{
    const bool is_mssql = (backend == fourstory::db::Backend::Odbc);
    const std::string like = prefix + "%";
    const std::string alice = prefix + "alice";
    const std::string bob = prefix + "bob";
    const std::string ip_blocked = prefix + "1.2.3.4";
    const std::string pw_alice = "hunter2";
    const std::string pw_bob = "secret";
    const int uid_alice = 1000001;
    const int uid_bob = 1000002;

    // Cleanup any prior state. Two passes: by name prefix (catches
    // same-PID re-runs) and by the hardcoded uid range
    // (1000001-1000002 — catches stale rows from older PID runs
    // that crashed mid-seed before the per-prefix cleanup at end
    // could fire).
    sql << "DELETE FROM \"TLOG\" WHERE \"dwUserID\" IN ("
           "SELECT \"dwUserID\" FROM \"TACCOUNT_PW\" WHERE \"szUserID\" LIKE :p)",
        soci::use(like);
    sql << "DELETE FROM \"TCURRENTUSER\" WHERE \"dwUserID\" IN ("
           "SELECT \"dwUserID\" FROM \"TACCOUNT_PW\" WHERE \"szUserID\" LIKE :p)",
        soci::use(like);
    sql << "DELETE FROM \"TUSERPROTECTED\" WHERE \"dwUserID\" IN ("
           "SELECT \"dwUserID\" FROM \"TACCOUNT_PW\" WHERE \"szUserID\" LIKE :p)",
        soci::use(like);
    sql << "DELETE FROM \"TACCOUNT_PW\" WHERE \"szUserID\" LIKE :p",
        soci::use(like);
    // Catch stale rows from crashed runs (no prefix match, but the
    // hardcoded test uids landed in the DB).
    sql << "DELETE FROM \"TLOG\"          WHERE \"dwUserID\" IN (1000001, 1000002)";
    sql << "DELETE FROM \"TCURRENTUSER\"  WHERE \"dwUserID\" IN (1000001, 1000002)";
    sql << "DELETE FROM \"TUSERPROTECTED\" WHERE \"dwUserID\" IN (1000001, 1000002)";
    sql << "DELETE FROM \"TUSERINFOTABLE\" WHERE \"dwUserID\" IN (1000001, 1000002)";
    sql << "DELETE FROM \"TACCOUNT_PW\"   WHERE \"dwUserID\" IN (1000001, 1000002)";
    sql << "DELETE FROM \"IPBLACKLIST_game\" WHERE \"szIP\" LIKE :p",
        soci::use(like);
    // Best-effort: TIPAUTHORITY may not exist on every deploy (modern
    // treats it as optional). Wrap so prefix cleanup doesn't abort the
    // whole seed when the column / table is missing.
    try
    {
        sql << "DELETE FROM \"TIPAUTHORITY\" WHERE \"szIP\" LIKE :p",
            soci::use(like);
    }
    catch (const std::exception&) { /* swallow */ }
    // Same for USERIPLOG — optional audit table.
    try
    {
        sql << "DELETE FROM \"USERIPLOG\" WHERE \"Username\" LIKE :p",
            soci::use(like);
    }
    catch (const std::exception&) { /* swallow */ }

    // Real MSSQL has TACCOUNT_PW.dwUserID as IDENTITY — explicit
    // INSERT values get rejected with "Cannot insert explicit value
    // for identity column" unless IDENTITY_INSERT is briefly toggled
    // ON for this session. PG dev fixture defines dwUserID as a
    // plain PK with no identity/serial, so the bracket statements
    // only fire for the ODBC backend.
    // MSSQL: SET IDENTITY_INSERT must be batched with the INSERT
    // because the pragma only sticks for the duration of the batch
    // statement. Issuing it as a separate SOCI statement gets the
    // SET committed and then "forgotten" before the next INSERT —
    // SOCI's ODBC binding seems to reset state between sessions.
    // Combine SET + INSERT + SET-OFF into a single multi-statement
    // batch per row.
    auto insert_account = [&](int uid, const std::string& uname,
                              const std::string& pw) {
        if (is_mssql)
        {
            sql << "SET IDENTITY_INSERT \"TACCOUNT_PW\" ON; "
                   "INSERT INTO \"TACCOUNT_PW\" "
                   "(\"dwUserID\", \"szUserID\", \"szPasswd\") "
                   "VALUES (:uid, :u, :p); "
                   "SET IDENTITY_INSERT \"TACCOUNT_PW\" OFF;",
                soci::use(uid), soci::use(uname), soci::use(pw);
        }
        else
        {
            sql << "INSERT INTO \"TACCOUNT_PW\" "
                   "(\"dwUserID\", \"szUserID\", \"szPasswd\") "
                   "VALUES (:uid, :u, :p)",
                soci::use(uid), soci::use(uname), soci::use(pw);
        }
    };

    insert_account(uid_alice, alice, pw_alice);
    insert_account(uid_bob,   bob,   pw_bob);

    // Pre-seed TUSERINFOTABLE so alice's auth doesn't trip the
    // post-login agreement check (LR_NEEDAGREEMENT). Bob doesn't
    // need this — his test path stops at the ban check upstream.
    sql << "INSERT INTO \"TUSERINFOTABLE\" "
           "(\"dwUserID\", \"bCanCreateCharCount\", \"bAgreement\") "
           "VALUES (:u, 6, 1)",
        soci::use(uid_alice);
    // Real MSSQL TUSERPROTECTED has more NOT-NULL/no-default columns
    // than PG dev fixture (startTime, szComment, szGMID, sentBanMail).
    // Set them explicitly so both backends accept the INSERT.
    sql << "INSERT INTO \"TUSERPROTECTED\" "
           "(\"dwUserID\", \"bBlockType\", \"bEternal\", "
           " \"startTime\", \"dwDuration\", \"bBlockReason\", "
           " \"szComment\", \"szGMID\", \"sentBanMail\") "
           "VALUES (:uid, 1, 1, "
           "        CURRENT_TIMESTAMP, 0, 1, "
           "        '', '', 0)",
        soci::use(uid_bob);

    // IP block fixture.
    sql << "INSERT INTO \"IPBLACKLIST_game\" (\"szIP\") VALUES (:ip)",
        soci::use(ip_blocked);

    // TIPAUTHORITY pattern fixture — wildcard-banned IP range. Best-
    // effort: if the optional table isn't deployed we just skip the
    // companion test below.
    try
    {
        const std::string pattern = prefix + "9.%";
        sql << "INSERT INTO \"TIPAUTHORITY\" (\"szIP\", \"bAuthority\") "
               "VALUES (:p, 0)",
            soci::use(pattern);
    }
    catch (const std::exception&) { /* swallow */ }
}

void RunTests(fourstory::db::Backend backend, const std::string& conn)
{
    fourstory::db::SessionPool pool(backend, conn, /*pool_size=*/2);

    // Distinct prefix per (backend, pid) — keeps parallel-run isolation
    // and avoids name collisions across the two backends, even if both
    // share a host.
    const char* tag = (backend == fourstory::db::Backend::Odbc) ? "ms" : "pg";
    const std::string prefix =
        std::string("soci_test_") + tag + "_" + std::to_string(::getpid()) + "_";

    // Seed using a dedicated session, then release before the service runs.
    {
        auto lease = pool.Acquire();
        SeedFixtures(*lease, prefix, backend);
    }

    tloginsvr::services::SociAuthService svc(pool);

    using namespace tloginsvr::services;

    // 1. Good login.
    {
        AuthRequest req{
            .user_id = prefix + "alice",
            .password = "hunter2",
            .client_ip = "127.0.0.1",
            .client_version = 0x2918,
        };
        const auto r = svc.Authenticate(req);
        Check(r.status == AuthStatus::Success,
            "alice + correct password → Success");
        Check(r.user_id == 1000001, "alice user_id matches seeded row");
        Check(r.session_key != 0, "session_key populated");

        // USERIPLOG row recorded for post-mortem audit (legacy TLogin
        // SP line 160). Best-effort: skip the assertion if the
        // optional table isn't deployed.
        try
        {
            auto lease = pool.Acquire();
            int hits = 0;
            *lease << "SELECT COUNT(*) FROM \"USERIPLOG\" "
                      "WHERE \"Username\" = :u AND \"IP\" = :ip",
                soci::use(req.user_id),
                soci::use(req.client_ip),
                soci::into(hits);
            Check(hits >= 1,
                "Successful login → USERIPLOG row recorded");
        }
        catch (const std::exception&)
        {
            std::printf("  SKIP  USERIPLOG audit check — table not deployed\n");
        }
    }

    // 2. Wrong password.
    {
        AuthRequest req{
            .user_id = prefix + "alice",
            .password = "wrong",
            .client_ip = "127.0.0.1",
        };
        const auto r = svc.Authenticate(req);
        Check(r.status == AuthStatus::WrongPassword,
            "alice + wrong password → WrongPassword");
    }

    // 3. Unknown user.
    {
        AuthRequest req{
            .user_id = prefix + "ghost",
            .password = "anything",
            .client_ip = "127.0.0.1",
        };
        const auto r = svc.Authenticate(req);
        Check(r.status == AuthStatus::NoUser,
            "unknown user → NoUser");
    }

    // 4. User-level ban (eternal).
    {
        AuthRequest req{
            .user_id = prefix + "bob",
            .password = "secret",
            .client_ip = "127.0.0.1",
        };
        const auto r = svc.Authenticate(req);
        Check(r.status == AuthStatus::Banned,
            "bob (eternal-banned) + correct password → Banned");
        Check(r.user_id == 1000002, "ban result carries user_id");
    }

    // 5. IP banlist (IPBLACKLIST_game exact match).
    {
        AuthRequest req{
            .user_id = prefix + "alice",
            .password = "hunter2",
            .client_ip = prefix + "1.2.3.4",
        };
        const auto r = svc.Authenticate(req);
        Check(r.status == AuthStatus::IpBanned,
            "IPBLACKLIST_game exact match → IpBanned");
    }

    // 5b. TIPAUTHORITY LIKE-pattern banlist. Seed pattern is
    // `<prefix>9.%`; client_ip below matches that pattern. Mirrors
    // legacy CSPCheckIP / TCheckIP SP semantics. Best-effort: if the
    // table isn't deployed, the seeding step swallowed silently and
    // this test would fail-open (status != IpBanned). Detect by
    // checking the row landed before asserting.
    {
        bool seeded = false;
        try
        {
            auto lease = pool.Acquire();
            const std::string pattern = prefix + "9.%";
            int hit = 0;
            *lease << "SELECT COUNT(*) FROM \"TIPAUTHORITY\" "
                      "WHERE \"szIP\" = :p",
                soci::use(pattern), soci::into(hit);
            seeded = (hit > 0);
        }
        catch (const std::exception&) { /* table missing — skip */ }

        if (seeded)
        {
            AuthRequest req{
                .user_id = prefix + "alice",
                .password = "hunter2",
                .client_ip = prefix + "9.42.42.42",
            };
            const auto r = svc.Authenticate(req);
            Check(r.status == AuthStatus::IpBanned,
                "TIPAUTHORITY pattern match → IpBanned");

            // Negative: an IP outside the pattern should NOT be banned
            // by TIPAUTHORITY (would still need to bypass everything
            // else to reach Success — we just check it's not IpBanned).
            AuthRequest req_ok{
                .user_id = prefix + "alice",
                .password = "hunter2",
                .client_ip = prefix + "8.42.42.42",
            };
            const auto r_ok = svc.Authenticate(req_ok);
            Check(r_ok.status != AuthStatus::IpBanned,
                "TIPAUTHORITY non-matching IP → not IpBanned");
        }
        else
        {
            std::printf("  SKIP  TIPAUTHORITY test — table not deployed\n");
        }
    }

    // 6. Duplicate-session detection. Alice already has a live row
    // from test 1; modern's flow (post-G8) is to flag the stale row
    // for kick and return Duplicate so the handler can route to the
    // duplicate-kick ack. The terminator clears the row
    // asynchronously after the prior session disconnects.
    {
        AuthRequest req{
            .user_id = prefix + "alice",
            .password = "hunter2",
            .client_ip = "127.0.0.1",
        };
        const auto r = svc.Authenticate(req);
        Check(r.status == AuthStatus::Duplicate,
            "alice re-auth with live session → Duplicate (flagged for kick)");
    }

    // 7. SetAgreement writes TUSERINFOTABLE.bAgreement (NOT
    //    TACCOUNT_PW.bCheck — separate legacy flag). Exercises the
    //    UPDATE branch since alice's TUSERINFOTABLE row was seeded
    //    above. Use bob (uid 1000002) for the INSERT branch — his
    //    row wasn't seeded so SetAgreement will INSERT it.
    {
        auto lease = pool.Acquire();

        // INSERT branch: bob has no TUSERINFOTABLE row.
        int bob_rows_before = 0;
        *lease << "SELECT COUNT(*) FROM \"TUSERINFOTABLE\" WHERE \"dwUserID\" = 1000002",
            soci::into(bob_rows_before);
        Check(bob_rows_before == 0,
            "bob has no TUSERINFOTABLE row before SetAgreement (INSERT branch)");

        svc.SetAgreement(1000002);
        int bob_after = 0;
        *lease << "SELECT \"bAgreement\" FROM \"TUSERINFOTABLE\" "
                  "WHERE \"dwUserID\" = 1000002",
            soci::into(bob_after);
        Check(bob_after == 1,
            "SetAgreement(bob) → INSERT TUSERINFOTABLE.bAgreement=1");

        // UPDATE branch: alice's row was seeded with bAgreement=1.
        // SetAgreement should leave it at 1 (idempotent UPDATE).
        svc.SetAgreement(1000001);
        int alice_after = 0;
        *lease << "SELECT \"bAgreement\" FROM \"TUSERINFOTABLE\" "
                  "WHERE \"dwUserID\" = 1000001",
            soci::into(alice_after);
        Check(alice_after == 1,
            "SetAgreement(alice) → UPDATE branch, bAgreement stays 1");

        // Idempotent re-call for bob.
        svc.SetAgreement(1000002);
        int bob_after2 = 0;
        *lease << "SELECT \"bAgreement\" FROM \"TUSERINFOTABLE\" "
                  "WHERE \"dwUserID\" = 1000002",
            soci::into(bob_after2);
        Check(bob_after2 == 1, "SetAgreement is idempotent");
    }

    // 8. SetAgreement(0) is a safe no-op (never-authed session).
    {
        svc.SetAgreement(0);
        Check(true, "SetAgreement(0) → no-op (no exception)");
    }

    // Cleanup. Make a best-effort pass; failures here aren't fatal.
    try
    {
        auto lease = pool.Acquire();
        soci::session& sql = *lease;
        sql << "DELETE FROM \"TLOG\" WHERE \"dwUserID\" IN (1000001, 1000002)";
        sql << "DELETE FROM \"TCURRENTUSER\" WHERE \"dwUserID\" IN (1000001, 1000002)";
        sql << "DELETE FROM \"TUSERPROTECTED\" WHERE \"dwUserID\" IN (1000001, 1000002)";
        sql << "DELETE FROM \"TUSERINFOTABLE\" WHERE \"dwUserID\" IN (1000001, 1000002)";
        sql << "DELETE FROM \"TACCOUNT_PW\" WHERE \"dwUserID\" IN (1000001, 1000002)";
        const std::string cleanup_like = prefix + "%";
        sql << "DELETE FROM \"IPBLACKLIST_game\" WHERE \"szIP\" LIKE :p",
            soci::use(cleanup_like);
        try
        {
            sql << "DELETE FROM \"TIPAUTHORITY\" WHERE \"szIP\" LIKE :p",
                soci::use(cleanup_like);
        }
        catch (const std::exception&) { /* optional table */ }
        try
        {
            sql << "DELETE FROM \"USERIPLOG\" WHERE \"Username\" LIKE :p",
                soci::use(cleanup_like);
        }
        catch (const std::exception&) { /* optional table */ }
    }
    catch (const std::exception& ex)
    {
        std::printf("  WARN  cleanup error (non-fatal): %s\n", ex.what());
    }
}

} // namespace

int main()
{
    std::printf("=== tloginsvr_asio SOCI auth-flow test ===\n");

    const char* pg_conn    = std::getenv("TLOGINSVR_TEST_PG_CONN");
    const char* mssql_conn = std::getenv("TLOGINSVR_TEST_MSSQL_CONN");

    if ((pg_conn == nullptr || pg_conn[0] == '\0') &&
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
