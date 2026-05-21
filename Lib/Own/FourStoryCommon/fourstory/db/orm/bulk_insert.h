#pragma once

// BulkInsert<T> — true multi-row VALUES INSERT for hot paths where
// per-row overhead matters (TLogSvr drain, SaveInventory on logout).
//
// Generates a single statement of the form:
//
//   INSERT INTO Table (col1, col2, col3) VALUES
//     (val1, val2, val3),
//     (val1, val2, val3),
//     ...;
//
// Values are embedded directly with SQL-escape rules (single-quote
// doubling for strings). Safe for internal cluster traffic where the
// data originates server-side; do NOT use with attacker-supplied
// payload without re-validating the escape coverage in BulkRowFormatter.
//
// Opt-in: the entity author specialises BulkRowFormatter<T> with two
// statics:
//
//   template<>
//   struct fourstory::db::orm::BulkRowFormatter<MyEntity> {
//       // Column list — first part of the INSERT statement.
//       static constexpr const char* Columns = "(col1, col2, col3)";
//       // Append one "(v1, v2, v3)" tuple to `sql` from `entity`.
//       static void Append(std::string& sql, const MyEntity& e) {
//           sql += "(";
//           sql += std::to_string(e.col1);
//           sql += ", N'";
//           sql += BulkEscape(e.col2);
//           sql += "', ";
//           sql += std::to_string(e.col3);
//           sql += ")";
//       }
//   };
//
// Then call `BulkInsert<MyEntity>::Execute(sql, rows)` directly OR via
// `Repository<MyEntity>::BulkInsert(rows)` when that wiring lands.
//
// Batching: BulkInsert chunks rows into `batch_size` groups (default
// 500) so the per-statement SQL stays under MSSQL's max-batch-size
// limit (~64 MB) and parameter cap (2100). Each batch is one wire
// round-trip.

#include "entity_mapping.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace fourstory::db::orm {

// Standard SQL single-quote escape — '  →  ''. Use inside string
// literals when building the (v1, v2, ...) row in BulkRowFormatter.
inline std::string BulkEscape(std::string_view s)
{
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s)
    {
        if (c == '\'') out += "''";
        else           out += c;
    }
    return out;
}

// Primary template — opting in requires specialising this. The
// unspecialised case is a compile-time error so accidental use of
// BulkInsert<T> for an entity without a formatter fails loudly.
template<typename T>
struct BulkRowFormatter
{
    static_assert(sizeof(T) == 0,
        "fourstory::db::orm::BulkRowFormatter<T> must be specialised for "
        "this entity. See bulk_insert.h for the required statics "
        "(Columns + Append).");
};

template<typename T>
class BulkInsert
{
    using Map = EntityMapping<T>;
    using Fmt = BulkRowFormatter<T>;

public:
    // Build + execute the multi-row INSERT against `sql`. Chunks `rows`
    // into batches of `batch_size`; each batch is one wire round-trip
    // wrapped in its own transaction (no atomicity across batches —
    // callers needing all-or-nothing should pass batch_size >= rows.size()
    // or wrap externally).
    //
    // Returns the number of rows inserted on success. Throws and rolls
    // back the in-flight batch on any SQL error.
    static std::size_t Execute(soci::session& sql,
                                const std::vector<T>& rows,
                                std::size_t batch_size = 500)
    {
        if (rows.empty()) return 0;
        if (batch_size == 0) batch_size = rows.size();

        std::size_t inserted = 0;
        for (std::size_t i = 0; i < rows.size(); i += batch_size)
        {
            const std::size_t end = std::min(i + batch_size, rows.size());
            std::string stmt;
            stmt.reserve(64 + (end - i) * 64);
            stmt += "INSERT INTO ";
            stmt += Map::Table;
            stmt += ' ';
            stmt += Fmt::Columns;
            stmt += " VALUES ";
            for (std::size_t j = i; j < end; ++j)
            {
                if (j > i) stmt += ", ";
                Fmt::Append(stmt, rows[j]);
            }

            soci::transaction tx(sql);
            try
            {
                sql << stmt;
                tx.commit();
                inserted += (end - i);
            }
            catch (const std::exception& ex)
            {
                try { tx.rollback(); } catch (...) {}
                spdlog::error("BulkInsert<{}>: batch [{},{}) failed: {}",
                    Map::Table, i, end, ex.what());
                throw;
            }
        }
        return inserted;
    }
};

} // namespace fourstory::db::orm
