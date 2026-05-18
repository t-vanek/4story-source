#pragma once

// Schema validator. Run once at startup against each SessionPool to
// confirm the expected legacy tables exist before accepting traffic.
// Fail-fast on mismatch is preferred over discovering it on the first
// CS_LOGIN_REQ — the alternative is a 500-equivalent for the first
// real user.
//
// The check is column-level: confirm each table exists AND has the
// columns the SOCI services query by name. Missing rows in the table
// are fine; we don't sample data. Spurious columns are fine too —
// only "service expects X but X doesn't exist" is fatal.

#include <stdexcept>
#include <string>

namespace tloginsvr::db {

class SessionPool;

// SchemaError describes a missing table or column found by the
// validator. Caller throws on first error; we ship it as a typed
// exception so main() can format the message consistently.
struct SchemaError : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

// Validate the TGLOBAL schema. Throws SchemaError on missing
// table/column. Quiet on success — caller logs at info level.
void ValidateGlobalSchema(SessionPool& pool);

// Validate the TGAME schema (world DB). Throws SchemaError on
// missing table/column.
void ValidateWorldSchema(SessionPool& pool);

} // namespace tloginsvr::db
