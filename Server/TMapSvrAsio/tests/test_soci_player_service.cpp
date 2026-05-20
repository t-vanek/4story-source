// Integration test for SociPlayerService.
//
// Runs against a live DB when the env var TMAPSVR_TEST_PG_CONN or
// TMAPSVR_TEST_MSSQL_CONN is set; skips cleanly when neither is
// configured so a no-DB CI run still passes. Pattern matches
// test_soci_session_validator.cpp exactly.
//
// Preconditions (only checked when a connection string is set):
//   * TCHARTABLE row with dwID = TMAPSVR_TEST_CHAR_ID (default 1)
//     and dwUserID = TMAPSVR_TEST_USER_ID (default 1) must exist.
//   * The row's szNAME must be non-empty.
//
// The test does NOT write to the DB.

#include "services/soci_player_service.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <cstdio>
#include <exception>
#include <string>

namespace {

int g_passed = 0;
int g_failed = 0;
int g_skipped = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

void Skip(const char* reason)
{
    ++g_skipped;
    std::printf("  SKIP  %s\n", reason);
}

std::string GetEnvOr(const char* name, const char* def)
{
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string(def);
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  SociPlayerService integration test ===\n\n");

    const std::string pg_conn =
        GetEnvOr("TMAPSVR_TEST_PG_CONN", "");
    const std::string ms_conn =
        GetEnvOr("TMAPSVR_TEST_MSSQL_CONN", "");

    if (pg_conn.empty() && ms_conn.empty())
    {
        Skip("TMAPSVR_TEST_PG_CONN and TMAPSVR_TEST_MSSQL_CONN not set "
             "— skipping live DB test");
        std::printf("\nResults: %d passed, %d failed, %d skipped\n",
            g_passed, g_failed, g_skipped);
        return 0;
    }

    const bool use_pg         = !pg_conn.empty();
    const std::string conn_str = use_pg ? pg_conn : ms_conn;
    const std::string backend  = use_pg ? "postgresql" : "odbc";

    const std::uint32_t test_char_id =
        static_cast<std::uint32_t>(std::stoul(
            GetEnvOr("TMAPSVR_TEST_CHAR_ID", "1")));
    const std::uint32_t test_user_id =
        static_cast<std::uint32_t>(std::stoul(
            GetEnvOr("TMAPSVR_TEST_USER_ID", "1")));

    std::printf("  backend=%s  char_id=%u  user_id=%u\n\n",
        backend.c_str(), test_char_id, test_user_id);

    try
    {
        fourstory::db::SessionPool pool(backend, conn_str, 2);
        tmapsvr::SociPlayerService svc(pool);

        // §1 Valid lookup
        auto snap = svc.LoadChar(test_char_id, test_user_id, 0xDEADBEEF);
        Check(snap.has_value(),
            "LoadChar returns a snapshot for the test char row");

        if (snap)
        {
            Check(snap->char_id == test_char_id,
                "snapshot.char_id matches requested char_id");
            Check(snap->user_id == test_user_id,
                "snapshot.user_id matches requested user_id");
            Check(snap->dw_key == 0xDEADBEEFu,
                "snapshot.dw_key is the caller-supplied token");
            Check(!snap->name.empty(),
                "snapshot.name is non-empty (szNAME present in row)");
            Check(snap->level >= 1 && snap->level <= 200,
                "snapshot.level is in expected range [1,200]");
            Check(snap->position.map_id > 0,
                "snapshot.position.map_id is set");
            std::printf("    char name='%s' level=%u map_id=%u\n",
                snap->name.c_str(), snap->level, snap->position.map_id);
        }

        // §2 Unknown char_id → nullopt
        auto miss = svc.LoadChar(0xFFFFFFFF, test_user_id, 0);
        Check(!miss.has_value(),
            "LoadChar returns nullopt for unknown char_id");

        // §3 Wrong user_id → nullopt
        auto wrong_uid = svc.LoadChar(test_char_id, 0, 0);
        Check(!wrong_uid.has_value(),
            "LoadChar returns nullopt when user_id doesn't match");
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception: %s\n", ex.what());
        ++g_failed;
    }

    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
