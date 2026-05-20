// Boot-time schema validator for TPatchSvrAsio — fail-fast on the
// patch-metadata tables this server reads.
//
// Why split TVERSION/TPREVERSION (required) from TUSER_INTERFACE
// (optional): legacy `CT_CHANGEIF_REQ` is allowed to return 0 files
// when the operator hasn't deployed UI-file overrides, and the
// repository already swallows the "invalid object name" error in
// debug logs. Treating TUSER_INTERFACE as required would break that
// graceful path on minimal dev databases that only ship TVERSION +
// TPREVERSION.
//
// Legacy parity: legacy TPatchSvr did not validate — it relied on
// SQL errors surfacing on the first client request. This is a new
// safety net, paralleling the validator already running on
// TLoginSvrAsio.

#include "schema_validator.h"

#include "fourstory/db/schema_validator.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <string>

namespace tpatchsvr::db {

namespace {

// Existence probe for the optional TUSER_INTERFACE table. Returns
// true when the table is present with the columns the repository
// queries. Soft-fails to false on any error (driver mismatch,
// permission denied, …) so we report a warning rather than aborting.
bool HasUserInterfaceTable(soci::session& sql)
{
    int hits = 0;
    try
    {
        sql << "SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
               "WHERE TABLE_NAME = 'TUSER_INTERFACE' "
               "AND COLUMN_NAME IN ('bOption','szName','dwSize')",
            soci::into(hits);
    }
    catch (const std::exception& ex)
    {
        spdlog::debug("schema_validator (patch_global): TUSER_INTERFACE "
                      "probe skipped: {}", ex.what());
        return false;
    }
    return hits == 3;
}

} // namespace

void ValidateGlobalSchema(fourstory::db::SessionPool& pool)
{
    auto lease = pool.Acquire();

    // Required: TVERSION + TPREVERSION. Column list matches exactly
    // what PatchRepository::List{Patches,PrePatches}Since reads.
    fourstory::db::CheckColumns(*lease, "patch_global", {
        { "TVERSION",    "dwVersion" },
        { "TVERSION",    "szPath" },
        { "TVERSION",    "szName" },
        { "TVERSION",    "dwSize" },
        { "TVERSION",    "dwBetaVer" },
        { "TPREVERSION", "dwBetaVer" },
        { "TPREVERSION", "szPath" },
        { "TPREVERSION", "szName" },
        { "TPREVERSION", "dwSize" },
    });

    // Optional: TUSER_INTERFACE — warn if missing so operators
    // notice, but don't refuse to start. CT_CHANGEIF clients on
    // such a deploy just see 0 UI files.
    if (!HasUserInterfaceTable(*lease))
    {
        spdlog::warn("schema_validator (patch_global): TUSER_INTERFACE "
                     "not deployed — CT_CHANGEIF will return 0 files. "
                     "Apply schema/patch-tables.sql to enable UI-file "
                     "overrides.");
    }
}

} // namespace tpatchsvr::db
