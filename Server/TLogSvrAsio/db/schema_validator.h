#pragma once

// Log-server schema validation. Boot-time fail-fast against the audit
// pool so a typo in `target_table` (or a fresh DB that never had the
// audit DDL applied) surfaces immediately, rather than every UDP
// packet failing its INSERT and the operator only noticing because
// nothing lands in TLOG_AUDIT.
//
// Mirrors the equivalent entry points on TLoginSvrAsio
// (`tloginsvr::db::ValidateGlobalSchema`) and TPatchSvrAsio
// (`tpatchsvr::db::ValidateGlobalSchema`).

#include <string>

namespace fourstory::db { class SessionPool; }

namespace tlogsvr::db {

// Verify the configured audit table (default `TLOG_AUDIT`) carries
// every LT_* column the SociLogSink INSERT binds. Column list is
// pinned to schema/tlog-audit.sql.
//
// Throws fourstory::db::SchemaError on a missing column. The thrown
// message names every missing entry so an operator running a partial
// or older audit DDL sees exactly what to migrate.
void ValidateAuditSchema(fourstory::db::SessionPool& pool,
                         const std::string& target_table);

} // namespace tlogsvr::db
