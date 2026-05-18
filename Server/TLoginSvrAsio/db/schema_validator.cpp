#include "schema_validator.h"
#include "session_pool.h"

#include <soci/soci.h>

#include <spdlog/spdlog.h>

#include <initializer_list>
#include <string>
#include <vector>

namespace tloginsvr::db {

namespace {

// One-shot per-pool check: confirm every (table, column) listed exists
// in the connected database. Uses INFORMATION_SCHEMA (portable across
// MSSQL + PostgreSQL — both surface columns there).
//
// Implementation choice — one COUNT query per column. Could be one
// query with IN-list per table, but the per-column form keeps the
// error message specific ("table TFOO column bar missing") and
// startup cost is trivial.
void CheckColumns(soci::session& sql,
                  const char* pool_label,
                  std::initializer_list<std::pair<const char*, const char*>> required)
{
    std::vector<std::string> missing;
    for (const auto& [table, column] : required)
    {
        int hits = 0;
        try
        {
            // Inline the identifier literals rather than binding as
            // parameters. INFORMATION_SCHEMA.COLUMNS stores
            // TABLE_NAME / COLUMN_NAME as `sysname` (NVARCHAR(128))
            // on MSSQL; SOCI's default ODBC string binding maps to
            // SQL_VARCHAR which compares to non-matching widechar
            // columns as "no rows" (silent miss). Table + column
            // names here are compile-time constants from the
            // CheckColumns list, so direct substitution is safe.
            std::string q =
                std::string("SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                            "WHERE TABLE_NAME = '") + table +
                "' AND COLUMN_NAME = '" + column + "'";
            sql << q, soci::into(hits);
        }
        catch (const std::exception& ex)
        {
            throw SchemaError(std::string("schema_validator (") + pool_label +
                "): INFORMATION_SCHEMA query failed: " + ex.what());
        }
        if (hits == 0)
        {
            missing.emplace_back(std::string(table) + "." + column);
        }
    }
    if (!missing.empty())
    {
        std::string msg = std::string("schema_validator (") + pool_label +
            "): missing column(s):";
        for (const auto& m : missing) { msg += ' '; msg += m; }
        throw SchemaError(msg);
    }
    spdlog::info("schema_validator ({}) OK ({} columns checked)",
        pool_label, required.size());
}

} // namespace

void ValidateGlobalSchema(SessionPool& pool)
{
    // Columns SociAuthService + SociMapServerLocator + the
    // global-side parts of SociCharService read or write.
    auto lease = pool.Acquire();
    CheckColumns(*lease, "global", {
        // Accounts + credentials
        { "TACCOUNT_PW",      "dwUserID" },
        { "TACCOUNT_PW",      "szUserID" },
        { "TACCOUNT_PW",      "szPasswd" },
        { "TACCOUNT_PW",      "dLastLogin" },
        // IP banlist
        { "IPBLACKLIST_game", "szIP" },
        // User-level bans
        { "TUSERPROTECTED",   "dwUserID" },
        { "TUSERPROTECTED",   "bEternal" },
        { "TUSERPROTECTED",   "startTime" },
        { "TUSERPROTECTED",   "dwDuration" },
        // Live sessions
        { "TCURRENTUSER",     "dwKEY" },
        { "TCURRENTUSER",     "dwUserID" },
        { "TCURRENTUSER",     "szLoginIP" },
        { "TCURRENTUSER",     "bLocked" },
        // Audit
        { "TLOG",             "dwKEY" },
        { "TLOG",             "dwUserID" },
        { "TLOG",             "timeLOGOUT" },
        // Agreement
        { "TUSERINFOTABLE",   "dwUserID" },
        { "TUSERINFOTABLE",   "bAgreement" },
        // Server registry
        { "TSERVER",          "bGroupID" },
        { "TSERVER",          "bServerID" },
        { "TSERVER",          "bType" },
        { "TSERVER",          "bMachineID" },
        { "TSERVER",          "wPort" },
        { "TIPADDR",          "bMachineID" },
        { "TIPADDR",          "szIPAddr" },
        { "TIPADDR",          "bActive" },
        { "TGROUP",           "bGroupID" },
        { "TGROUP",           "szNAME" },
        { "TGROUP",           "bStatus" },
        { "TCHANNEL",         "bGroupID" },
        { "TCHANNEL",         "bChannel" },
        { "TCHANNEL",         "szNAME" },
        // Cross-world char index
        { "TALLCHARTABLE",    "dwUserID" },
        { "TALLCHARTABLE",    "bWorldID" },
        { "TALLCHARTABLE",    "dwCharID" },
        { "TALLCHARTABLE",    "bDelete" },
        // Char name reservation
        { "TRESERVEDNAME",    "szName" },
        { "TKEEPINGNAME",     "szName" },
        // Veteran chart
        { "TVETERANCHART",    "bID" },
        { "TVETERANCHART",    "bLevel" },
    });
}

void ValidateWorldSchema(SessionPool& pool)
{
    auto lease = pool.Acquire();
    CheckColumns(*lease, "world", {
        // Per-world char rows (read by CharList, write by Create/Delete)
        { "TCHARTABLE",       "dwCharID" },
        { "TCHARTABLE",       "dwUserID" },
        { "TCHARTABLE",       "szNAME" },
        { "TCHARTABLE",       "bSlot" },
        { "TCHARTABLE",       "bDelete" },
        { "TCHARTABLE",       "bClass" },
        { "TCHARTABLE",       "bLevel" },
        // Items (equipped JOIN for CharList)
        { "TITEMTABLE",       "dlID" },
        { "TITEMTABLE",       "bStorageType" },
        { "TITEMTABLE",       "dwStorageID" },
        { "TITEMTABLE",       "bOwnerType" },
        { "TITEMTABLE",       "dwOwnerID" },
        { "TITEMTABLE",       "bItemID" },
        { "TITEMTABLE",       "wItemID" },
        { "TITEMTABLE",       "bLevel" },
        { "TITEMTABLE",       "bGradeEffect" },
        { "TITEMTABLE",       "wMoggItemID" },
        // Guild fame
        { "TGUILDMEMBERTABLE","dwCharID" },
        { "TGUILDMEMBERTABLE","dwGuildID" },
        { "TGUILDTABLE",      "dwID" },
        { "TGUILDTABLE",      "szName" },
        { "TGUILDTABLE",      "dwFame" },
        { "TGUILDTABLE",      "dwFameColor" },
    });
}

} // namespace tloginsvr::db
