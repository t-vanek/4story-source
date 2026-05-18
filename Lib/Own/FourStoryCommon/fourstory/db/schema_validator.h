#pragma once

// Schema validator framework — generic CheckColumns helper that each
// server uses to verify its expected tables/columns exist before
// accepting traffic. Per-server validator entry points live in the
// server's own db/ directory; this header carries the shared
// framework only.
//
// The check is column-level: confirm each table exists AND has the
// columns the SOCI services query by name. Missing rows in the table
// are fine; we don't sample data. Spurious columns are fine too —
// only "service expects X but X doesn't exist" is fatal.

#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>

namespace soci { class session; }

namespace fourstory::db {

// SchemaError describes a missing table or column found by the
// validator. Caller throws on first error; we ship it as a typed
// exception so main() can format the message consistently.
struct SchemaError : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

// Verify that every (table, column) listed exists in the connected
// database. Throws SchemaError listing missing entries on first
// mismatch. `pool_label` appears in the exception message + info log.
//
// Table + column names are inlined into the query string (no
// parameter binding) because the standard INFORMATION_SCHEMA parameter
// path fails on some ODBC/MSSQL driver combinations — the columns
// are typed as `sysname` (NVARCHAR) and SOCI's default VARCHAR
// binding compares as "no row" silently. All inputs come from
// compile-time constant lists, so direct substitution is safe.
void CheckColumns(soci::session& sql,
                  const char* pool_label,
                  std::initializer_list<std::pair<const char*, const char*>> required);

} // namespace fourstory::db
