// Integration test for SociMapServerLocator against a live PostgreSQL.
// Seeds rows in TSERVER + TIPADDR + TGROUP and checks the join + filter
// logic returns the right endpoint (or none, when no map server is
// configured / the machine is inactive).

#include "../db/session_pool.h"
#include "../services/soci_map_server_locator.h"

#include <soci/soci.h>

#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

void WipeFixtures(soci::session& sql,
                  const std::vector<int>& group_ids,
                  const std::vector<int>& machine_ids,
                  const std::vector<int>& user_ids)
{
    for (int g : group_ids)
    {
        sql << "DELETE FROM \"TSERVER\" WHERE \"bGroupID\" = :g", soci::use(g);
    }
    for (int m : machine_ids)
    {
        sql << "DELETE FROM \"TIPADDR\" WHERE \"bMachineID\" = :m", soci::use(m);
    }
    for (int u : user_ids)
    {
        sql << "DELETE FROM \"TBRPLAYERTABLE\" WHERE \"dwUserID\" = :u", soci::use(u);
        sql << "DELETE FROM \"TBOWPLAYERTABLE\" WHERE \"dwUserID\" = :u", soci::use(u);
    }
}

void RunTests(const std::string& conn)
{
    tloginsvr::db::SessionPool pool(
        tloginsvr::db::Backend::PostgreSQL, conn, /*pool_size=*/2);

    // PID-scoped machine + group IDs to avoid colliding with other test
    // runs. smallint is 16-bit signed; cap at 200 + pid%50 etc to stay
    // safe.
    const int base = 100 + (::getpid() % 50);
    const int group_ok       = base;     // has active map server
    const int group_inactive = base + 1; // map server points at inactive machine
    const int group_other    = base + 2; // only non-map (bType != 4) servers
    const int group_empty    = base + 3; // no rows at all

    const int mach_active   = base + 10;
    const int mach_inactive = base + 11;

    const std::vector<int> all_groups   = { group_ok, group_inactive, group_other, group_empty };
    const std::vector<int> all_machines = { mach_active, mach_inactive };

    // Test users for BR/BOW shard scenarios.
    const int user_normal = 5000000 + (::getpid() % 1000);
    const int user_br     = user_normal + 1;
    const int user_bow    = user_normal + 2;
    const int char_normal = 6000000 + (::getpid() % 1000);
    const int char_br     = char_normal + 1;
    const int char_bow    = char_normal + 2;
    const std::vector<int> all_users = { user_normal, user_br, user_bow };

    {
        auto lease = pool.Acquire();
        soci::session& sql = *lease;
        WipeFixtures(sql, all_groups, all_machines, all_users);

        const std::string ip_active   = "10.20.30.40";
        const std::string ip_inactive = "10.20.30.99";
        const std::string ip_br_shard = "10.20.30.50";
        const std::string ip_bow_shard = "10.20.30.30";

        sql << "INSERT INTO \"TIPADDR\" (\"bMachineID\", \"szIPAddr\", \"bActive\") "
               "VALUES (:m, :ip, 1)",
            soci::use(mach_active), soci::use(ip_active);
        sql << "INSERT INTO \"TIPADDR\" (\"bMachineID\", \"szIPAddr\", \"bActive\") "
               "VALUES (:m, :ip, 0)",
            soci::use(mach_inactive), soci::use(ip_inactive);

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

        // Shard membership rows.
        sql << "INSERT INTO \"TBRPLAYERTABLE\" (\"dwCharID\", \"dwUserID\") "
               "VALUES (:c, :u)", soci::use(char_br), soci::use(user_br);
        sql << "INSERT INTO \"TBOWPLAYERTABLE\" (\"dwCharID\", \"dwUserID\") "
               "VALUES (:c, :u)", soci::use(char_bow), soci::use(user_bow);
    }

    tloginsvr::services::SociMapServerLocator svc(pool);

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

    // 3. BOW shard override → routes to server_id=30.
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

    // 7. Empty group → no endpoint.
    {
        const auto ep = svc.Lookup(
            user_normal, static_cast<std::uint8_t>(group_empty), 0, char_normal);
        Check(!ep.has_value(), "unknown group → nullopt");
    }

    // Cleanup.
    try
    {
        auto lease = pool.Acquire();
        WipeFixtures(*lease, all_groups, all_machines, all_users);
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

    const char* conn = std::getenv("TLOGINSVR_TEST_PG_CONN");
    if (conn == nullptr || conn[0] == '\0')
    {
        std::printf("  SKIP  TLOGINSVR_TEST_PG_CONN not set\n");
        std::printf("\nResults: 0 passed, 0 failed (skipped)\n");
        return 0;
    }

    try
    {
        RunTests(conn);
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception: %s\n", ex.what());
        ++g_failed;
    }

    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
