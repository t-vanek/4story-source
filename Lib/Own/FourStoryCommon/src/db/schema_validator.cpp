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
            std::string q =
                std::string("SELECT COUNT(*) FROM INFORMATION_SCHEMA.COLUMNS "
                            "WHERE TABLE_NAME = '") + table +
                "' AND COLUMN_NAME = '" + column + "'";
            sql << q, soci::into(hits);
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
