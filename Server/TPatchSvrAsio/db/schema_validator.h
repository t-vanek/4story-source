#pragma once

// Patch-server schema validation. Boot-time fail-fast against TGLOBAL
// to catch missing patch metadata tables (TVERSION / TPREVERSION) and
// the optional UI-files table (TUSER_INTERFACE) before the listener
// accepts traffic. Mirrors the equivalent entry point on
// TLoginSvrAsio (`tloginsvr::db::ValidateGlobalSchema`).

namespace fourstory::db { class SessionPool; }

namespace tpatchsvr::db {

// Validate the TGLOBAL schema patch-server side: TVERSION and
// TPREVERSION required, TUSER_INTERFACE checked separately and
// reported as a warning when absent (legacy deploys without UI files
// still work — CT_CHANGEIF just returns 0 files).
//
// Throws fourstory::db::SchemaError on a required-table miss.
void ValidateGlobalSchema(fourstory::db::SessionPool& pool);

} // namespace tpatchsvr::db
