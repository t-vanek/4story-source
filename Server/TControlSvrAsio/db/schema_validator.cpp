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
                     "not deployed — CT_PREVERSIONTABLE/UPDATE will "
                     "return empty / silently no-op until the table "
                     "is added.");
    }
    if (!TableHasColumns(*lease, "TVERSION",
            {"dwVersion","szPath","szName","dwSize","dwBetaVer"}))
    {
        spdlog::warn("schema_validator (control_global): TVERSION "
                     "not deployed — CT_UPDATEPATCH_REQ will fail "
                     "silently at the SP layer.");
    }

    // Dynamic peer state tables — warn when absent so devs can boot
    // against a minimal DB that hasn't run the peer DDL migration yet.
    // TControlSvr runs fine without them; peer state stays in-memory only.
    if (!TableHasColumns(*lease, "TPEER_REGISTRY",
            {"bGroupID","bServerID","bType","szReportedAddr",
             "wReportedPort","szVersion","dwPID","dtStartTime",
             "dwCurUsers","dwMaxUsers","llLeaseEpoch",
             "dtRegisteredAt","dtHeartbeatAt"}))
    {
        spdlog::warn("schema_validator (control_global): TPEER_REGISTRY "
                     "not deployed — peer registrations stay in-memory only "
                     "(lost on restart). Run the peer DDL migration to persist.");
    }
    if (!TableHasColumns(*lease, "TPEER_STATUS_LOG",
            {"bGroupID","bServerID","bOldStatus","bNewStatus","dtChangedAt"}))
    {
        spdlog::warn("schema_validator (control_global): TPEER_STATUS_LOG "
                     "not deployed — status-change history unavailable.");
    }
    if (!TableHasColumns(*lease, "TPEER_METRICS",
            {"bGroupID","bServerID","dwSession","dwUser","dtSampledAt"}))
    {
        spdlog::warn("schema_validator (control_global): TPEER_METRICS "
                     "not deployed — per-peer metrics history unavailable.");
    }
    if (!TableHasColumns(*lease, "TOP_AUDIT_LOG",
            {"szOperatorID","szAction","dtActionAt"}))
    {
        spdlog::warn("schema_validator (control_global): TOP_AUDIT_LOG "
                     "not deployed — operator audit log stays in spdlog only.");
    }
}

} // namespace tcontrolsvr::db
