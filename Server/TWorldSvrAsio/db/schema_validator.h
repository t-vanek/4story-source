#pragma once

// TWorldSvrAsio schema validation. Boot-time fail-fast against
// the world DB pool to catch missing tables (TGUILDTABLE +
// TGUILDMEMBERTABLE for W3a-1, more in later phases) before the
// listener starts accepting peer connections. Mirrors the same
// shape as `tcontrolsvr::db::ValidateGlobalSchema`.

namespace fourstory::db { class SessionPool; }

namespace tworldsvr::db {

// Throws fourstory::db::SchemaError on a required-table miss.
// Required for W3a-1: TGUILDTABLE + TGUILDMEMBERTABLE columns the
// SociGuildRepository reads. Optional auxiliary tables checked at
// W3+/W4+ scope are warned about rather than aborted so a dev DB
// without those migrations can still boot the partial world
// binary.
void ValidateWorldSchema(fourstory::db::SessionPool& pool);

} // namespace tworldsvr::db
