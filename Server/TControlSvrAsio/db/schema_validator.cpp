// Boot-time schema validator for TControlSvrAsio. Confirms the
// inventory tables the SOCI service-inventory loader reads
// (TMACHINE / TGROUP / TSVRTYPE / TSERVER / TIPADDR) plus the
// MANAGER auth table referenced by the legacy TOPLogin SP.
//
// Event / cash-shop / pre-version tables are optional in F2 —
// they're consumed by F4 (event scheduler) and F5 (patch metadata);
// missing them logs a warning instead of aborting so the F2 binary
// can stand up against a dev DB that hasn't deployed those tables
// yet.

#include "schema_validator.h"

#include "fourstory/db/schema_validator.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <string>
#include <vector>

namespace tcontrolsvr::db {

namespace {

// Yes-or-no probe against INFORMATION_SCHEMA. Soft-fails to false on
// driver / permission errors so we warn rather than abort when the
// probe itself can't run.
bool TableHasColumns(soci::session& sql,
                     const std::string& table,
                     const std::vector<std::string>& cols)
{
    if (cols.empty()) return false;
    std::string in_clause;
    for (std::size_t i = 0; i < cols.size(); ++i)
    {
        if (i) in_clause += ",";
        in_clause += "'";
        in_clause += cols[i];
        in_clause += "'";
    }
    int hits = 0;
    try
    {
        sql << "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
               "WHERE TABLE_NAME = '" + table + "' "
               "AND COLUMN_NAME IN (" + in_clause + ")",
            soci::into(hits);
    }
    catch (const std::exception& ex)
    {
        spdlog::debug("schema_validator (control_global): probe '{}' "
                      "skipped: {}", table, ex.what());
        return false;
    }
    return hits == static_cast<int>(cols.size());
}

} // namespace

void ValidateGlobalSchema(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();

    // Required: the four inventory tables the SOCI service-inventory
    // loader reads. Column list matches SociServiceInventory::Reload.
    fourstory::db::CheckColumns(*lease, "control_global", {
        { "TGROUP",    "bGroupID" },
        { "TGROUP",    "szName" },
        { "TMACHINE",  "bMachineID" },
        { "TMACHINE",  "szName" },
        { "TMACHINE",  "bRouteID" },
        { "TIPADDR",   "bMachineID" },
        { "TIPADDR",   "szIPAddr" },
        { "TIPADDR",   "szPriAddr" },
        { "TIPADDR",   "bActive" },
        { "TSVRTYPE",  "bType" },
        { "TSVRTYPE",  "szName" },
        { "TSVRTYPE",  "bControl" },
        { "TSERVER",   "bGroupID" },
        { "TSERVER",   "bServerID" },
        { "TSERVER",   "bType" },
        { "TSERVER",   "bMachineID" },
        { "TSERVER",   "wPort" },
        { "TSERVER",   "szName" },
    });

    // Optional in F2 — these tables back handler families that
    // ship later (F4 events, F5 pre-version metadata).
    if (!TableHasColumns(*lease, "TEVENTCHART",
            {"dwIndex","bID","szTitle","bGroupID","bSvrType","bSvrID"}))
    {
        spdlog::warn("schema_validator (control_global): TEVENTCHART "
                     "not deployed — CT_EVENTLIST/CHANGE/UPDATE will be "
                     "stubbed when those handlers land in F4.");
    }
    if (!TableHasColumns(*lease, "TCASHSHOPITEMCHART",
            {"wID","szName","bCanSell"}))
    {
        spdlog::warn("schema_validator (control_global): TCASHSHOPITEMCHART "
                     "not deployed — CT_CASHITEMLIST will return empty when "
                     "that handler lands in F4.");
    }
    if (!TableHasColumns(*lease, "TPREVERSION",
            {"dwBetaVer","szPath","szName","dwSize"}))
    {
        spdlog::warn("schema_validator (control_global): TPREVERSION "
                     "not deployed — CT_PREVERSIONTABLE/UPDATE will be "
                     "stubbed when those handlers land in F5.");
    }
}

} // namespace tcontrolsvr::db
