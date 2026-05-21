#pragma once

// SpCall — fluent stored-procedure call builder for MSSQL / SOCI.
//
// Builds a T-SQL DECLARE + EXEC + SELECT block at call time, executes
// it against a SOCI session, and wraps the results in SpResult for
// typed retrieval.
//
// Dialect: MSSQL T-SQL (ODBC backend).  Escape sequences for string
// values follow SQL standard: single-quote → double single-quote.
//
// Usage:
//
//   auto result = fourstory::db::orm::SpCall("TLogin")
//       .In("szUserID",  user_id)
//       .In("szPasswd",  password)
//       .In("szLoginIP", ip)
//       .In("bIPCheck",  1)
//       .Out<int>("dwKEY")
//       .Out<int>("dwUserID")
//       .Out<std::string>("szIPAddr")
//       .WithReturn()
//       .Execute(sql);
//
//   if (result.ReturnCode() == 0) {
//       auto key = result.Out<int>("dwKEY");
//       auto uid = result.Out<int>("dwUserID");
//   }

#include <any>
#include <cstdio>
#include <ctime>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace soci { class session; }

namespace fourstory::db::orm {

// ── SpResult ────────────────────────────────────────────────────────
// Holds the OUT parameters and return code from a completed SpCall.

class SpResult
{
public:
    bool Ok() const { return m_ok; }

    // EXEC return code (0 = success in legacy SPs).
    long long ReturnCode() const
    {
        return m_return.value_or(-1);
    }

    // Typed OUT-parameter getter. T must match what was declared in
    // Out<T>(). Throws std::bad_any_cast on type mismatch.
    template<typename T>
    T Out(const std::string& name) const
    {
        const auto it = m_values.find(name);
        if (it == m_values.end())
            throw std::out_of_range("SpResult::Out: unknown param '" + name + "'");
        return std::any_cast<T>(it->second);
    }

    template<typename T>
    std::optional<T> TryOut(const std::string& name) const
    {
        const auto it = m_values.find(name);
        if (it == m_values.end()) return std::nullopt;
        try { return std::any_cast<T>(it->second); }
        catch (...) { return std::nullopt; }
    }

private:
    friend class SpCall;

    bool                                         m_ok     = false;
    std::optional<long long>                     m_return;
    std::unordered_map<std::string, std::any>    m_values;
};

// ── SpCall ───────────────────────────────────────────────────────────

class SpCall
{
public:
    explicit SpCall(std::string sp_name)
        : m_name(std::move(sp_name))
    {}

    // ── IN parameters ───────────────────────────────────────────────

    SpCall& In(std::string name, int val)
    {
        m_in.push_back({std::move(name), std::to_string(val)});
        return *this;
    }
    SpCall& In(std::string name, long long val)
    {
        m_in.push_back({std::move(name), std::to_string(val)});
        return *this;
    }
    SpCall& In(std::string name, unsigned int val)
    {
        m_in.push_back({std::move(name), std::to_string(val)});
        return *this;
    }
    SpCall& In(std::string name, unsigned long long val)
    {
        m_in.push_back({std::move(name), std::to_string(val)});
        return *this;
    }
    SpCall& In(std::string name, double val)
    {
        m_in.push_back({std::move(name), std::to_string(val)});
        return *this;
    }
    SpCall& In(std::string name, const std::string& val)
    {
        m_in.push_back({std::move(name), "N'" + EscapeStr(val) + "'"});
        return *this;
    }
    SpCall& In(std::string name, const char* val)
    {
        return In(std::move(name), std::string(val ? val : ""));
    }
    SpCall& In(std::string name, bool val)
    {
        m_in.push_back({std::move(name), val ? "1" : "0"});
        return *this;
    }

    // std::tm → SQL DATETIME literal in ISO-like form. MSSQL accepts
    // 'YYYY-MM-DD hh:mm:ss' as an implicit DATETIME conversion.
    SpCall& In(std::string name, const std::tm& tm)
    {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "'%04d-%02d-%02d %02d:%02d:%02d'",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
        m_in.push_back({std::move(name), buf});
        return *this;
    }

    // ── OUT parameters (type-tagged) ─────────────────────────────────
    // The template parameter T determines the SQL type and the type
    // returned by SpResult::Out<T>(name).

    template<typename T>
    SpCall& Out(std::string name)
    {
        m_out.push_back({std::move(name), SqlTypeFor<T>(), OutKind::Regular});
        return *this;
    }

    // ── RETURN value ──────────────────────────────────────────────────
    SpCall& WithReturn()
    {
        m_want_return = true;
        return *this;
    }

    // ── Execute ───────────────────────────────────────────────────────
    SpResult Execute(soci::session& sql) const;

private:
    // SQL type string for each C++ type used in Out<T>().
    template<typename T> static constexpr const char* SqlTypeFor();

    // Standard SQL single-quote escape.
    static std::string EscapeStr(const std::string& s)
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

    std::string BuildSql() const;

    struct InParam  { std::string name; std::string sql_literal; };
    enum class OutKind { Regular };
    struct OutParam { std::string name; std::string sql_type; OutKind kind; };

    std::string            m_name;
    std::vector<InParam>   m_in;
    std::vector<OutParam>  m_out;
    bool                   m_want_return = false;
};

// ── SqlTypeFor specialisations ────────────────────────────────────────

template<> inline constexpr const char* SpCall::SqlTypeFor<int>()
    { return "INT"; }
template<> inline constexpr const char* SpCall::SqlTypeFor<long long>()
    { return "BIGINT"; }
template<> inline constexpr const char* SpCall::SqlTypeFor<unsigned int>()
    { return "INT"; }
template<> inline constexpr const char* SpCall::SqlTypeFor<unsigned long long>()
    { return "BIGINT"; }
template<> inline constexpr const char* SpCall::SqlTypeFor<short>()
    { return "SMALLINT"; }
template<> inline constexpr const char* SpCall::SqlTypeFor<unsigned short>()
    { return "SMALLINT"; }
template<> inline constexpr const char* SpCall::SqlTypeFor<double>()
    { return "FLOAT"; }
template<> inline constexpr const char* SpCall::SqlTypeFor<float>()
    { return "REAL"; }
template<> inline constexpr const char* SpCall::SqlTypeFor<std::string>()
    { return "VARCHAR(512)"; }
template<> inline constexpr const char* SpCall::SqlTypeFor<bool>()
    { return "BIT"; }

} // namespace fourstory::db::orm
