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

// Wipe + seed deterministic fixtures. Idempotent; safe to re-run.
//
// Note: SOCI's `soci::use()` captures its argument by reference, so the
// bound value must outlive the statement. Pass named locals — not
// `prefix + "%"` temporaries — or the param gets read after the
// std::string destructor and you see garbage on the wire.
void SeedFixtures(soci::session& sql, const std::string& prefix)
{
    const std::string like = prefix + "%";
    const std::string alice = prefix + "alice";
    const std::string bob = prefix + "bob";
    const std::string ip_blocked = prefix + "1.2.3.4";
    const std::string pw_alice = "hunter2";
    const std::string pw_bob = "secret";
    const int uid_alice = 1000001;
    const int uid_bob = 1000002;

    // Cleanup any prior state for this prefix.
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
    sql << "DELETE FROM \"IPBLACKLIST_game\" WHERE \"szIP\" LIKE :p",
        soci::use(like);

    // User 1: alice — clean, valid credentials.
    sql << "INSERT INTO \"TACCOUNT_PW\" (\"dwUserID\", \"szUserID\", \"szPasswd\") "
           "VALUES (:uid, :u, :p)",
        soci::use(uid_alice), soci::use(alice), soci::use(pw_alice);

    // User 2: bob — valid creds but flagged in TUSERPROTECTED.
    sql << "INSERT INTO \"TACCOUNT_PW\" (\"dwUserID\", \"szUserID\", \"szPasswd\") "
           "VALUES (:uid, :u, :p)",
        soci::use(uid_bob), soci::use(bob), soci::use(pw_bob);
    sql << "INSERT INTO \"TUSERPROTECTED\" "
           "(\"dwUserID\", \"bBlockType\", \"bEternal\", \"dwDuration\", \"bBlockReason\") "
           "VALUES (:uid, 1, 1, 0, 1)",
        soci::use(uid_bob);

    // IP block fixture.
    sql << "INSERT INTO \"IPBLACKLIST_game\" (\"szIP\") VALUES (:ip)",
        soci::use(ip_blocked);
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
        SeedFixtures(*lease, prefix);
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

    // 5. IP banlist.
    {
        AuthRequest req{
            .user_id = prefix + "alice",
            .password = "hunter2",
            .client_ip = prefix + "1.2.3.4",
        };
        const auto r = svc.Authenticate(req);
        Check(r.status == AuthStatus::IpBanned,
            "banned IP → IpBanned (precedes user lookup)");
    }

    // 6. Duplicate-session cleanup. Alice already has a live row from
    // test 1; re-auth must succeed (current impl deletes the stale row).
    {
        AuthRequest req{
            .user_id = prefix + "alice",
            .password = "hunter2",
            .client_ip = "127.0.0.1",
        };
        const auto r = svc.Authenticate(req);
        Check(r.status == AuthStatus::Success,
            "alice re-auth → Success (stale TCURRENTUSER cleared)");
    }

    // 7. SetAgreement flips TACCOUNT_PW.bCheck (idempotent).
    {
        // Pre-check.
        auto lease = pool.Acquire();
        int before = 0;
        *lease << "SELECT \"bCheck\" FROM \"TACCOUNT_PW\" WHERE \"dwUserID\" = 1000001",
            soci::into(before);
        Check(before == 0, "alice bCheck starts at 0 (default after seed)");

        svc.SetAgreement(1000001);
        int after = 0;
        *lease << "SELECT \"bCheck\" FROM \"TACCOUNT_PW\" WHERE \"dwUserID\" = 1000001",
            soci::into(after);
        Check(after == 1, "SetAgreement(1000001) → bCheck=1");

        // Idempotent re-call.
        svc.SetAgreement(1000001);
        int after2 = 0;
        *lease << "SELECT \"bCheck\" FROM \"TACCOUNT_PW\" WHERE \"dwUserID\" = 1000001",
            soci::into(after2);
        Check(after2 == 1, "SetAgreement is idempotent");
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
        sql << "DELETE FROM \"TACCOUNT_PW\" WHERE \"dwUserID\" IN (1000001, 1000002)";
        const std::string cleanup_like = prefix + "%";
        sql << "DELETE FROM \"IPBLACKLIST_game\" WHERE \"szIP\" LIKE :p",
            soci::use(cleanup_like);
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
