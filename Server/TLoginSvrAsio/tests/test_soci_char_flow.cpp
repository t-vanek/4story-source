// Integration test for SociCharService against a live PostgreSQL.
// Same skip-on-missing-env pattern as test_soci_auth_flow: TLOGINSVR_TEST_PG_CONN
// unset → test passes with 0/0. Seeds an isolated namespace per PID
// so parallel runs (or cross-test pollution) don't collide.

#include "../db/session_pool.h"
#include "../services/soci_char_service.h"

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

// Wipe + seed. PID-scoped names so the test is parallel-safe and so
// repeated runs don't accumulate state. Uses two pre-existing users:
// 2000001 (the "owner") and 2000002 (an unrelated user for isolation
// tests). The TCHARTABLE rows that already belong to those users are
// removed at the start so each run starts clean.
void WipeFixtures(soci::session& sql, const std::string& name_prefix)
{
    const std::string like = name_prefix + "%";

    // Cascade-clean in dependency order.
    sql << "DELETE FROM \"TGUILDMEMBERTABLE\" WHERE \"dwCharID\" IN ("
           "SELECT \"dwCharID\" FROM \"TCHARTABLE\" WHERE \"szNAME\" LIKE :p)",
        soci::use(like);
    sql << "DELETE FROM \"TALLCHARTABLE\" WHERE \"szName\" LIKE :p",
        soci::use(like);
    sql << "DELETE FROM \"TCHARTABLE\" WHERE \"szNAME\" LIKE :p",
        soci::use(like);
    sql << "DELETE FROM \"TRESERVEDNAME\" WHERE \"szName\" LIKE :p",
        soci::use(like);
    sql << "DELETE FROM \"TKEEPINGNAME\" WHERE \"szName\" LIKE :p",
        soci::use(like);
}

tloginsvr::services::CharacterCreateRequest
MakeCreateReq(int user_id, std::uint8_t world,
              const std::string& name, std::uint8_t slot)
{
    tloginsvr::services::CharacterCreateRequest r{};
    r.user_id    = user_id;
    r.group_id   = world;
    r.name       = name;
    r.slot       = slot;
    r.char_class = 1;
    r.race       = 0;
    r.country    = 2;
    r.sex        = 0;
    r.hair       = 1;
    r.face       = 2;
    r.body       = 3;
    r.pants      = 4;
    r.hand       = 5;
    r.foot       = 6;
    return r;
}

