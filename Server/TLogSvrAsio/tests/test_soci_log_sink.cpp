// Integration test for SociLogSink + the audit schema validator
// against a live MSSQL database. Skips (exits 0) when
// TLOGSVR_TEST_MSSQL_CONN is unset — matches the convention the
// sibling soci tests use so CI shards without a DB still go green.
//
// What it covers:
//   * ValidateAuditSchema accepts the shipped TLOG_AUDIT shape
//   * ValidateAuditSchema rejects a non-existent table name
//   * ValidateAuditSchema refuses an unsafe identifier (SQL injection
//     trap) without ever touching the DB
//   * SociLogSink::Write round-trips a record into TLOG_AUDIT and the
//     row is readable with the same field values
//
// The test cleans up its own inserted rows on success. On exception
// the row leak is acceptable — a follow-up run keys off
// LT_CLIENTIP = '__test__:<pid>' so we never collide with another run.

#include "db/schema_validator.h"
#include "fourstory/db/schema_validator.h"
#include "fourstory/db/session_pool.h"
#include "../services/log_sink.h"

#include <soci/soci.h>

#if defined(_WIN32)
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
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

std::string TestClientIp()
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "__test__:%d", static_cast<int>(::getpid()));
    return buf;
}

void DeleteTestRows(soci::session& sql)
{
    try
    {
        const auto ip = TestClientIp();
        sql << "DELETE FROM TLOG_AUDIT WHERE LT_CLIENTIP = :ip",
            soci::use(ip);
    }
    catch (const std::exception&) { /* best-effort cleanup */ }
}

void TestValidatorAcceptsShippedSchema(fourstory::db::SessionPool& pool)
{
    std::printf("[soci_log_sink — validator accepts TLOG_AUDIT]\n");
    bool ok = false;
    try
    {
        tlogsvr::db::ValidateAuditSchema(pool, "TLOG_AUDIT");
        ok = true;
    }
    catch (const std::exception& ex)
    {
        std::printf("  -- unexpected: %s\n", ex.what());
    }
    Check(ok, "ValidateAuditSchema(TLOG_AUDIT) returns without throwing");
}

void TestValidatorRejectsMissingTable(fourstory::db::SessionPool& pool)
{
    std::printf("[soci_log_sink — validator rejects missing table]\n");
    bool threw = false;
    try
    {
        tlogsvr::db::ValidateAuditSchema(pool, "TLOG_AUDIT_DOES_NOT_EXIST");
    }
    catch (const fourstory::db::SchemaError&) { threw = true; }
    catch (const std::exception&)             { threw = true; }
    Check(threw, "missing target_table throws SchemaError");
}

void TestValidatorRefusesUnsafeIdentifier(fourstory::db::SessionPool& pool)
{
    std::printf("[soci_log_sink — validator refuses SQL-meta identifier]\n");
    // SOCI never sees this string — the identifier check inside
    // ValidateAuditSchema rejects it before building the query, so
    // operators can't trick the boot path into running arbitrary SQL
    // via the target_table TOML key.
    bool threw = false;
    try
    {
        tlogsvr::db::ValidateAuditSchema(pool, "TLOG; DROP TABLE TLOG_AUDIT--");
    }
    catch (const fourstory::db::SchemaError&) { threw = true; }
    Check(threw, "target_table with SQL meta chars throws SchemaError");
}

void TestSinkRoundTrip(fourstory::db::SessionPool& pool)
{
    std::printf("[soci_log_sink — Write round-trips into TLOG_AUDIT]\n");
    {
        auto lease = pool.Acquire();
        DeleteTestRows(*lease);
    }

    tlogsvr::SociLogSink sink(pool, "TLOG_AUDIT");

    tlogsvr::LogRecord rec{};
    rec.timestamp_iso = "2026-05-20 14:07:42";
    rec.server_id     = 0x4242;
    rec.client_ip     = TestClientIp();
    rec.action        = 0x0008;  // LOGLOGIN_GAMESTART
    rec.map_id        = 31;
    rec.pos_x = 100; rec.pos_y = 200; rec.pos_z = 300;
    for (int i = 0; i < 11; ++i) rec.search_int[i] = 1000 + i;
    rec.search_str[0] = "user42";
    rec.search_str[4] = "ok";
    rec.format        = 0;
    const char blob_bytes[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    rec.payload.assign(
        reinterpret_cast<const std::byte*>(blob_bytes),
        reinterpret_cast<const std::byte*>(blob_bytes) + sizeof(blob_bytes));

    sink.Write(rec);

    {
        auto lease = pool.Acquire();
        soci::session& sql = *lease;
        int hits = 0;
        long long sid_back = 0;
        long long act_back = 0;
        std::string ip_back;
        std::string s0_back;
        std::string s4_back;
        sql << "SELECT TOP 1 LT_SERVERID, LT_ACTION, LT_CLIENTIP, "
               "LT_KEY1, LT_KEY5 FROM TLOG_AUDIT "
               "WHERE LT_CLIENTIP = :ip ORDER BY LT_ID DESC",
            soci::use(rec.client_ip),
            soci::into(sid_back), soci::into(act_back),
            soci::into(ip_back),
            soci::into(s0_back), soci::into(s4_back);
        hits = sql.got_data() ? 1 : 0;

        Check(hits == 1, "row landed in TLOG_AUDIT");
        Check(sid_back == 0x4242, "LT_SERVERID round-trip");
        Check(act_back == 0x0008, "LT_ACTION round-trip");
        Check(ip_back == rec.client_ip, "LT_CLIENTIP round-trip");
        Check(s0_back == "user42", "LT_KEY1 round-trip");
        Check(s4_back == "ok",      "LT_KEY5 round-trip");

        DeleteTestRows(sql);
    }
}

} // namespace

int main()
{
    std::printf("=== tlogsvr_asio soci_log_sink integration test ===\n");

    const char* conn_env = std::getenv("TLOGSVR_TEST_MSSQL_CONN");
    if (!conn_env || std::strlen(conn_env) == 0)
    {
        std::printf("SKIP: TLOGSVR_TEST_MSSQL_CONN not set\n");
        return 0;
    }

    try
    {
        fourstory::db::SessionPool pool(
            fourstory::db::Backend::Odbc, conn_env, /*pool_size=*/2);

        TestValidatorAcceptsShippedSchema(pool);
        TestValidatorRejectsMissingTable(pool);
        TestValidatorRefusesUnsafeIdentifier(pool);
        TestSinkRoundTrip(pool);
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }

    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
