// SociMapServerLocator implementation — SOCI-backed
// IMapServerLocator. Replaces the legacy TFindServerID stored proc
// with explicit, traceable SOCI queries.
//
// Lookup() resolves CS_START_REQ to a Map endpoint by walking three
// candidate tables in order, mirroring the legacy SP:
//   1. TSVRCHART        — explicit char-to-server pin (used by
//                          long-lived chars).
//   2. TCHANNELCHART    — char→channel hint, then resolve channel→server.
//   3. TSPAWNPOSCHART   — fallback: map the char's last-spawn coord to
//                          a server via spawn-zone ownership table.
// Round-robin load-balancing over TMACHINE rows ties for active count.
// BR / BOW shard chars are routed to the dedicated shard server when
// the char is found in TBRPLAYERTABLE / TBOWPLAYERTABLE.
//
// ListGroups() / ListChannels() back the lobby's CS_GROUPLIST_REQ /
// CS_CHANNELLIST_REQ — query TGROUP / TCHANNEL with a live count of
// TCURRENTUSER rows for status decoration.
//
// Legacy parity: Server/TLoginSvr's TFindServerID stored proc +
// CTLoginSvrModule::CSPGroupList / CSPChannelList SP calls.

#include "soci_map_server_locator.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

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

// Round-robin IP picker. Port of legacy TRoute SP (TRoute.sql:48-71):
//
//   1. Count active TIPADDR rows for the machine.
//   2. Read TMACHINE.bRouteID (defaults to 0 if no row).
//   3. Advance the counter modulo count, 1-indexed
//      (`new = (cur % count) + 1`).
//   4. Pick the n-th active IP (sorted by szIPAddr — legacy uses
//      cursor FETCH ABSOLUTE which iterates in the implementation-
//      defined index order; sorting by szIPAddr gives stable,
//      deterministic rotation across the active IPs).
//   5. UPSERT TMACHINE.bRouteID with the new value.
//
// Best-effort throughout: TMACHINE is optional in modern, and a
// missing row just means the counter starts at 0 on every call (we
// always pick ips[0] — same as the pre-G12 JOIN-only behavior).
// Step 5 swallows persistence errors so a read-only TMACHINE doesn't
// break routing.
std::optional<std::string>
PickIpForMachine(soci::session& sql, int machine_id)
{
    // Step 1+4 combined: fetch all active IPs sorted. SOCI rowset
    // closes its statement when the destructor runs at end of this
    // scope, so subsequent queries on the same connection are clean.
    std::vector<std::string> ips;
    {
        soci::rowset<std::string> rs = (sql.prepare <<
            "SELECT \"szIPAddr\" FROM \"TIPADDR\" "
            "WHERE \"bMachineID\" = :m AND \"bActive\" = 1 "
            "ORDER BY \"szIPAddr\"",
            soci::use(machine_id));
        for (const auto& ip : rs) ips.push_back(ip);
    }
    if (ips.empty()) return std::nullopt;

    // Step 2 — read current counter. Optional table; default 0.
    int current_route = 0;
    try
    {
        soci::indicator ind = soci::i_null;
        soci::statement st = (sql.prepare <<
            "SELECT \"bRouteID\" FROM \"TMACHINE\" WHERE \"bMachineID\" = :m",
            soci::use(machine_id),
            soci::into(current_route, ind));
        st.execute(true);
        if (!st.got_data() || ind == soci::i_null)
            current_route = 0;
    }
    catch (const std::exception& ex)
    {
        spdlog::debug("map_locator: TMACHINE read skipped (machine_id={}): {}",
            machine_id, ex.what());
        current_route = 0;
    }

    // Step 3 — advance modulo count. 1-indexed to match the legacy
    // cursor's FETCH ABSOLUTE indexing.
    const int count = static_cast<int>(ips.size());
    const int next_route = (current_route % count) + 1;

    // Step 5 — persist new counter (INSERT-or-UPDATE). Race-safe
    // enough for LB: at worst, two concurrent picks observe the same
    // current_route and both write next_route, resulting in slightly
    // uneven distribution. Legacy SP wraps in TRAN TROUTE; modern
    // skips that — the deviation is bounded by pool_size and
    // bounded LB skew is fine.
    try
    {
        int row_exists = 0;
        sql << "SELECT COUNT(*) FROM \"TMACHINE\" WHERE \"bMachineID\" = :m",
            soci::use(machine_id), soci::into(row_exists);
        if (row_exists > 0)
        {
            sql << "UPDATE \"TMACHINE\" SET \"bRouteID\" = :r "
                   "WHERE \"bMachineID\" = :m",
                soci::use(next_route), soci::use(machine_id);
        }
        else
        {
            sql << "INSERT INTO \"TMACHINE\" "
                   "(\"bMachineID\", \"szNAME\", \"bRouteID\") "
                   "VALUES (:m, '', :r)",
                soci::use(machine_id), soci::use(next_route);
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::debug("map_locator: TMACHINE upsert skipped (machine_id={}): {}",
            machine_id, ex.what());
    }

    return ips[next_route - 1];
}

// Port of legacy TUpdateActiveChar (TGAME) — TFindServerID SP EXECs
// this after a successful route resolution (TFindServerID.sql:84).
// Maintains TACTIVECHARTABLE — the "high-level chars eligible for
// kingdom-war / castle-siege" registry. Eligibility = level >= 80
// AND (char's country < 2 OR aid country < 2), i.e. enrolled in a
// PvP-aligned country (legacy NetCode countries 0/1 are the two PvP
// factions; 2 is the AI / neutral country, 4 is peace).
//
// Behavior matches legacy SP exactly:
//   if row exists in TACTIVECHARTABLE:
//      eligible → UPDATE dateEnter = now
//      not eligible → DELETE (char re-rolled / dropped from PvP)
//   else if eligible AND level >= 80 → INSERT
//
// Tables `TAIDTABLE` and `TACTIVECHARTABLE` are optional in modern;
// each lookup is wrapped so a missing table downgrades to a no-op.
// `world_sql` is the TGAME session.
void UpdateActiveChar(soci::session& world_sql, int char_id)
{
    int level = 0, country = 0;
    bool got_char = false;
    {
        soci::indicator l_ind = soci::i_null, c_ind = soci::i_null;
        soci::statement st = (world_sql.prepare <<
            "SELECT \"bLevel\", \"bCountry\" FROM \"TCHARTABLE\" "
            "WHERE \"dwCharID\" = :c",
            soci::use(char_id),
            soci::into(level, l_ind),
            soci::into(country, c_ind));
        st.execute(true);
        got_char = st.got_data();
    }
    if (!got_char) return;

    // TAIDTABLE optional — default aid_country=3 (no aiding) so the
    // eligibility check falls through to char's primary country.
    int aid_country = 3;
    try
    {
        soci::indicator a_ind = soci::i_null;
        soci::statement st = (world_sql.prepare <<
            "SELECT \"bCountry\" FROM \"TAIDTABLE\" "
            "WHERE \"dwCharID\" = :c",
            soci::use(char_id),
            soci::into(aid_country, a_ind));
        st.execute(true);
        if (!st.got_data() || a_ind == soci::i_null) aid_country = 3;
    }
    catch (const std::exception&) { aid_country = 3; }

    const bool eligible = (country < 2 || aid_country < 2);

    try
    {
        int exists_count = 0;
        world_sql << "SELECT COUNT(*) FROM \"TACTIVECHARTABLE\" "
                     "WHERE \"dwCharID\" = :c",
            soci::use(char_id), soci::into(exists_count);

        if (exists_count > 0)
        {
            if (eligible)
            {
                world_sql << "UPDATE \"TACTIVECHARTABLE\" SET "
                             "  \"dateEnter\" = CURRENT_TIMESTAMP "
                             "WHERE \"dwCharID\" = :c",
                    soci::use(char_id);
            }
            else
            {
                world_sql << "DELETE FROM \"TACTIVECHARTABLE\" "
                             "WHERE \"dwCharID\" = :c",
                    soci::use(char_id);
            }
        }
        else if (level >= 80 && eligible)
        {
            world_sql << "INSERT INTO \"TACTIVECHARTABLE\" "
                         "(\"dwCharID\", \"dateEnter\") "
                         "VALUES (:c, CURRENT_TIMESTAMP)",
                soci::use(char_id);
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::debug("map_locator: TACTIVECHARTABLE update skipped "
                      "(char_id={}): {}", char_id, ex.what());
    }
}

// Port of TFindServerID (Server/TLoginSvr DB plan, TGAME.dbo.TFindServerID).
// Resolves the Map server hosting a character's current map zone.
//
//   1. Read char's wMapID + (fPosX, fPosZ) + wSpawnID from TCHARTABLE
//   2. Compute spatial wUnitID = trunc(fPosZ/1024)*256 + trunc(fPosX/1024)
//   3. JOIN TSVRCHART + TCHANNELCHART for (group, mapID, unitID, channel) → bServerID
//   4. If no row, fall back to TSPAWNPOSCHART by wSpawnID — char is in a
//      zone with no server; reposition them to the spawn point and
//      stamp the new (wMapID, fPosX, fPosZ) onto TCHARTABLE so the
//      legacy map server (frozen — doesn't auto-correct stale
//      positions on EnterMap) gets a coherent char row. Only update
//      after the spawn-side server lookup succeeds, matching legacy
//      TFindServerID SP semantics (UPDATE inside the same IF block
//      that has the second SELECT).
//   5. Return resolved bServerID, or nullopt if every lookup misses.
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
    auto resolved = lookup_at(spawn_map, unit_id);
    if (!resolved) return std::nullopt;

    // Persist the corrected position so the next CS_START_REQ skips
    // the fallback path (and the legacy map server reads a coherent
    // wMapID + position from TCHARTABLE when the client connects).
    // Legacy TFindServerID SP does this inline (TFindServerID.sql:81).
    // Best-effort: a stale position is recoverable on the next login;
    // an UPDATE failure shouldn't break routing for this attempt.
    try
    {
        sql << "UPDATE \"TCHARTABLE\" SET "
               "  \"wMapID\" = :m, "
               "  \"fPosX\"  = :x, "
               "  \"fPosY\"  = 0, "
               "  \"fPosZ\"  = :z "
               "WHERE \"dwCharID\" = :c",
            soci::use(spawn_map),
            soci::use(spawn_x),
            soci::use(spawn_z),
            soci::use(char_id);
    }
    catch (const std::exception& ex)
    {
        spdlog::warn("map_locator: spawn-respawn UPDATE TCHARTABLE "
                     "char_id={} failed ({}) — routing still succeeded, "
                     "next login will re-enter fallback path",
            char_id, ex.what());
    }
    return resolved;
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
        int machine_id = 0;
        bool got_row = false;
        // Dialect: PG/SQLite use trailing LIMIT 1, MSSQL uses TOP 1
        // immediately after SELECT.
        const bool is_mssql = (m_pool.GetBackend() == fourstory::db::Backend::Odbc);

        // Step 1 — find the TSERVER row matching the filter. Two
        // shapes (shard / default) × two dialects (MSSQL / PG). The
        // shard path pins server_id; the default path takes the first
        // map server in the group ordered by bServerID. We pull
        // bMachineID here so Step 2 can rotate across that machine's
        // active IPs (TIPADDR rows) via the TMACHINE round-robin
        // counter — see PickIpForMachine.
        const char* query;
        if (resolved_server_id.has_value())
        {
            query = is_mssql
                ? "SELECT TOP 1 \"wPort\", \"bServerID\", \"bMachineID\" "
                  "FROM \"TSERVER\" "
                  "WHERE \"bGroupID\" = :g AND \"bType\" = :t "
                  "  AND \"bServerID\" = :sid"
                : "SELECT \"wPort\", \"bServerID\", \"bMachineID\" "
                  "FROM \"TSERVER\" "
                  "WHERE \"bGroupID\" = :g AND \"bType\" = :t "
                  "  AND \"bServerID\" = :sid "
                  "LIMIT 1";
        }
        else
        {
            query = is_mssql
                ? "SELECT TOP 1 \"wPort\", \"bServerID\", \"bMachineID\" "
                  "FROM \"TSERVER\" "
                  "WHERE \"bGroupID\" = :g AND \"bType\" = :t "
                  "ORDER BY \"bServerID\""
                : "SELECT \"wPort\", \"bServerID\", \"bMachineID\" "
                  "FROM \"TSERVER\" "
                  "WHERE \"bGroupID\" = :g AND \"bType\" = :t "
                  "ORDER BY \"bServerID\" "
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
                    soci::into(machine_id))
                : (sql.prepare << query,
                    soci::use(static_cast<int>(group_id)),
                    soci::use(kServerTypeMap),
                    soci::into(port),
                    soci::into(server_id),
                    soci::into(machine_id));
            st.execute(true);
            got_row = st.got_data();
        }
        if (!got_row)
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

        // Step 2 — pick a rotating IP for the resolved machine.
        // Matches legacy TRoute SP (TRoute.sql:48-71): count active IPs,
        // load TMACHINE.bRouteID, advance modulo count, pick the n-th
        // IP, persist the new counter. Multi-NIC machines get LB
        // across all active IPs; single-IP machines always return
        // that one IP (cycle length 1).
        const auto picked = PickIpForMachine(sql, machine_id);
        if (!picked)
        {
            spdlog::warn("map_locator: group={} server_id={} machine_id={} "
                         "has no active TIPADDR rows",
                static_cast<int>(group_id), server_id, machine_id);
            return std::nullopt;
        }
        const std::string& ip = *picked;

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

        // Post-route side effects — legacy TFindServerID SP EXECs a
        // chain of helpers after the route is resolved
        // (TFindServerID.sql:84-97). Modern ports the in-scope ones:
        //
        //   * TUpdateEnterLuckyDate — stamps TCURRENTUSER.dEnterDate
        //     + bLuckyNumber. Login-server scope (TGLOBAL write).
        //   * TUpdateActiveChar — maintains TACTIVECHARTABLE for
        //     kingdom-war eligibility. World-server-adjacent but
        //     wired here so the registry stays in sync with logins.
        //
        // NOT ported (out of scope):
        //   * TScammingPost — in-game post / GM messaging.
        //   * TChangedPetSystemToMountSystem — empty SP body in
        //     legacy (one-time migration, no-op now).

        // TUpdateEnterLuckyDate. Lucky number: legacy uses
        // `CAST(RAND()*100 AS TINYINT)`, we approximate with
        // `std::rand() % 100`. Best-effort.
        if (user_id != 0)
        {
            try
            {
                const int lucky = std::rand() % 100;
                sql << "UPDATE \"TCURRENTUSER\" SET "
                       "  \"dEnterDate\"   = CURRENT_TIMESTAMP, "
                       "  \"bLuckyNumber\" = :n "
                       "WHERE \"dwUserID\" = :u",
                    soci::use(lucky), soci::use(user_id);
            }
            catch (const std::exception& ex)
            {
                spdlog::debug("map_locator: enter-date stamp skipped "
                              "(uid={}): {}", user_id, ex.what());
            }
        }

        // TUpdateActiveChar — runs on the world pool against
        // TCHARTABLE / TAIDTABLE / TACTIVECHARTABLE. Only meaningful
        // when char_id != 0 (group-list lookups have no char yet).
        if (m_world != nullptr && char_id != 0)
        {
            try
            {
                auto wlease = m_world->Acquire();
                UpdateActiveChar(*wlease, char_id);
            }
            catch (const std::exception& ex)
            {
                spdlog::debug("map_locator: UpdateActiveChar skipped "
                              "(char_id={}): {}", char_id, ex.what());
            }
        }

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
