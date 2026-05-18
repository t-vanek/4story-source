#include "soci_map_server_locator.h"
#include "../db/session_pool.h"

#include <soci/soci.h>

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <sstream>

namespace tloginsvr::services {

namespace {

// Parse a dotted IPv4 like "192.168.1.42" into 4 octets. The four-arg
// `sscanf %d` doesn't reject "300.0.0.0"-style garbage; range-check
// each value below. Returns nullopt on any parse / range failure so
// the caller can log + treat as "no endpoint available."
std::optional<std::array<std::uint8_t, 4>>
ParseIPv4(const std::string& s)
{
    int a = 0, b = 0, c = 0, d = 0;
    char extra = 0;
    if (std::sscanf(s.c_str(), "%d.%d.%d.%d%c", &a, &b, &c, &d, &extra) != 4)
        return std::nullopt;
    if (a < 0 || a > 255 || b < 0 || b > 255 ||
        c < 0 || c > 255 || d < 0 || d > 255)
        return std::nullopt;
    return std::array<std::uint8_t, 4>{
        static_cast<std::uint8_t>(a),
        static_cast<std::uint8_t>(b),
        static_cast<std::uint8_t>(c),
        static_cast<std::uint8_t>(d),
    };
}

// Legacy SVRGRP_MAPSVR (see Server/TLoginSvr NetCode.h).
constexpr int kServerTypeMap = 4;

} // namespace

SociMapServerLocator::SociMapServerLocator(db::SessionPool& pool)
    : m_pool(pool)
{
}

std::optional<MapEndpoint>
SociMapServerLocator::Lookup(std::uint8_t group_id,
                             std::uint8_t /*channel*/,
                             std::int32_t /*char_id*/)
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    try
    {
        int port = 0;
        int server_id = 0;
        std::string ip;
        soci::indicator ip_ind = soci::i_null;
        bool got_row = false;
        // Dialect: PG/SQLite use trailing LIMIT 1, MSSQL uses TOP 1
        // immediately after SELECT.
        const bool is_mssql = (m_pool.GetBackend() == db::Backend::Odbc);
        const char* query = is_mssql
            ? "SELECT TOP 1 s.\"wPort\", s.\"bServerID\", i.\"szIPAddr\" "
              "FROM \"TSERVER\" s "
              "JOIN \"TIPADDR\" i "
              "  ON i.\"bMachineID\" = s.\"bMachineID\" "
              " AND i.\"bActive\" = 1 "
              "WHERE s.\"bGroupID\" = :g AND s.\"bType\" = :t "
              "ORDER BY s.\"bServerID\""
            : "SELECT s.\"wPort\", s.\"bServerID\", i.\"szIPAddr\" "
              "FROM \"TSERVER\" s "
              "JOIN \"TIPADDR\" i "
              "  ON i.\"bMachineID\" = s.\"bMachineID\" "
              " AND i.\"bActive\" = 1 "
              "WHERE s.\"bGroupID\" = :g AND s.\"bType\" = :t "
              "ORDER BY s.\"bServerID\" "
              "LIMIT 1";
        {
            // Scope the statement so the cursor closes before this
            // function returns / any other query runs on the connection —
            // ODBC/MSSQL semantics require it; PG is unaffected.
            soci::statement st = (sql.prepare << query,
                soci::use(static_cast<int>(group_id)),
                soci::use(kServerTypeMap),
                soci::into(port),
                soci::into(server_id),
                soci::into(ip, ip_ind));
            st.execute(true);
            got_row = st.got_data();
        }
        if (!got_row || ip_ind == soci::i_null)
        {
            return std::nullopt;
        }

        const auto octets = ParseIPv4(ip);
        if (!octets)
        {
            spdlog::warn("map_locator: group={} TIPADDR.szIPAddr='{}' "
                         "unparseable as IPv4",
                static_cast<int>(group_id), ip);
            return std::nullopt;
        }

        MapEndpoint ep{};
        ep.ipv4      = *octets;
        ep.port      = static_cast<std::uint16_t>(port);
        ep.server_id = static_cast<std::uint8_t>(server_id);
        return ep;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("map_locator.Lookup(group={}) DB error: {}",
            static_cast<int>(group_id), ex.what());
        return std::nullopt;
    }
}

} // namespace tloginsvr::services
