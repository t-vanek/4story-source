// Integration test for SociMapServerLocator against a live PostgreSQL.
// Seeds rows in TSERVER + TIPADDR + TGROUP and checks the join + filter
// logic returns the right endpoint (or none, when no map server is
// configured / the machine is inactive).

#include "fourstory/db/session_pool.h"
#include "../services/soci_map_server_locator.h"

#include <soci/soci.h>

#if defined(_WIN32)
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

void WipeFixtures(soci::session& gsql,
                  soci::session& wsql,
                  const std::vector<int>& group_ids,
                  const std::vector<int>& machine_ids,
                  const std::vector<int>& user_ids)
{
    // Global-pool cleanup. Delete in FK-respecting order:
    // TSERVER + TCHANNEL (both child of TGROUP), THEN TGROUP.
    for (int g : group_ids)
    {
        gsql << "DELETE FROM \"TSERVER\" WHERE \"bGroupID\" = :g", soci::use(g);
        try
        {
            gsql << "DELETE FROM \"TCHANNEL\" WHERE \"bGroupID\" = :g",
                soci::use(g);
        }
        catch (const std::exception&) { /* may be empty */ }
        try
        {
            gsql << "DELETE FROM \"TGROUP\" WHERE \"bGroupID\" = :g",
                soci::use(g);
        }
        catch (const std::exception&) { /* may be empty */ }
    }
    for (int m : machine_ids)
    {
        gsql << "DELETE FROM \"TIPADDR\" WHERE \"bMachineID\" = :m", soci::use(m);
        // TMACHINE optional — clear the round-robin counter for our
        // test machines so each run starts from index 0.
        try
        {
            gsql << "DELETE FROM \"TMACHINE\" WHERE \"bMachineID\" = :m",
                soci::use(m);
        }
        catch (const std::exception&) { /* optional table */ }
    }
    // World-pool cleanup (TBR/BOW shard tables live in TGAME). TBOW
    // table not deployed on every world DB — wrap in try/catch.
    for (int u : user_ids)
    {
        try
        {
            wsql << "DELETE FROM \"TBRPLAYERTABLE\" WHERE \"dwUserID\" = :u", soci::use(u);
        }
        catch (const std::exception&) { /* optional */ }
        try
        {
            wsql << "DELETE FROM \"TBOWPLAYERTABLE\" WHERE \"dwUserID\" = :u", soci::use(u);
        }
        catch (const std::exception&) { /* optional */ }
    }
}

