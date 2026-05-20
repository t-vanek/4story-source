// Integration test for SociMapSessionValidator against a live database.
//
// Skips silently when neither TMAPSVR_TEST_PG_CONN nor
// TMAPSVR_TEST_MSSQL_CONN is set in the environment — same env-var
// convention as the other Asio SOCI tests.
//
// Coverage:
//   * Schema validator: passes on a healthy DB, fails fast on a
//     missing column (we drop a fake TCURRENTUSER column to force
//     the negative branch).
//   * Validator happy path: seed a row, validate the matching
//     (uid, char, key) tuple → true.
//   * Validator deny paths: row absent / wrong key → false.

#include "../db/schema_validator.h"
#include "../services/soci_session_validator.h"
#include "fourstory/db/schema_validator.h"
#include "fourstory/db/session_pool.h"

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

// Insert one TCURRENTUSER row with the full NOT-NULL column set the
// legacy MSSQL schema requires. PG dev fixture has DEFAULT 0 on those
// columns so the short shape also works there. Returns the row's
// auto-assigned dwKEY (the value the client must echo on
// CS_CONNECT_REQ).
std::uint32_t InsertCurrentUser(soci::session& sql,
                                fourstory::db::Backend backend,
                                int user_id,
                                int char_id)
{
    const bool is_mssql = (backend == fourstory::db::Backend::Odbc);
    int dw_key = 0;
    if (is_mssql)
    {
        sql << "INSERT INTO \"TCURRENTUSER\" "
               "(\"dwUserID\", \"dwCharID\", \"bGroupID\", \"bChannel\", "
               " \"wPort\", \"bLocked\", \"szLoginIP\") "
               "OUTPUT INSERTED.\"dwKEY\" "
               "VALUES (:u, :c, 0, 0, 0, 0, '127.0.0.1')",
            soci::use(user_id), soci::use(char_id),
            soci::into(dw_key);
    }
    else
    {
        sql << "INSERT INTO \"TCURRENTUSER\" "
               "(\"dwUserID\", \"dwCharID\", \"bGroupID\", \"bChannel\", "
               " \"wPort\", \"bLocked\", \"szLoginIP\") "
               "VALUES (:u, :c, 0, 0, 0, 0, '127.0.0.1') "
               "RETURNING \"dwKEY\"",
            soci::use(user_id), soci::use(char_id),
            soci::into(dw_key);
    }
    return static_cast<std::uint32_t>(dw_key);
}

void RunTests(fourstory::db::Backend backend, const std::string& conn)
{
    fourstory::db::SessionPool pool(backend, conn, /*pool_size=*/2);

    // 1. Schema validator passes on the deployed schema.
    try
    {
        tmapsvr::db::ValidateGlobalSchema(pool);
        Check(true, "ValidateGlobalSchema → no exception on healthy DB");
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  ValidateGlobalSchema unexpected throw: %s\n",
            ex.what());
        ++g_failed;
    }

    // 2. Schema validator fails fast on a missing column. Use a
    //    column name the validator demands but TCURRENTUSER doesn't
    //    have — simulated via the shared CheckColumns directly so we
    //    don't have to ALTER the real table.
    try
    {
        auto lease = pool.Acquire();
        fourstory::db::CheckColumns(*lease, "map_global_fake", {
            { "TCURRENTUSER", "intentionally_missing_column" },
        });
        Check(false, "CheckColumns(missing) → should have thrown");
    }
    catch (const fourstory::db::SchemaError&)
    {
        Check(true, "CheckColumns(missing) → SchemaError thrown");
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception type: %s\n", ex.what());
        ++g_failed;
    }

    // PID-scoped user IDs so concurrent test runs don't collide. Split
    // per-backend tag for shared PG+MSSQL hosts.
    const int backend_tag = (backend == fourstory::db::Backend::Odbc) ? 500000 : 0;
    const int base_uid = 4000000 + backend_tag + (::getpid() % 1000);
    const int char_id  = 4242;

    // Clean any leftover state from a prior interrupted run.
    {
        auto lease = pool.Acquire();
        *lease << "DELETE FROM \"TCURRENTUSER\" "
                  "WHERE \"dwUserID\" BETWEEN :a AND :b",
            soci::use(base_uid), soci::use(base_uid + 99);
    }

    // 3. Seed one row, validate matching tuple → true.
    std::uint32_t real_key = 0;
    {
        auto lease = pool.Acquire();
        real_key = InsertCurrentUser(*lease, backend, base_uid + 1, char_id);
    }

    tmapsvr::SociMapSessionValidator validator(pool);
    {
        tmapsvr::MapSessionLookup lookup{};
        lookup.user_id = static_cast<std::uint32_t>(base_uid + 1);
        lookup.char_id = static_cast<std::uint32_t>(char_id);
        lookup.dw_key  = real_key;
        Check(validator.Validate(lookup),
            "Validate(matching uid/char/key) → true");
    }

    // 4. Wrong dwKEY → deny.
    {
        tmapsvr::MapSessionLookup lookup{};
        lookup.user_id = static_cast<std::uint32_t>(base_uid + 1);
        lookup.char_id = static_cast<std::uint32_t>(char_id);
        lookup.dw_key  = real_key ^ 0xFFFFFFFFu;  // bit-flipped — definitely wrong
        Check(!validator.Validate(lookup),
            "Validate(wrong key) → false");
    }

    // 5. Wrong dwUserID → deny.
    {
        tmapsvr::MapSessionLookup lookup{};
        lookup.user_id = static_cast<std::uint32_t>(base_uid + 999);
        lookup.char_id = static_cast<std::uint32_t>(char_id);
        lookup.dw_key  = real_key;
        Check(!validator.Validate(lookup),
            "Validate(wrong uid) → false");
    }

    // 6. Wrong dwCharID → deny.
    {
        tmapsvr::MapSessionLookup lookup{};
        lookup.user_id = static_cast<std::uint32_t>(base_uid + 1);
        lookup.char_id = static_cast<std::uint32_t>(char_id + 1);
        lookup.dw_key  = real_key;
        Check(!validator.Validate(lookup),
            "Validate(wrong char) → false");
    }

    // Cleanup.
    {
        auto lease = pool.Acquire();
        *lease << "DELETE FROM \"TCURRENTUSER\" "
                  "WHERE \"dwUserID\" BETWEEN :a AND :b",
            soci::use(base_uid), soci::use(base_uid + 99);
    }
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio SOCI session-validator test ===\n");

    const char* pg_conn    = std::getenv("TMAPSVR_TEST_PG_CONN");
    const char* mssql_conn = std::getenv("TMAPSVR_TEST_MSSQL_CONN");

    if ((pg_conn    == nullptr || pg_conn[0]    == '\0') &&
        (mssql_conn == nullptr || mssql_conn[0] == '\0'))
    {
        std::printf("  SKIP  neither TMAPSVR_TEST_PG_CONN nor "
                    "TMAPSVR_TEST_MSSQL_CONN set\n");
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
