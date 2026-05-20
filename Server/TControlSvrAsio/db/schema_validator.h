#pragma once

// Control-server schema validation. Boot-time fail-fast against
// TGLOBAL_RAGEZONE to catch missing inventory tables
// (TMACHINE / TGROUP / TSVRTYPE / TSERVER / TIPADDR) before the
// listener accepts traffic. Mirrors the equivalent entry point on
// TLoginSvrAsio and TPatchSvrAsio.

namespace fourstory::db { class SessionPool; }

namespace tcontrolsvr::db {

// Throws fourstory::db::SchemaError on a required-table miss.
// Optional tables (TEVENTCHART, TCASHSHOPITEMCHART, TPREVERSION) are
// checked separately and reported as a warning when absent so a
// minimal dev DB can still boot the F2 inventory path.
void ValidateGlobalSchema(fourstory::db::SessionPool& pool);

} // namespace tcontrolsvr::db