void RunTests(fourstory::db::Backend backend,
              const std::string& conn,
              const std::string& world_conn = "")
{
    fourstory::db::SessionPool pool(backend, conn, /*pool_size=*/2);
    const bool is_mssql = (backend == fourstory::db::Backend::Odbc);

    // For MSSQL the legacy schema is split TGLOBAL/TGAME — TBR/BOW
    // shard tables, TCHARTABLE, TACTIVECHARTABLE all live in TGAME.
    // PG dev fixture flattens both into one DB; pass world_conn==""
    // there to share the pool. The map locator's constructor takes
    // a world_pool*; when split, we hand it a second pool.
    std::unique_ptr<fourstory::db::SessionPool> world_pool;
    if (!world_conn.empty())
    {
        world_pool = std::make_unique<fourstory::db::SessionPool>(
            backend, world_conn, /*pool_size=*/2);
    }
    fourstory::db::SessionPool& world_ref =
        world_pool ? *world_pool : pool;

    // PID-scoped machine + group IDs to avoid colliding with other
    // test runs. Real MSSQL has bGroupID / bMachineID as tinyint
    // (max 255), so the test_gid in test 8 (base + 100, see below)
    // can't go past 155 — cap base at 50. PG dev fixture has them
    // as smallint and tolerates any 16-bit value. PG and MSSQL hit
    // separate DBs so no cross-backend collision; only PID matters.
    const int base = 50 + (::getpid() % 50);
    const int group_ok       = base;     // has active map server
    const int group_inactive = base + 1; // map server points at inactive machine
    const int group_other    = base + 2; // only non-map (bType != 4) servers
    const int group_empty    = base + 3; // no rows at all
    const int group_lb       = base + 4; // multi-IP machine for round-robin test

    const int mach_active   = base + 10;
    const int mach_inactive = base + 11;
    const int mach_lb       = base + 12; // has 2 active IPs

    const std::vector<int> all_groups   = { group_ok, group_inactive, group_other, group_empty, group_lb };
    const std::vector<int> all_machines = { mach_active, mach_inactive, mach_lb };

    // Test users for BR/BOW shard scenarios.
    const int user_normal = 5000000 + (::getpid() % 1000);
    const int user_br     = user_normal + 1;
    const int user_bow    = user_normal + 2;
    const int char_normal = 6000000 + (::getpid() % 1000);
    const int char_br     = char_normal + 1;
    const int char_bow    = char_normal + 2;
    const std::vector<int> all_users = { user_normal, user_br, user_bow };

    bool bow_seeded_outer = false;
    {
        auto glease = pool.Acquire();
        auto wlease = world_ref.Acquire();
        WipeFixtures(*glease, *wlease, all_groups, all_machines, all_users);
        soci::session& sql = *glease;

        const std::string ip_active   = "10.20.30.40";
        const std::string ip_inactive = "10.20.30.99";
        const std::string ip_br_shard = "10.20.30.50";
        const std::string ip_bow_shard = "10.20.30.30";

        // Legacy MSSQL TIPADDR has a FK on bMachineID → TMACHINE.
        // PG dev fixture doesn't enforce it, but for the MSSQL run
        // we have to seed the parent rows first. TMACHINE is also
        // the round-robin LB counter target (G12 fix), so all three
        // test machines need parent rows.
        for (int m : { mach_active, mach_inactive, mach_lb })
        {
            try
            {
                sql << "INSERT INTO \"TMACHINE\" "
                       "(\"bMachineID\", \"szNAME\", \"bRouteID\") "
                       "VALUES (:m, 'test', 0)",
                    soci::use(m);
            }
            catch (const std::exception&) { /* PG without TMACHINE */ }
        }

        sql << "INSERT INTO \"TIPADDR\" (\"bMachineID\", \"szIPAddr\", \"bActive\") "
               "VALUES (:m, :ip, 1)",
            soci::use(mach_active), soci::use(ip_active);
        sql << "INSERT INTO \"TIPADDR\" (\"bMachineID\", \"szIPAddr\", \"bActive\") "
               "VALUES (:m, :ip, 0)",
            soci::use(mach_inactive), soci::use(ip_inactive);

        // mach_lb: 2 active IPs for round-robin test, sorted as
        // 10.20.30.71 / .72 so the rotation order is deterministic.
        // Plus one inactive IP that should be ignored by the picker.
        sql << "INSERT INTO \"TIPADDR\" (\"bMachineID\", \"szIPAddr\", \"bActive\") "
               "VALUES (:m, '10.20.30.71', 1)",
            soci::use(mach_lb);
        sql << "INSERT INTO \"TIPADDR\" (\"bMachineID\", \"szIPAddr\", \"bActive\") "
               "VALUES (:m, '10.20.30.72', 1)",
            soci::use(mach_lb);
        sql << "INSERT INTO \"TIPADDR\" (\"bMachineID\", \"szIPAddr\", \"bActive\") "
               "VALUES (:m, '10.20.30.73', 0)",
            soci::use(mach_lb);

        // Legacy MSSQL has TSERVER.bGroupID → TGROUP FK and
        // TCHANNEL.bGroupID → TGROUP FK. Seed the parent group rows
        // before any TSERVER INSERT. Full NOT-NULL column set
        // (bGroupID/bType/szNAME/szDSN/szUserID). Skip group_empty
        // (no TSERVER rows for it either) so we keep the "unknown
        // group → no endpoint" assertion path intact.
        for (int g : { group_ok, group_inactive, group_other, group_lb })
        {
            sql << "INSERT INTO \"TGROUP\" "
                   "(\"bGroupID\", \"bType\", \"szNAME\", "
                   " \"szDSN\", \"szUserID\") "
                   "VALUES (:g, 4, 'test', '', '')",
                soci::use(g);
        }

        // group_lb: one server pointing to the multi-IP machine.
        sql << "INSERT INTO \"TSERVER\" "
               "(\"bGroupID\", \"bServerID\", \"bType\", \"bMachineID\", \"wPort\") "
               "VALUES (:g, 1, 4, :m, 5070)",
            soci::use(group_lb), soci::use(mach_lb);

        // group_ok: server_id=1 (default), 50 (BR shard), 30 (BOW shard).
        sql << "INSERT INTO \"TSERVER\" "
               "(\"bGroupID\", \"bServerID\", \"bType\", \"bMachineID\", \"wPort\") "
               "VALUES (:g, 1, 4, :m, 5001)",
            soci::use(group_ok), soci::use(mach_active);
        sql << "INSERT INTO \"TSERVER\" "
               "(\"bGroupID\", \"bServerID\", \"bType\", \"bMachineID\", \"wPort\") "
               "VALUES (:g, 50, 4, :m, 5050)",
            soci::use(group_ok), soci::use(mach_active);
        sql << "INSERT INTO \"TSERVER\" "
               "(\"bGroupID\", \"bServerID\", \"bType\", \"bMachineID\", \"wPort\") "
               "VALUES (:g, 30, 4, :m, 5030)",
            soci::use(group_ok), soci::use(mach_active);

        // group_inactive: map server on a machine that's marked inactive.
        sql << "INSERT INTO \"TSERVER\" "
               "(\"bGroupID\", \"bServerID\", \"bType\", \"bMachineID\", \"wPort\") "
               "VALUES (:g, 1, 4, :m, 5002)",
            soci::use(group_inactive), soci::use(mach_inactive);

        // group_other: a different server type on the active machine
        // (e.g. bType=1 char-server). Lookup should ignore it.
        sql << "INSERT INTO \"TSERVER\" "
               "(\"bGroupID\", \"bServerID\", \"bType\", \"bMachineID\", \"wPort\") "
               "VALUES (:g, 1, 1, :m, 5003)",
            soci::use(group_other), soci::use(mach_active);

        // Shard membership rows live in the world (TGAME) pool.
        // TBOWPLAYERTABLE not deployed on every legacy world DB
        // (e.g. the restored 2019 dump's TGAME has TBRPLAYERTABLE
        // but no BOW); track whether the seed actually landed so
        // the BOW assertions can SKIP cleanly.
        soci::session& wsql = *wlease;
        try
        {
            wsql << "INSERT INTO \"TBRPLAYERTABLE\" (\"dwCharID\", \"dwUserID\") "
                    "VALUES (:c, :u)", soci::use(char_br), soci::use(user_br);
        }
        catch (const std::exception&) { /* optional */ }
        try
        {
            wsql << "INSERT INTO \"TBOWPLAYERTABLE\" (\"dwCharID\", \"dwUserID\") "
                    "VALUES (:c, :u)", soci::use(char_bow), soci::use(user_bow);
            bow_seeded_outer = true;
        }
        catch (const std::exception&) { /* TBOWPLAYERTABLE absent */ }
    }

    // Hand the locator the appropriate (global, world) pool pair.
    // When world_conn was empty, world_ref aliases the global pool —
    // matches the legacy single-DB PG dev fixture.
    tloginsvr::services::SociMapServerLocator svc(pool, &world_ref);

    // 1. Happy path — normal char routes to default server (id=1, lowest).
    {
        const auto ep = svc.Lookup(
            user_normal, static_cast<std::uint8_t>(group_ok), 0, char_normal);
        Check(ep.has_value(), "active map server → endpoint returned");
        if (ep)
        {
            Check(ep->port == 5001, "default server endpoint port (5001)");
            Check(ep->server_id == 1, "default server endpoint server_id (1)");
        }
    }

    // 2. BR shard override → routes to server_id=50.
    {
        const auto ep = svc.Lookup(
            user_br, static_cast<std::uint8_t>(group_ok), 0, char_br);
        Check(ep.has_value(), "BR player → endpoint returned");
        if (ep)
        {
            Check(ep->port == 5050, "BR shard port (5050)");
            Check(ep->server_id == 50, "BR shard server_id (50)");
        }
    }

    // 3. BOW shard override → routes to server_id=30. Skipped on
    //    deploys without TBOWPLAYERTABLE (the seed above swallowed
    //    the "invalid object name" so we know the table wasn't
    //    available — the lookup would fall through to the default
    //    route and fail the assertion).
    if (bow_seeded_outer)
    {
        const auto ep = svc.Lookup(
            user_bow, static_cast<std::uint8_t>(group_ok), 0, char_bow);
        Check(ep.has_value(), "BOW player → endpoint returned");
        if (ep)
        {
            Check(ep->port == 5030, "BOW shard port (5030)");
            Check(ep->server_id == 30, "BOW shard server_id (30)");
        }
    }
    else
    {
        std::printf("  SKIP  BOW shard test — TBOWPLAYERTABLE not deployed\n");
    }

    // 4. Shard membership is (user, char)-scoped — BR row for a different
    //    char doesn't override this lookup.
    {
        const auto ep = svc.Lookup(
            user_br, static_cast<std::uint8_t>(group_ok), 0, char_normal);
        Check(ep.has_value() && ep->server_id == 1,
            "user has BR row but for different char → default route");
    }

    // 5. Inactive machine → no endpoint.
    {
        const auto ep = svc.Lookup(
            user_normal, static_cast<std::uint8_t>(group_inactive), 0, char_normal);
        Check(!ep.has_value(),
            "map server on inactive machine → nullopt");
    }

    // 6. Group with only non-map servers → no endpoint (bType filter).
    {
        const auto ep = svc.Lookup(
            user_normal, static_cast<std::uint8_t>(group_other), 0, char_normal);
        Check(!ep.has_value(),
            "group with no SVRGRP_MAPSVR row → nullopt");
    }

    // 6b. Round-robin LB across multiple active IPs (G12). Machine
    //     mach_lb has 2 active IPs (.71, .72) sorted and 1 inactive
    //     (.73 — must be skipped). Three successive lookups for the
    //     same (group, char) should rotate: .71 → .72 → .71. Counter
    //     persistence verified via TMACHINE.bRouteID.
    {
        const std::uint8_t glb = static_cast<std::uint8_t>(group_lb);
        const auto ep1 = svc.Lookup(user_normal, glb, 0, char_normal);
        const auto ep2 = svc.Lookup(user_normal, glb, 0, char_normal);
        const auto ep3 = svc.Lookup(user_normal, glb, 0, char_normal);
        Check(ep1.has_value() && ep2.has_value() && ep3.has_value(),
            "LB: three lookups all return endpoints");
        if (ep1 && ep2 && ep3)
        {
            // Inactive IP must never appear.
            const auto is_active = [](const auto& ep) {
                return ep->ipv4 == std::array<std::uint8_t,4>{10,20,30,71}
                    || ep->ipv4 == std::array<std::uint8_t,4>{10,20,30,72};
            };
            Check(is_active(ep1) && is_active(ep2) && is_active(ep3),
                "LB: inactive .73 IP never picked");

            // Sorted order: .71 first, then .72, then back to .71.
            Check(ep1->ipv4[3] == 71, "LB: first lookup picks .71");
            Check(ep2->ipv4[3] == 72, "LB: second lookup picks .72");
            Check(ep3->ipv4[3] == 71, "LB: third lookup cycles back to .71");
        }

        // Counter persisted in TMACHINE — verify the upsert landed.
        {
            auto lease = pool.Acquire();
            int route_id = 0;
            *lease << "SELECT \"bRouteID\" FROM \"TMACHINE\" "
                      "WHERE \"bMachineID\" = :m",
                soci::use(mach_lb), soci::into(route_id);
            Check(route_id == 1,
                "LB: TMACHINE.bRouteID persisted after rotation cycle");
        }
    }

    // 7. Empty group → no endpoint.
    {
        const auto ep = svc.Lookup(
            user_normal, static_cast<std::uint8_t>(group_empty), 0, char_normal);
        Check(!ep.has_value(), "unknown group → nullopt");
    }

    // 8. ListGroups + ListChannels — round-trip TGROUP + TCHANNEL.
    //    The dev DB has rows from schema/postgres-dev.sql; we seed an
    //    extra group + channel pair so the tests don't depend on the
    //    exact seeded count.
    const int test_gid = group_ok + 100;
    const std::string gname = "ListGrp" + std::to_string(::getpid());
    const std::string cname = "ListCh"  + std::to_string(::getpid());
    {
        auto lease = pool.Acquire();
        soci::session& sql = *lease;
        sql << "DELETE FROM \"TGROUP\" WHERE \"bGroupID\" = :g", soci::use(test_gid);
        sql << "DELETE FROM \"TCHANNEL\" WHERE \"bGroupID\" = :g", soci::use(test_gid);
        // bStatus=1 (ENABLE), busy=10, full=100.
        sql << "INSERT INTO \"TGROUP\" "
               "(\"bGroupID\", \"bType\", \"szNAME\", "
               " \"szDSN\", \"szUserID\", "
               " \"wBusy\", \"wFull\", \"bUseRate\", \"bStatus\") "
               "VALUES (:g, 4, :n, '', '', 10, 100, 1, 1)",
            soci::use(test_gid), soci::use(gname);
        sql << "INSERT INTO \"TCHANNEL\" "
               "(\"bGroupID\", \"bChannel\", \"szNAME\", \"wBusy\", \"wFull\", \"bStatus\") "
               "VALUES (:g, 1, :n, 10, 100, 1)",
            soci::use(test_gid), soci::use(cname);
    }
    {
        const auto groups = svc.ListGroups(/*user_id=*/0);
        bool found = false;
        for (const auto& g : groups)
        {
            if (g.group_id == test_gid)
            {
                found = true;
                Check(g.name == gname, "ListGroups: seeded name matches");
                Check(g.type == 4, "ListGroups: type==4 (map server)");
                Check(g.status == tloginsvr::services::GroupStatus::Normal,
                    "ListGroups: status NORMAL (count<busy)");
            }
        }
        Check(found, "ListGroups returns seeded group");
    }
    {
        const auto channels = svc.ListChannels(
            static_cast<std::uint8_t>(test_gid));
        Check(channels.size() == 1, "ListChannels: one row for seeded group");
        if (!channels.empty())
        {
            Check(channels[0].channel == 1, "ListChannels: channel==1");
            Check(channels[0].name == cname, "ListChannels: name matches");
            Check(channels[0].status == tloginsvr::services::GroupStatus::Normal,
                "ListChannels: status NORMAL");
        }
    }
    {
        const auto channels = svc.ListChannels(
            static_cast<std::uint8_t>(group_empty));
        Check(channels.empty(),
            "ListChannels: empty group → empty vector");
    }

    // Cleanup (additional rows from test 8).
    {
        auto lease = pool.Acquire();
        soci::session& sql = *lease;
        sql << "DELETE FROM \"TCHANNEL\" WHERE \"bGroupID\" = :g", soci::use(test_gid);
        sql << "DELETE FROM \"TGROUP\" WHERE \"bGroupID\" = :g", soci::use(test_gid);
    }

    // 9. Post-route side effects (G5). Lookup() runs two helpers
    //    after resolving the endpoint:
    //      a) TUpdateEnterLuckyDate equivalent — UPDATE TCURRENTUSER
    //         SET dEnterDate=now, bLuckyNumber=rand.
    //      b) TUpdateActiveChar equivalent — INSERT into
    //         TACTIVECHARTABLE for level>=80 PvP-country chars.
    //
    //    Both tables are optional in modern; the helpers swallow
    //    missing-table errors. This test seeds the required rows
    //    and asserts the writes landed. If the optional tables
    //    aren't deployed, the inserts SKIP rather than fail.
    {
        const int side_uid  = user_normal + 100;
        const int side_char = char_normal + 100;
        const std::string old_ts = "1970-01-02 00:00:00";

        // Pre-clean. TCURRENTUSER → global pool; TCHARTABLE +
        // TACTIVECHARTABLE → world pool.
        {
            auto glease = pool.Acquire();
            *glease << "DELETE FROM \"TCURRENTUSER\" WHERE \"dwUserID\" = :u",
                soci::use(side_uid);

            auto wlease = world_ref.Acquire();
            try
            {
                *wlease << "DELETE FROM \"TCHARTABLE\" WHERE \"dwCharID\" = :c",
                    soci::use(side_char);
            }
            catch (const std::exception&) { /* may not exist if TGAME unavailable */ }
            try
            {
                *wlease << "DELETE FROM \"TACTIVECHARTABLE\" "
                           "WHERE \"dwCharID\" = :c",
                    soci::use(side_char);
            }
            catch (const std::exception&) { /* optional */ }
        }

        // Seed TCURRENTUSER. PG dev fixture has DEFAULTs on most
        // columns; legacy MSSQL has dwCharID, bGroupID, bChannel,
        // wPort, bLocked as NOT NULL with no default — supply all
        // explicitly so both backends accept the INSERT.
        {
            auto lease = pool.Acquire();
            *lease << "INSERT INTO \"TCURRENTUSER\" "
                      "(\"dwUserID\", \"dwCharID\", \"bGroupID\", \"bChannel\", "
                      " \"wPort\", \"bLocked\", \"dEnterDate\", "
                      " \"bLuckyNumber\", \"szLoginIP\") "
                      "VALUES (:u, 0, 0, 0, 0, 0, :t, 99, '127.0.0.1')",
                soci::use(side_uid), soci::use(old_ts);
        }

        // Seed TCHARTABLE with high-level PvP-country char so
        // UpdateActiveChar's INSERT branch fires. The legacy MSSQL
        // schema has TCHARTABLE.dwCharID as IDENTITY + ~14 more
        // NOT-NULL columns than PG dev fixture (dwHP/dwMP/wMapID/
        // fPos*/wDIR/wTemptedMon/bAftermath/dwGold/dwSilver/dwCooper).
        // The SET IDENTITY_INSERT batch lets the test pin the
        // explicit side_char value; the full column list keeps both
        // backends happy. TCHARTABLE lives in the world pool.
        {
            auto lease = world_ref.Acquire();
            soci::session& sql = *lease;
            const std::string cname = "LbSide" + std::to_string(::getpid());
            const std::string char_insert_pg =
                "INSERT INTO \"TCHARTABLE\" "
                "(\"dwCharID\", \"dwUserID\", \"bSlot\", \"szNAME\", "
                " \"bClass\", \"bRace\", \"bCountry\", \"bRealSex\", "
                " \"bSex\", \"bHair\", \"bFace\", \"bBody\", "
                " \"bPants\", \"bHand\", \"bFoot\", \"bLevel\") "
                "VALUES (:c, :u, 0, :n, 0, 0, 0, 0, 0, 0, 0, 0, "
                " 0, 0, 0, 80)";
            const std::string char_insert_ms =
                "SET IDENTITY_INSERT \"TCHARTABLE\" ON; "
                "INSERT INTO \"TCHARTABLE\" "
                "(\"dwCharID\", \"dwUserID\", \"bSlot\", \"szNAME\", "
                " \"bClass\", \"bRace\", \"bCountry\", \"bRealSex\", "
                " \"bSex\", \"bHair\", \"bFace\", \"bBody\", "
                " \"bPants\", \"bHand\", \"bFoot\", \"bLevel\", "
                " \"dwEXP\", \"dwHP\", \"dwMP\", \"wSkillPoint\", "
                " \"dwGold\", \"dwSilver\", \"dwCooper\", "
                " \"wMapID\", \"wSpawnID\", \"wTemptedMon\", \"bAftermath\", "
                " \"fPosX\", \"fPosY\", \"fPosZ\", \"wDIR\") "
                "VALUES (:c, :u, 0, :n, 0, 0, 0, 0, 0, 0, 0, 0, "
                " 0, 0, 0, 80, "
                " 0, 100, 100, 0, 0, 0, 0, "
                " 2010, 15003, 0, 0, "
                " 0, 0, 0, 0); "
                "SET IDENTITY_INSERT \"TCHARTABLE\" OFF;";
            const std::string& stmt = is_mssql ? char_insert_ms : char_insert_pg;
            sql << stmt,
                soci::use(side_char), soci::use(side_uid), soci::use(cname);
        }

        const auto ep = svc.Lookup(
            side_uid, static_cast<std::uint8_t>(group_ok), 0, side_char);
        Check(ep.has_value(), "side-effects: Lookup resolved endpoint");

        // (a) TCURRENTUSER.dEnterDate moved past the seeded 1970 ts.
        {
            auto lease = pool.Acquire();
            int recent_hits = 0;
            *lease << "SELECT COUNT(*) FROM \"TCURRENTUSER\" "
                      "WHERE \"dwUserID\"   = :u "
                      "  AND \"dEnterDate\" > :t",
                soci::use(side_uid), soci::use(old_ts),
                soci::into(recent_hits);
            Check(recent_hits == 1,
                "TUpdateEnterLuckyDate: dEnterDate bumped past seed");
        }

        // (b) TACTIVECHARTABLE row inserted (best-effort — skip
        //     assert if the optional table isn't deployed). TGAME
        //     table → world pool.
        try
        {
            auto lease = world_ref.Acquire();
            int active_hits = 0;
            *lease << "SELECT COUNT(*) FROM \"TACTIVECHARTABLE\" "
                      "WHERE \"dwCharID\" = :c",
                soci::use(side_char), soci::into(active_hits);
            Check(active_hits == 1,
                "TUpdateActiveChar: TACTIVECHARTABLE row inserted "
                "(level>=80, country<2)");
        }
        catch (const std::exception&)
        {
            std::printf("  SKIP  TACTIVECHARTABLE check — table not deployed\n");
        }

        // Cleanup — TCURRENTUSER on global pool, TCHARTABLE +
        // TACTIVECHARTABLE on world pool.
        {
            auto glease = pool.Acquire();
            *glease << "DELETE FROM \"TCURRENTUSER\" WHERE \"dwUserID\" = :u",
                soci::use(side_uid);
            auto wlease = world_ref.Acquire();
            try
            {
                *wlease << "DELETE FROM \"TCHARTABLE\" WHERE \"dwCharID\" = :c",
                    soci::use(side_char);
            }
            catch (const std::exception&) { /* TGAME unavailable */ }
            try
            {
                *wlease << "DELETE FROM \"TACTIVECHARTABLE\" "
                           "WHERE \"dwCharID\" = :c",
                    soci::use(side_char);
            }
            catch (const std::exception&) { /* optional */ }
        }
    }

    // Cleanup.
    try
    {
        auto glease = pool.Acquire();
        auto wlease = world_ref.Acquire();
        WipeFixtures(*glease, *wlease, all_groups, all_machines, all_users);
    }
    catch (const std::exception& ex)
    {
        std::printf("  WARN  cleanup error (non-fatal): %s\n", ex.what());
    }
}

} // namespace

