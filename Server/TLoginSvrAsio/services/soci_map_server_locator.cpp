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
// Shard-override IDs — NetCode.h:143/146.
constexpr int kBowServerId  = 30;  // Battle of Worlds
constexpr int kBrServerId   = 50;  // Battle Royale

// True if (user_id, char_id) is registered in the named shard table.
// `table` is interpolated directly (no user input → no injection risk).
bool IsInShard(soci::session& sql,
               const char* table,
               int user_id,
               int char_id)
{
    int hits = 0;
    std::string q = std::string("SELECT COUNT(*) FROM \"") + table +
        "\" WHERE \"dwUserID\" = :u AND \"dwCharID\" = :c";
    sql << q, soci::use(user_id), soci::use(char_id), soci::into(hits);
    return hits > 0;
}

} // namespace

SociMapServerLocator::SociMapServerLocator(db::SessionPool& pool)
    : m_pool(pool)
{
}

std::optional<MapEndpoint>
SociMapServerLocator::Lookup(std::int32_t user_id,
                             std::uint8_t group_id,
                             std::uint8_t /*channel*/,
                             std::int32_t char_id)
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    try
    {
        // BR / BOW shard membership — overrides the routing target.
        // BR takes precedence over BOW per legacy CSHandler ordering
        // (BR check runs after BOW and unconditionally overrides the
        // server_id; we collapse it here into a single decision).
        std::optional<int> shard_override;
        if (user_id != 0 && char_id != 0)
        {
            if (IsInShard(sql, "TBRPLAYERTABLE", user_id, char_id))
            {
                shard_override = kBrServerId;
            }
            else if (IsInShard(sql, "TBOWPLAYERTABLE", user_id, char_id))
            {
                shard_override = kBowServerId;
            }
        }

        int port = 0;
        int server_id = 0;
        std::string ip;
        soci::indicator ip_ind = soci::i_null;
        bool got_row = false;
        // Dialect: PG/SQLite use trailing LIMIT 1, MSSQL uses TOP 1
        // immediately after SELECT.
        const bool is_mssql = (m_pool.GetBackend() == db::Backend::Odbc);

        // Two query shapes per dialect — with and without the shard-id
        // filter. The shard path pins server_id; default path takes
        // the first map server in the group (the per-character routing
        // currently isn't wired, see header doc).
        const char* query;
        if (shard_override.has_value())
        {
            query = is_mssql
                ? "SELECT TOP 1 s.\"wPort\", s.\"bServerID\", i.\"szIPAddr\" "
                  "FROM \"TSERVER\" s "
                  "JOIN \"TIPADDR\" i "
                  "  ON i.\"bMachineID\" = s.\"bMachineID\" "
                  " AND i.\"bActive\" = 1 "
                  "WHERE s.\"bGroupID\" = :g AND s.\"bType\" = :t "
                  "  AND s.\"bServerID\" = :sid"
                : "SELECT s.\"wPort\", s.\"bServerID\", i.\"szIPAddr\" "
                  "FROM \"TSERVER\" s "
                  "JOIN \"TIPADDR\" i "
                  "  ON i.\"bMachineID\" = s.\"bMachineID\" "
                  " AND i.\"bActive\" = 1 "
                  "WHERE s.\"bGroupID\" = :g AND s.\"bType\" = :t "
                  "  AND s.\"bServerID\" = :sid "
                  "LIMIT 1";
        }
        else
        {
            query = is_mssql
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
        }
        {
            // Scope the statement so the cursor closes before this
            // function returns / any other query runs on the connection —
            // ODBC/MSSQL semantics require it; PG is unaffected.
            const int shard_sid = shard_override.value_or(0);
            soci::statement st = shard_override.has_value()
                ? (sql.prepare << query,
                    soci::use(static_cast<int>(group_id)),
                    soci::use(kServerTypeMap),
                    soci::use(shard_sid),
                    soci::into(port),
                    soci::into(server_id),
                    soci::into(ip, ip_ind))
                : (sql.prepare << query,
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
            if (shard_override.has_value())
            {
                spdlog::warn("map_locator: shard override server_id={} "
                             "configured for char_id={} but no TSERVER row "
                             "matches in group={}",
                    *shard_override, char_id, static_cast<int>(group_id));
            }
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

namespace {

// Map TGROUP.bStatus + per-group current-user count → wire-level
// GroupStatus per legacy CSHandler.cpp:524. The legacy code uses
// COUNT(*) FROM TCURRENTUSER joined on bGroupID; we replicate that
// here. The COUNT is computed inside the same SELECT to keep the
// service single-query.
GroupStatus ResolveStatus(int row_status,    // TGROUP.bStatus
                          int current_count, // live TCURRENTUSER count
                          int busy_threshold,
                          int full_threshold)
{
    if (row_status == 2 /* TSVR_STATUS_SLEEP */) return GroupStatus::Sleep;
    if (current_count > full_threshold)          return GroupStatus::Full;
    if (current_count > busy_threshold)          return GroupStatus::Busy;
    return GroupStatus::Normal;
}

} // namespace

std::vector<GroupInfo>
SociMapServerLocator::ListGroups(std::int32_t /*user_id*/)
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    try
    {
        // Join with TCURRENTUSER for the live count so the client sees
        // accurate busy/full status. LEFT JOIN + COALESCE so empty
        // groups still show up with count=0.
        std::vector<GroupInfo> out;
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT g.\"bGroupID\", g.\"bType\", g.\"szNAME\", "
            "       g.\"bStatus\", g.\"wBusy\", g.\"wFull\", g.\"bUseRate\", "
            "       COALESCE(("
            "         SELECT COUNT(*) FROM \"TCURRENTUSER\" cu "
            "         WHERE cu.\"bGroupID\" = g.\"bGroupID\""
            "       ), 0) AS curcount "
            "FROM \"TGROUP\" g "
            "ORDER BY g.\"bGroupID\"");
        for (const auto& r : rs)
        {
            GroupInfo info{};
            info.group_id = static_cast<std::uint8_t>(r.get<int>(0));
            info.type     = static_cast<std::uint8_t>(r.get<int>(1));
            info.name     = r.get<std::string>(2);
            const int status_raw = r.get<int>(3);
            const int busy_th    = r.get<int>(4);
            const int full_th    = r.get<int>(5);
            info.flags    = static_cast<std::uint8_t>(r.get<int>(6));
            // COALESCE on COUNT(*) — SOCI surfaces it as long long on PG,
            // int on MSSQL. Read as long long to be safe and narrow.
            long long curcount = 0;
            try { curcount = r.get<long long>(7); }
            catch (...) { curcount = r.get<int>(7); }
            info.status   = ResolveStatus(
                status_raw, static_cast<int>(curcount), busy_th, full_th);
            out.push_back(std::move(info));
        }
        return out;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("map_locator.ListGroups DB error: {}", ex.what());
        return {};
    }
}

std::vector<ChannelInfo>
SociMapServerLocator::ListChannels(std::uint8_t group_id)
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    try
    {
        std::vector<ChannelInfo> out;
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT c.\"bChannel\", c.\"szNAME\", c.\"bStatus\", "
            "       c.\"wBusy\", c.\"wFull\", "
            "       COALESCE(("
            "         SELECT COUNT(*) FROM \"TCURRENTUSER\" cu "
            "         WHERE cu.\"bGroupID\" = c.\"bGroupID\" "
            "           AND cu.\"bChannel\"  = c.\"bChannel\""
            "       ), 0) AS curcount "
            "FROM \"TCHANNEL\" c "
            "WHERE c.\"bGroupID\" = :g "
            "ORDER BY c.\"bChannel\"",
            soci::use(static_cast<int>(group_id)));
        for (const auto& r : rs)
        {
            ChannelInfo info{};
            info.channel = static_cast<std::uint8_t>(r.get<int>(0));
            info.name    = r.get<std::string>(1);
            const int status_raw = r.get<int>(2);
            const int busy_th    = r.get<int>(3);
            const int full_th    = r.get<int>(4);
            long long curcount = 0;
            try { curcount = r.get<long long>(5); }
            catch (...) { curcount = r.get<int>(5); }
            info.status  = ResolveStatus(
                status_raw, static_cast<int>(curcount), busy_th, full_th);
            out.push_back(std::move(info));
        }
        return out;
    }
    catch (const std::exception& ex)
    {
        spdlog::error("map_locator.ListChannels(group={}) DB error: {}",
            static_cast<int>(group_id), ex.what());
        return {};
    }
}

} // namespace tloginsvr::services
