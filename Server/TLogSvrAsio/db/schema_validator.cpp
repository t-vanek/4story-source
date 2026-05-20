// Boot-time schema validator for TLogSvrAsio — fail-fast on the audit
// table this server writes.
//
// Why split this off from the shared fourstory::db::CheckColumns: the
// target table name is configurable (TOML `target_table` key) rather
// than a compile-time constant the framework helper accepts. We
// validate the configured name against an SQL-identifier whitelist
// first, then build the INFORMATION_SCHEMA query with it inlined —
// matching the framework's approach for the column literals.
//
// Legacy parity: legacy TLogSvr did not validate — every datagram's
// INSERT either succeeded or got logged as a DB error and dropped.
// This is a new safety net at boot, matching the validators already
// running on TLoginSvrAsio and TPatchSvrAsio (F5 in SQL_AUDIT).

#include "schema_validator.h"

#include "fourstory/db/schema_validator.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace tlogsvr::db {

namespace {

// Identifier whitelist: SQL Server unquoted regular identifiers.
// We reject anything else rather than trying to escape it; the
// configured table name is operator-controlled, so a refusal here is
// strictly better than guessing at quoting semantics that vary by DB.
bool IsSafeIdentifier(const std::string& s)
{
    if (s.empty() || s.size() > 128) return false;
    const char first = s.front();
    if (!std::isalpha(static_cast<unsigned char>(first)) && first != '_')
        return false;
    return std::all_of(s.begin(), s.end(), [](char c) {
        const auto u = static_cast<unsigned char>(c);
        return std::isalnum(u) || c == '_';
    });
}

} // namespace

void ValidateAuditSchema(fourstory::db::SessionPool& pool,
                         const std::string& target_table)
{
    if (!IsSafeIdentifier(target_table))
    {
        throw fourstory::db::SchemaError(
            "schema_validator (audit): target_table '" + target_table +
            "' is not a safe SQL identifier (allowed: [A-Za-z_][A-Za-z0-9_]{0,127})");
    }

    auto lease = pool.Acquire();

    // Every column the SociLogSink INSERT binds. Must match
    // schema/tlog-audit.sql and the bind list in
    // services/log_sink.cpp::Write — drift here means the validator
    // would let a misshapen table through and the first UDP packet
    // would fail at INSERT time.
    static constexpr std::array<const char*, 28> kColumns{{
        "LT_LOGDATE", "LT_SERVERID", "LT_CLIENTIP", "LT_ACTION", "LT_MAPID",
        "LT_X", "LT_Y", "LT_Z",
        "LT_DWKEY1", "LT_DWKEY2", "LT_DWKEY3", "LT_DWKEY4", "LT_DWKEY5",
        "LT_DWKEY6", "LT_DWKEY7", "LT_DWKEY8", "LT_DWKEY9", "LT_DWKEY10",
        "LT_DWKEY11",
        "LT_KEY1", "LT_KEY2", "LT_KEY3", "LT_KEY4", "LT_KEY5", "LT_KEY6",
        "LT_KEY7",
        "LT_FMT", "LT_LOG",
    }};

    std::vector<std::string> missing;
    for (const char* column : kColumns)
    {
        int hits = 0;
        try
        {
            std::string q =
                std::string("SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                            "WHERE TABLE_NAME = '") + target_table +
                "' AND COLUMN_NAME = '" + column + "'";
            *lease << q, soci::into(hits);
        }
        catch (const std::exception& ex)
        {
            throw fourstory::db::SchemaError(
                std::string("schema_validator (audit): INFORMATION_SCHEMA "
                            "query failed for ") + target_table + "." + column +
                ": " + ex.what());
        }
        if (hits == 0)
            missing.emplace_back(std::string(target_table) + "." + column);
    }

    if (!missing.empty())
    {
        std::string msg = "schema_validator (audit): missing column(s):";
        for (const auto& m : missing) { msg += ' '; msg += m; }
        msg += " — apply Server/TLogSvrAsio/schema/tlog-audit.sql";
        throw fourstory::db::SchemaError(msg);
    }

    spdlog::info("schema_validator (audit) OK — {} ({} columns checked)",
        target_table, kColumns.size());
}

} // namespace tlogsvr::db
