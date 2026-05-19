#include "soci_map_server_locator.h"
#include "fourstory/db/session_pool.h"

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

// Port of TFindServerID (Server/TLoginSvr DB plan, TGAME.dbo.TFindServerID).
// Resolves the Map server hosting a character's current map zone.
//
//   1. Read char's wMapID + (fPosX, fPosZ) + wSpawnID from TCHARTABLE
//   2. Compute spatial wUnitID = trunc(fPosZ/1024)*256 + trunc(fPosX/1024)
//   3. JOIN TSVRCHART + TCHANNELCHART for (group, mapID, unitID, channel) → bServerID
//   4. If no row, fall back to TSPAWNPOSCHART by wSpawnID — char is in a
//      zone with no server; the legacy SP repositions them to the spawn
//      point first.
//   5. Return resolved bServerID, or nullopt if every lookup misses.
//
// Step 4's "stamp wMapID + fPosX/fPosZ on the char row" side-effect from
// the legacy SP is intentionally NOT replicated — that's gameplay state
// the Map server owns; the legacy bundled it into the routing SP to
// avoid an extra DB hit, but in our split Map server handles
// repositioning on EnterMap, and the login server has no business
// mutating the char row.
std::optional<int> FindServerForChar(soci::session& sql,
                                     int char_id,
                                     int group_id,
                                     int channel)
{
    // SOCI's exchange-traits supports double + long long + int +
    // std::string. `float` from TCHARTABLE's `real` column reads
    // back into a `double` cleanly on both MSSQL (FLOAT/REAL → double)
    // and PG (REAL → double).
    double pos_x = 0.0, pos_z = 0.0;
    int    map_id = 0;
    int    spawn_id = 0;
    bool   got_char = false;
    {
        soci::statement st = (sql.prepare <<
            "SELECT \"wMapID\", \"fPosX\", \"fPosZ\", \"wSpawnID\" "
            "FROM \"TCHARTABLE\" WHERE \"dwCharID\" = :c "
            "  AND \"bDelete\" = 0",
            soci::use(char_id),
            soci::into(map_id),
            soci::into(pos_x),
            soci::into(pos_z),
            soci::into(spawn_id));
        st.execute(true);
        got_char = st.got_data();
    }
    if (!got_char) return std::nullopt;

    // Spatial bucket — matches the SP's CAST(fPosZ/1024 AS SMALLINT)*256
    // + CAST(fPosX/1024 AS SMALLINT). Negative coordinates round
    // towards zero in both T-SQL CAST AS SMALLINT and C++ static_cast
    // <int16_t>, so the bucket math stays equivalent.
    const auto unit_id_for = [](double x, double z) -> int {
        return static_cast<int>(static_cast<std::int16_t>(z / 1024.0)) * 256
             + static_cast<int>(static_cast<std::int16_t>(x / 1024.0));
    };
    int unit_id = unit_id_for(pos_x, pos_z);

    auto lookup_at = [&](int mid, int uid) -> std::optional<int> {
        int sid = 0;
        bool got = false;
        {
            soci::statement st = (sql.prepare <<
                "SELECT TOP 1 s.\"bServerID\" "
                "FROM \"TSVRCHART\" s "
                "JOIN \"TCHANNELCHART\" c "
                "  ON s.\"bGroup\" = c.\"bGroupID\" "
                " AND s.\"wMapID\" = c.\"wMapID\" "
                " AND s.\"wUnitID\" = c.\"wUnitID\" "
                " AND s.\"bChannel\" = c.\"bPhyChannel\" "
                "WHERE s.\"bGroup\" = :g "
                "  AND s.\"wMapID\" = :m "
                "  AND s.\"wUnitID\" = :u "
                "  AND c.\"bLogChannel\" = :ch",
                soci::use(group_id),
                soci::use(mid),
                soci::use(uid),
                soci::use(channel),
                soci::into(sid));
            st.execute(true);
            got = st.got_data();
        }
        return got ? std::optional<int>(sid) : std::nullopt;
    };

    if (auto sid = lookup_at(map_id, unit_id)) return sid;

    // Fallback — char is in an invalid zone; repoint to their spawn.
    double spawn_x = 0.0, spawn_z = 0.0;
    int    spawn_map = 0;
    bool   got_spawn = false;
    {
        soci::statement st = (sql.prepare <<
            "SELECT \"wMapID\", \"fPosX\", \"fPosZ\" "
            "FROM \"TSPAWNPOSCHART\" WHERE \"wID\" = :s",
            soci::use(spawn_id),
            soci::into(spawn_map),
            soci::into(spawn_x),
            soci::into(spawn_z));
        st.execute(true);
        got_spawn = st.got_data();
    }
    if (!got_spawn) return std::nullopt;
    unit_id = unit_id_for(spawn_x, spawn_z);
    return lookup_at(spawn_map, unit_id);
}

} // namespace

