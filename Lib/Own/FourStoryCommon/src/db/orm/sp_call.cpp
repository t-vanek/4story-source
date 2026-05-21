#include "fourstory/db/orm/sp_call.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <sstream>
#include <stdexcept>

namespace fourstory::db::orm {

// ── T-SQL generator ──────────────────────────────────────────────────
// Produces a single multi-statement string:
//
//   DECLARE @_ret INT;                     -- if WithReturn()
//   DECLARE @_OUT_name1 TYPE1;             -- per Out<T>(name)
//   ...
//   EXEC @_ret = SpName                    -- EXEC [+ return capture]
//       @in1 = literal1,                   -- IN params (embedded literals)
//       @out1 = @_OUT_name1 OUTPUT,        -- OUT params
//       ...;
//   SELECT @_ret, @_OUT_name1, ...;        -- read results

std::string SpCall::BuildSql() const
{
    std::ostringstream ss;

    // DECLARE phase
    if (m_want_return)
        ss << "DECLARE @_ret INT;\n";
    for (const auto& op : m_out)
        ss << "DECLARE @_OUT_" << op.name << ' ' << op.sql_type << ";\n";

    // EXEC phase
    ss << "EXEC ";
    if (m_want_return)
        ss << "@_ret = ";
    ss << m_name;

    bool first = true;
    auto sep = [&] { ss << (first ? "\n    " : ",\n    "); first = false; };

    for (const auto& ip : m_in)
    {
        sep();
        ss << '@' << ip.name << " = " << ip.sql_literal;
    }
    for (const auto& op : m_out)
    {
        sep();
        ss << '@' << op.name << " = @_OUT_" << op.name << " OUTPUT";
    }
    ss << ";\n";

    // SELECT phase — collect all results in one row
    if (m_want_return || !m_out.empty())
    {
        ss << "SELECT ";
        bool sfirst = true;
        auto ssep = [&] { if (!sfirst) ss << ", "; sfirst = false; };
        if (m_want_return) { ssep(); ss << "@_ret"; }
        for (const auto& op : m_out) { ssep(); ss << "@_OUT_" << op.name; }
        ss << ";";
    }

    return ss.str();
}

// ── Execute ───────────────────────────────────────────────────────────

SpResult SpCall::Execute(soci::session& sql) const
{
    SpResult res;
    const std::string query = BuildSql();

    spdlog::debug("SpCall::Execute '{}' sql={}", m_name, query);

    try
    {
        if (!m_want_return && m_out.empty())
        {
            // Pure IN-only SP — just execute, no results to collect.
            sql << query;
            res.m_ok = true;
            return res;
        }

        // The SELECT gives one row: [@_ret,] [@_OUT_1, @_OUT_2, ...]
        soci::rowset<soci::row> rs =
            (sql.prepare << query);

        for (const auto& row : rs)
        {
            std::size_t col = 0;
            if (m_want_return)
            {
                // @_ret is always INT
                res.m_return = static_cast<long long>(row.get<int>(col));
                ++col;
            }
            for (const auto& op : m_out)
            {
                // Read based on declared SQL type (first 3 chars sufficient)
                const std::string& t = op.sql_type;
                if (t == "INT" || t == "BIT" || t.rfind("INT", 0) == 0 ||
                    t.rfind("TINY", 0) == 0 || t.rfind("SMALL", 0) == 0)
                {
                    soci::indicator ind = soci::i_null;
                    int v = 0;
                    // rowset<soci::row> uses get<T> or get<T>(col)
                    ind = row.get_indicator(col);
                    if (ind != soci::i_null)
                        v = row.get<int>(col);
                    res.m_values[op.name] = v;
                }
                else if (t == "BIGINT")
                {
                    long long v = 0;
                    if (row.get_indicator(col) != soci::i_null)
                        v = row.get<long long>(col);
                    res.m_values[op.name] = v;
                }
                else if (t == "FLOAT" || t == "REAL")
                {
                    double v = 0.0;
                    if (row.get_indicator(col) != soci::i_null)
                        v = row.get<double>(col);
                    res.m_values[op.name] = v;
                }
                else  // VARCHAR / NVARCHAR / CHAR / TEXT
                {
                    std::string v;
                    if (row.get_indicator(col) != soci::i_null)
                        v = row.get<std::string>(col);
                    res.m_values[op.name] = v;
                }
                ++col;
            }
            break; // only the first row
        }
        res.m_ok = true;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("SpCall::Execute '{}' failed: {}\n  sql={}",
            m_name, ex.what(), query);
    }
    return res;
}

} // namespace fourstory::db::orm
