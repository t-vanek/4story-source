#include "fourstory/db/schema_validator.h"

#include <soci/soci.h>

#include <spdlog/spdlog.h>

#include <string>
#include <vector>

namespace fourstory::db {

void CheckColumns(soci::session& sql,
                  const char* pool_label,
                  std::initializer_list<std::pair<const char*, const char*>> required)
{
    std::vector<std::string> missing;
    for (const auto& [table, column] : required)
    {
        int hits = 0;
        try
        {
            if (sql.get_backend_name() == "postgresql")
            {
                // Resolve precisely the relation that unqualified repository
                // SQL will use; a same-named historical table is not sufficient.
                const std::string relation = std::string("\"") + table + "\"";
                const std::string col = column;
                sql << "SELECT count(*) FROM pg_catalog.pg_attribute "
                       "WHERE attrelid=pg_catalog.to_regclass(:relation) "
                       "AND attname=:column AND attnum>0 AND NOT attisdropped",
                    soci::use(relation), soci::use(col), soci::into(hits);
            }
            else
            {
            std::string q =
                std::string("SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                            "WHERE TABLE_NAME = '") + table +
                "' AND COLUMN_NAME = '" + column + "'";
            sql << q, soci::into(hits);
            }
        }
        catch (const std::exception& ex)
        {
            throw SchemaError(std::string("schema_validator (") + pool_label +
                "): INFORMATION_SCHEMA query failed: " + ex.what());
        }
        if (hits == 0)
        {
            missing.emplace_back(std::string(table) + "." + column);
        }
    }
    if (!missing.empty())
    {
        std::string msg = std::string("schema_validator (") + pool_label +
            "): missing column(s):";
        for (const auto& m : missing) { msg += ' '; msg += m; }
        throw SchemaError(msg);
    }
    spdlog::info("schema_validator ({}) OK ({} columns checked)",
        pool_label, required.size());
}

} // namespace fourstory::db