SociMapServerLocator::SociMapServerLocator(fourstory::db::SessionPool& global_pool,
                                           fourstory::db::SessionPool* world_pool)
    : m_pool(global_pool)
    , m_world(world_pool)
{
}

std::optional<MapEndpoint>
SociMapServerLocator::Lookup(std::int32_t user_id,
                             std::uint8_t group_id,
                             std::uint8_t channel,
                             std::int32_t char_id)
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    try
    {
        // Three-tier server resolution, matching the legacy chain:
        //   1. BR / BOW shard membership (TBRPLAYERTABLE / TBOWPLAYERTABLE)
        //      pins the user to a dedicated shard for those PvP modes.
        //   2. Otherwise per-character routing via TFindServerID logic —
        //      char's current zone → bServerID. This is the legacy
        //      default flow for normal PvE chars.
        //   3. If both miss, fall through to "first map server in
        //      group" — used when char_id is 0 (group-list screen) or
        //      the char's zone has no server registered.
        std::optional<int> resolved_server_id;
        if (m_world != nullptr && user_id != 0 && char_id != 0)
        {
            auto wlease = m_world->Acquire();
            soci::session& wsql = *wlease;
            try
            {
                if (IsInShard(wsql, "TBRPLAYERTABLE", user_id, char_id))
                {
                    resolved_server_id = kBrServerId;
                }
                else if (IsInShard(wsql, "TBOWPLAYERTABLE", user_id, char_id))
                {
                    resolved_server_id = kBowServerId;
                }
            }
            catch (const std::exception& ex)
            {
                spdlog::debug("map_locator: shard-table check failed ({}) — "
                              "defaulting to no override", ex.what());
            }

            // Per-char routing if BR/BOW didn't claim them.
            if (!resolved_server_id.has_value())
            {
                try
                {
                    resolved_server_id = FindServerForChar(
                        wsql, char_id, static_cast<int>(group_id),
                        static_cast<int>(channel));
                }
                catch (const std::exception& ex)
                {
                    spdlog::warn("map_locator: per-char routing failed "
                                 "(char_id={}): {} — falling back to default",
                        char_id, ex.what());
                }
            }
        }

        int port = 0;
        int server_id = 0;
        std::string ip;
        soci::indicator ip_ind = soci::i_null;
        bool got_row = false;
        // Dialect: PG/SQLite use trailing LIMIT 1, MSSQL uses TOP 1
        // immediately after SELECT.
        const bool is_mssql = (m_pool.GetBackend() == fourstory::db::Backend::Odbc);

        // Two query shapes per dialect — with and without the shard-id
        // filter. The shard path pins server_id; default path takes
        // the first map server in the group (the per-character routing
        // currently isn't wired, see header doc).
        const char* query;
        if (resolved_server_id.has_value())
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
            const int shard_sid = resolved_server_id.value_or(0);
            soci::statement st = resolved_server_id.has_value()
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
            if (resolved_server_id.has_value())
            {
                spdlog::warn("map_locator: shard override server_id={} "
                             "configured for char_id={} but no TSERVER row "
                             "matches in group={}",
                    *resolved_server_id, char_id, static_cast<int>(group_id));
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
// GroupStatus per legacy CSHandler.cpp:524. Plus the legacy cap
// override (CSHandler.cpp:525-534): if max_user > 0 AND the user
// has no existing character in the group AND live count >= max_user,
// force the status to Full. Already-enrolled users (has_char == 1)
// bypass the cap so they can keep playing on a "full" world.
GroupStatus ResolveStatus(int row_status,    // TGROUP.bStatus
                          int current_count, // live TCURRENTUSER count
                          int busy_threshold,
                          int full_threshold,
                          int max_user,
                          int has_char)
{
    if (row_status == 2 /* TSVR_STATUS_SLEEP */) return GroupStatus::Sleep;
    if (current_count > full_threshold)          return GroupStatus::Full;
    if (max_user > 0 && has_char == 0 && current_count >= max_user)
        return GroupStatus::Full;
    if (current_count > busy_threshold)          return GroupStatus::Busy;
    return GroupStatus::Normal;
}

} // namespace

std::vector<GroupInfo>
SociMapServerLocator::ListGroups(std::int32_t user_id)
{
    auto lease = m_pool.Acquire();
    soci::session& sql = *lease;

    try
    {
        // Join with TCURRENTUSER for the live count so the client sees
        // accurate busy/full status, plus TALLCHARTABLE for the
        // per-user `has_char` flag the client uses to decorate the
        // lobby entry. LEFT JOIN + COALESCE so empty groups still
        // show up with count=0 / has_char=0.
        //
        // Legacy CTBLGroupList query (DBAccess.h:305) uses
        // `TALLCHARTABLE.dwUserID = ? AND bDelete = 0` inside the
        // join predicate; we do the same.
        std::vector<GroupInfo> out;
        soci::rowset<soci::row> rs = (sql.prepare <<
            "SELECT g.\"bGroupID\", g.\"bType\", g.\"szNAME\", "
            "       g.\"bStatus\", g.\"wBusy\", g.\"wFull\", "
            "       COALESCE(g.\"dwMaxUser\", 0) AS maxuser, "
            "       COALESCE(("
            "         SELECT COUNT(*) FROM \"TCURRENTUSER\" cu "
            "         WHERE cu.\"bGroupID\" = g.\"bGroupID\""
            "       ), 0) AS curcount, "
            "       COALESCE(("
            "         SELECT COUNT(DISTINCT ac.\"dwCharID\") "
            "         FROM \"TALLCHARTABLE\" ac "
            "         WHERE ac.\"bWorldID\" = g.\"bGroupID\" "
            "           AND ac.\"dwUserID\" = :uid "
            "           AND ac.\"bDelete\" = 0"
            "       ), 0) AS haschar "
            "FROM \"TGROUP\" g "
            "ORDER BY g.\"bGroupID\"",
            soci::use(user_id));
        for (const auto& r : rs)
        {
            GroupInfo info{};
            info.group_id = static_cast<std::uint8_t>(r.get<int>(0));
            info.type     = static_cast<std::uint8_t>(r.get<int>(1));
            info.name     = r.get<std::string>(2);
            const int status_raw = r.get<int>(3);
            const int busy_th    = r.get<int>(4);
            const int full_th    = r.get<int>(5);
            // dwMaxUser → unsigned 32-bit on the wire side. The column
            // may be NULL in older schemas (COALESCE'd to 0) — that
            // disables the override.
            long long max_user_ll = 0;
            try { max_user_ll = r.get<long long>(6); }
            catch (...) { max_user_ll = r.get<int>(6); }
            info.max_user = static_cast<std::uint32_t>(
                std::max<long long>(0, max_user_ll));
            // COALESCE on COUNT(*) — SOCI surfaces it as long long on PG,
            // int on MSSQL. Read as long long to be safe and narrow.
            long long curcount = 0;
            try { curcount = r.get<long long>(7); }
            catch (...) { curcount = r.get<int>(7); }
            info.current_count = static_cast<std::uint32_t>(
                std::max<long long>(0, curcount));
            long long haschar = 0;
            try { haschar = r.get<long long>(8); }
            catch (...) { haschar = r.get<int>(8); }
            // Cap at 255 — the wire byte fits a uint8_t; the lobby UI
            // doesn't render above ~3 distinct chars per world anyway.
            info.has_char = static_cast<std::uint8_t>(
                std::min<long long>(255, std::max<long long>(0, haschar)));
            // The cap-override branch in ResolveStatus only cares
            // about the >0 / ==0 distinction, so pass it as a 0/1 flag
            // there even though we send the full count on the wire.
            const int has_char_flag = info.has_char > 0 ? 1 : 0;
            info.status   = ResolveStatus(
                status_raw, static_cast<int>(curcount), busy_th, full_th,
                static_cast<int>(info.max_user), has_char_flag);
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
            // Channels don't carry the per-user has-char / cap-override
            // semantics — legacy CSHandler.cpp:577 uses the same
            // busy/full thresholds without those. Pass 0/0 so the
            // override branch in ResolveStatus is a no-op.
            info.status  = ResolveStatus(
                status_raw, static_cast<int>(curcount), busy_th, full_th,
                /*max_user=*/0, /*has_char=*/0);
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