int main()
{
    std::printf("=== tloginsvr_asio SOCI map-locator test ===\n");

    const char* pg_conn          = std::getenv("TLOGINSVR_TEST_PG_CONN");
    const char* mssql_conn       = std::getenv("TLOGINSVR_TEST_MSSQL_CONN");
    const char* mssql_world_conn = std::getenv("TLOGINSVR_TEST_MSSQL_WORLD_CONN");

    if ((pg_conn    == nullptr || pg_conn[0]    == '\0') &&
        (mssql_conn == nullptr || mssql_conn[0] == '\0'))
    {
        std::printf("  SKIP  neither TLOGINSVR_TEST_PG_CONN nor "
                    "TLOGINSVR_TEST_MSSQL_CONN set\n");
        std::printf("\nResults: 0 passed, 0 failed (skipped)\n");
        return 0;
    }

    if (pg_conn != nullptr && pg_conn[0] != '\0')
    {
        std::printf("\n--- backend: postgresql ---\n");
        try
        {
            RunTests(fourstory::db::Backend::PostgreSQL, pg_conn);
        }
        catch (const std::exception& ex)
        {
            std::printf("  FAIL  pg exception: %s\n", ex.what());
            ++g_failed;
        }
    }

    if (mssql_conn != nullptr && mssql_conn[0] != '\0')
    {
        std::printf("\n--- backend: odbc (MSSQL) ---\n");
        const std::string world_conn =
            (mssql_world_conn != nullptr && mssql_world_conn[0] != '\0')
                ? mssql_world_conn
                : "";
        if (world_conn.empty())
        {
            std::printf("  NOTE  TLOGINSVR_TEST_MSSQL_WORLD_CONN unset — "
                        "TGAME-table operations (BR/BOW shard + test 9) "
                        "will fall through to silent failures\n");
        }
        try
        {
            RunTests(fourstory::db::Backend::Odbc, mssql_conn, world_conn);
        }
        catch (const std::exception& ex)
        {
            std::printf("  FAIL  mssql exception: %s\n", ex.what());
            ++g_failed;
        }
    }

    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
