// Boot-time schema validator — fail-fast on schema drift.
//
// The two entry points each hand a curated `{table, column}` list to
// the shared fourstory::db::CheckColumns helper, which issues a
// dialect-aware introspection query (information_schema.columns on PG
// / MSSQL, sqlite_master pragma_table_info on SQLite) and throws
// fourstory::db::SchemaError naming the first missing entry.
//
// Why fail-fast: a missing column on TGLOBAL would otherwise surface
// only on the first CS_LOGIN_REQ, by which time the listener is open
// and ops sees a flood of LR_INTERNAL replies with no clear cause.
// Validating before the listener binds turns the same problem into a
// loud startup error.
//
// Columns covered match exactly what SociAuthService /
// SociCharService / SociMapServerLocator / SociSessionTerminator read
// or write — TGLOBAL: 40 columns, TGAME: 23 columns. Keep both lists
// in lockstep with the SOCI impls.
//
// Legacy parity: legacy server didn't validate — it just crashed on
// the first SQL error. This is a new safety net.

#include "schema_validator.h"

#include "fourstory/db/schema_validator.h"
#include "fourstory/db/session_pool.h"

namespace tloginsvr::db {

void ValidateGlobalSchema(fourstory::db::SessionPool& pool)
{
    // Columns SociAuthService + SociMapServerLocator + the global-side
    // parts of SociCharService read or write.
    auto lease = pool.Acquire();
    fourstory::db::CheckColumns(*lease, "global", {
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
        { "TCURRENTUSER",     "bGroupID" },
        { "TCURRENTUSER",     "bChannel" },
        // Audit
        { "TLOG",             "dwKEY" },
        { "TLOG",             "dwUserID" },
        { "TLOG",             "timeLOGOUT" },
        { "TLOG",             "dwCharID" },
        { "TLOG",             "bGroupID" },
        { "TLOG",             "bChannel" },
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
        // 2FA (apply schema/2fa-tables.sql if missing)
        { "TUSEREMAIL",       "dwUserID" },
        { "TUSEREMAIL",       "szEmail" },
        { "TUSEREMAIL",       "bTwoFactorEnabled" },
        { "TUSERTRUSTEDIP",   "dwUserID" },
        { "TUSERTRUSTEDIP",   "szIP" },
    });
}

void ValidateWorldSchema(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();
    fourstory::db::CheckColumns(*lease, "world", {
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