void RunTests(const std::string& conn)
{
    tloginsvr::db::SessionPool pool(
        tloginsvr::db::Backend::PostgreSQL, conn, /*pool_size=*/2);

    // Alphanumeric-only — legacy IsValidCharName disallows _ and other
    // punctuation, so the prefix has to satisfy that too.
    const std::string prefix =
        std::string("Soci") + std::to_string(::getpid()) + "X";

    {
        auto lease = pool.Acquire();
        WipeFixtures(*lease, prefix);
    }

    tloginsvr::services::SociCharService svc(pool);
    using namespace tloginsvr::services;

    const int user_a = 2000001;
    const int user_b = 2000002;
    const std::uint8_t world = 1;

    // 1. Empty list for a fresh user.
    {
        const auto list = svc.List(user_a, world);
        Check(list.empty(), "fresh user → empty char list");
    }

    // 2. Invalid name (length).
    {
        auto req = MakeCreateReq(user_a, world, prefix + "ab", 0);
        // prefix is 8 chars min → already > 3, so make it explicitly too short
        req.name = "ab";
        const auto r = svc.Create(req);
        Check(r.status == CreateCharResult::OverChar,
            "name too short → OverChar");
    }

    // 3. Invalid name (charset).
    {
        auto req = MakeCreateReq(user_a, world, prefix + "bad name", 0);
        const auto r = svc.Create(req);
        Check(r.status == CreateCharResult::OverChar,
            "name with space → OverChar");
    }

    int alice_id = 0;
    // 4. Happy path create.
    {
        const auto r = svc.Create(MakeCreateReq(user_a, world, prefix + "Alpha", 0));
        Check(r.status == CreateCharResult::Success, "create Alpha → Success");
        Check(r.char_id > 0, "char_id assigned");
        Check(r.starting_level == 1, "starting_level == 1");
        Check(r.remaining_slots == 5, "remaining_slots == 5 after first create");
        alice_id = r.char_id;
    }

    // 5. Duplicate name (case-sensitive).
    {
        const auto r = svc.Create(MakeCreateReq(user_b, world, prefix + "Alpha", 0));
        Check(r.status == CreateCharResult::DuplicateName,
            "second create with same name → DuplicateName");
    }

    // 6. Slot collision (same user, same slot).
    {
        const auto r = svc.Create(MakeCreateReq(user_a, world, prefix + "Beta", 0));
        Check(r.status == CreateCharResult::InvalidSlot,
            "slot already taken → InvalidSlot");
    }

    // 7. Reserved name.
    {
        {
            auto lease = pool.Acquire();
            soci::session& sql = *lease;
            const std::string res = prefix + "Admin";
            sql << "INSERT INTO \"TRESERVEDNAME\" (\"szName\", \"dwUserID\") "
                   "VALUES (:n, 0)", soci::use(res);
        }
        const auto r = svc.Create(MakeCreateReq(user_a, world, prefix + "Admin", 1));
        Check(r.status == CreateCharResult::Protected,
            "reserved name → Protected");
    }

    // 8. List returns the created char.
    {
        const auto list = svc.List(user_a, world);
        Check(list.size() == 1, "list has 1 char");
        if (!list.empty())
        {
            Check(list[0].char_id == alice_id, "list char_id matches");
            Check(list[0].name == prefix + "Alpha", "list name matches");
            Check(list[0].slot == 0, "list slot matches");
            Check(list[0].level == 1, "list level matches");
            Check(list[0].char_class == 1, "list char_class matches");
        }
    }

    // 9. List is world-scoped.
    {
        const auto list = svc.List(user_a, world + 1);
        Check(list.empty(), "different world → empty list");
    }

    // 10. Delete blocked by guild membership.
    {
        {
            auto lease = pool.Acquire();
            *lease << "INSERT INTO \"TGUILDMEMBERTABLE\" "
                      "(\"dwCharID\", \"dwGuildID\", \"bDuty\", \"bPeer\") "
                      "VALUES (:c, 1, 0, 0)",
                soci::use(alice_id);
        }
        const auto r = svc.Delete(user_a, world, alice_id, "ignored");
        Check(r == DeleteCharResult::Failed,
            "delete with active guild membership → Failed");
        // Cleanup the guild row so the next delete can succeed.
        {
            auto lease = pool.Acquire();
            *lease << "DELETE FROM \"TGUILDMEMBERTABLE\" WHERE \"dwCharID\" = :c",
                soci::use(alice_id);
        }
    }

    // 11. Hard delete (level <= 5).
    {
        const auto r = svc.Delete(user_a, world, alice_id, "ignored");
        Check(r == DeleteCharResult::Success, "delete level-1 char → Success (hard)");

        // TCHARTABLE row should be gone, TALLCHARTABLE row marked deleted.
        auto lease = pool.Acquire();
        int char_hits = 0;
        *lease << "SELECT COUNT(*) FROM \"TCHARTABLE\" WHERE \"dwCharID\" = :c",
            soci::use(alice_id), soci::into(char_hits);
        Check(char_hits == 0, "TCHARTABLE row hard-deleted");
        int all_hits_alive = 0;
        *lease << "SELECT COUNT(*) FROM \"TALLCHARTABLE\" "
                  "WHERE \"dwCharID\" = :c AND \"bDelete\" = 0",
            soci::use(alice_id), soci::into(all_hits_alive);
        Check(all_hits_alive == 0, "TALLCHARTABLE row marked deleted");
    }

    // 12. Soft delete (level > 5).
    {
        const auto cr = svc.Create(MakeCreateReq(user_a, world, prefix + "Gamma", 2));
        Check(cr.status == CreateCharResult::Success, "create Gamma → Success");
        const int gamma_id = cr.char_id;

        // Bump level past the threshold.
        {
            auto lease = pool.Acquire();
            *lease << "UPDATE \"TCHARTABLE\" SET \"bLevel\" = 10 WHERE \"dwCharID\" = :c",
                soci::use(gamma_id);
        }

        const auto r = svc.Delete(user_a, world, gamma_id, "ignored");
        Check(r == DeleteCharResult::Success, "delete level-10 char → Success (soft)");

        auto lease = pool.Acquire();
        int soft_marked = 0;
        *lease << "SELECT COUNT(*) FROM \"TCHARTABLE\" "
                  "WHERE \"dwCharID\" = :c AND \"bDelete\" = 1",
            soci::use(gamma_id), soci::into(soft_marked);
        Check(soft_marked == 1, "TCHARTABLE row soft-deleted (bDelete=1)");
    }

    // 13. Delete fails when char doesn't belong to user.
    {
        const auto cr = svc.Create(MakeCreateReq(user_a, world, prefix + "Delta", 3));
        Check(cr.status == CreateCharResult::Success, "create Delta → Success");
        const auto r = svc.Delete(user_b, world, cr.char_id, "ignored");
        Check(r == DeleteCharResult::Failed,
            "delete with wrong owner → Failed");
    }

    // Cleanup.
    try
    {
        auto lease = pool.Acquire();
        WipeFixtures(*lease, prefix);
    }
    catch (const std::exception& ex)
    {
        std::printf("  WARN  cleanup error (non-fatal): %s\n", ex.what());
    }
}

} // namespace

int main()
{
    std::printf("=== tloginsvr_asio SOCI char-flow test ===\n");

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
