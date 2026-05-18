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
                  const std::vector<int>& machine_ids)
{
    for (int g : group_ids)
    {
        sql << "DELETE FROM \"TSERVER\" WHERE \"bGroupID\" = :g", soci::use(g);
    }
    for (int m : machine_ids)
    {
        sql << "DELETE FROM \"TIPADDR\" WHERE \"bMachineID\" = :m", soci::use(m);
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

    const std::vector<int> all_groups = { group_ok, group_inactive, group_other, group_empty };
    const std::vector<int> all_machines = { mach_active, mach_inactive };

    {
        auto lease = pool.Acquire();
        soci::session& sql = *lease;
        WipeFixtures(sql, all_groups, all_machines);

        const std::string ip_active   = "10.20.30.40";
        const std::string ip_inactive = "10.20.30.99";

        sql << "INSERT INTO \"TIPADDR\" (\"bMachineID\", \"szIPAddr\", \"bActive\") "
               "VALUES (:m, :ip, 1)",
            soci::use(mach_active), soci::use(ip_active);
        sql << "INSERT INTO \"TIPADDR\" (\"bMachineID\", \"szIPAddr\", \"bActive\") "
               "VALUES (:m, :ip, 0)",
            soci::use(mach_inactive), soci::use(ip_inactive);

        // group_ok: one map server (bType=4) on active machine.
        sql << "INSERT INTO \"TSERVER\" "
               "(\"bGroupID\", \"bServerID\", \"bType\", \"bMachineID\", \"wPort\") "
               "VALUES (:g, 1, 4, :m, 5001)",
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
    }

    tloginsvr::services::SociMapServerLocator svc(pool);

    // 1. Happy path.
    {
        const auto ep = svc.Lookup(static_cast<std::uint8_t>(group_ok), 0, 0);
        Check(ep.has_value(), "active map server → endpoint returned");
        if (ep)
        {
            Check(ep->ipv4 == std::array<std::uint8_t, 4>{10, 20, 30, 40},
                "endpoint IP parsed correctly");
            Check(ep->port == 5001, "endpoint port correct");
            Check(ep->server_id == 1, "endpoint server_id correct");
        }
    }

    // 2. Inactive machine → no endpoint.
    {
        const auto ep = svc.Lookup(static_cast<std::uint8_t>(group_inactive), 0, 0);
        Check(!ep.has_value(),
            "map server on inactive machine → nullopt");
    }

    // 3. Group with only non-map servers → no endpoint (bType filter).
    {
        const auto ep = svc.Lookup(static_cast<std::uint8_t>(group_other), 0, 0);
        Check(!ep.has_value(),
            "group with no SVRGRP_MAPSVR row → nullopt");
    }

    // 4. Empty group → no endpoint.
    {
        const auto ep = svc.Lookup(static_cast<std::uint8_t>(group_empty), 0, 0);
        Check(!ep.has_value(), "unknown group → nullopt");
    }

    // Cleanup.
    try
    {
        auto lease = pool.Acquire();
        WipeFixtures(*lease, all_groups, all_machines);
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
