// Integration test for SociCharService against a live PostgreSQL.
// Same skip-on-missing-env pattern as test_soci_auth_flow: TLOGINSVR_TEST_PG_CONN
// unset → test passes with 0/0. Seeds an isolated namespace per PID
// so parallel runs (or cross-test pollution) don't collide.

#include "fourstory/db/session_pool.h"
#include "../services/soci_char_service.h"

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

// Wipe + seed. PID-scoped names so the test is parallel-safe and so
// repeated runs don't accumulate state. Uses two pre-existing users:
// 2000001 (the "owner") and 2000002 (an unrelated user for isolation
// tests). The TCHARTABLE rows that already belong to those users are
// removed at the start so each run starts clean.
void WipeFixtures(soci::session& gsql, soci::session& wsql,
                  const std::string& name_prefix)
{
    const std::string like = name_prefix + "%";

    // World-pool cleanup. Cascade-clean in dependency order: guild
    // members (FK → TCHARTABLE-ish), then per-char items, then
    // TCHARTABLE itself.
    wsql << "DELETE FROM \"TGUILDMEMBERTABLE\" WHERE \"dwCharID\" IN ("
            "SELECT \"dwCharID\" FROM \"TCHARTABLE\" WHERE \"szNAME\" LIKE :p)",
        soci::use(like);
    try
    {
        wsql << "DELETE FROM \"TITEMTABLE\" WHERE \"dwOwnerID\" IN ("
                "SELECT \"dwCharID\" FROM \"TCHARTABLE\" WHERE \"szNAME\" LIKE :p)",
            soci::use(like);
    }
    catch (const std::exception&) { /* may not exist on dev */ }
    wsql << "DELETE FROM \"TCHARTABLE\" WHERE \"szNAME\" LIKE :p",
        soci::use(like);

    // Global-pool cleanup. TALLCHARTABLE is the cross-world directory;
    // TRESERVED/TKEEPINGNAME are name-reservation lists.
    gsql << "DELETE FROM \"TALLCHARTABLE\" WHERE \"szName\" LIKE :p",
        soci::use(like);
    gsql << "DELETE FROM \"TRESERVEDNAME\" WHERE \"szName\" LIKE :p",
        soci::use(like);
    gsql << "DELETE FROM \"TKEEPINGNAME\" WHERE \"szName\" LIKE :p",
        soci::use(like);

    // BR/BOW shard tables are user-id-keyed (no name prefix). Clean
    // the test user range so successive runs don't see stale shard
    // enrollments. World-pool tables — TBOWPLAYERTABLE often absent
    // on legacy builds.
    for (int uid : { 2000001, 2000002 })
    {
        try
        {
            wsql << "DELETE FROM \"TBRPLAYERTABLE\" WHERE \"dwUserID\" = :u",
                soci::use(uid);
        }
        catch (const std::exception&) { /* table optional */ }
        try
        {
            wsql << "DELETE FROM \"TBOWPLAYERTABLE\" WHERE \"dwUserID\" = :u",
                soci::use(uid);
        }
        catch (const std::exception&) { /* table optional */ }
    }
}

// NetCode.h constants used by tests below.
constexpr std::uint8_t kCountryPeace      = 4;  // TCONTRY_PEACE → start level 1
constexpr std::uint8_t kCountryChoice     = 2;  // TCONTRY_B     → start level 9
constexpr std::uint8_t kPeaceStartLevel   = 1;
constexpr std::uint8_t kChoiceStartLevel  = 9;

tloginsvr::services::CharacterCreateRequest
MakeCreateReq(int user_id, std::uint8_t world,
              const std::string& name, std::uint8_t slot,
              std::uint8_t country = kCountryPeace)
{
    tloginsvr::services::CharacterCreateRequest r{};
    r.user_id    = user_id;
    r.group_id   = world;
    r.name       = name;
    r.slot       = slot;
    r.char_class = 1;
    r.race       = 0;
    r.country    = country;
    r.sex        = 0;
    r.hair       = 1;
    r.face       = 2;
    r.body       = 3;
    r.pants      = 4;
    r.hand       = 5;
    r.foot       = 6;
    return r;
}

void RunTests(fourstory::db::Backend backend,
              const std::string& conn,
              const std::string& world_conn = "")
{
    fourstory::db::SessionPool pool(backend, conn, /*pool_size=*/2);
    std::unique_ptr<fourstory::db::SessionPool> world_pool;
    if (!world_conn.empty())
    {
        world_pool = std::make_unique<fourstory::db::SessionPool>(
            backend, world_conn, /*pool_size=*/2);
    }
    fourstory::db::SessionPool& world_ref =
        world_pool ? *world_pool : pool;

    // Alphanumeric-only — legacy IsValidCharName disallows _ and other
    // punctuation, so the prefix has to satisfy that too.
    const std::string prefix =
        std::string("Soci") + std::to_string(::getpid()) + "X";

    {
        auto glease = pool.Acquire();
        auto wlease = world_ref.Acquire();
        WipeFixtures(*glease, *wlease, prefix);
    }

    // Two-pool layout — when world_conn is empty, world_ref aliases
    // the global pool (legacy PG dev fixture single-DB layout).
    tloginsvr::services::SociCharService svc(pool, world_ref);
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

    // 7b. TKEEPINGNAME LIKE-pattern match. Legacy TGLOBAL.TCreateChar
    //     uses `@szName LIKE szName` so stored rows are patterns
    //     (e.g. 'Mod%' bans every name starting with Mod). Seed a
    //     pattern and try both a name that matches and one that
    //     doesn't. The negative test uses user_b so the resulting
    //     char doesn't perturb user_a's slot/list assertions below.
    //
    //     Names use 2-char suffixes ("Kx" / "Ox") so that even with
    //     a 6-digit PID the total stays under modern's 16-char
    //     IsValidCharName cap (prefix is 9-11 chars; total <= 13).
    //     Earlier "KeptBob" / "OtherBob" suffixes overran the cap
    //     when PIDs grew to 5+ digits.
    {
        {
            auto lease = pool.Acquire();
            soci::session& sql = *lease;
            const std::string pattern = prefix + "K%";
            sql << "INSERT INTO \"TKEEPINGNAME\" (\"szName\") VALUES (:n)",
                soci::use(pattern);
        }
        const auto r_match = svc.Create(
            MakeCreateReq(user_a, world, prefix + "Kx", 1));
        Check(r_match.status == CreateCharResult::Protected,
            "TKEEPINGNAME pattern match → Protected");

        const auto r_miss = svc.Create(
            MakeCreateReq(user_b, world, prefix + "Ox", 0));
        Check(r_miss.status != CreateCharResult::Protected,
            "TKEEPINGNAME non-matching name → not Protected");
    }

    // 8. List returns the created char with starter items. Items
    //     showing up here requires Create + List to agree on
    //     bOwnerType (= TOWNER_CHAR = kOwnerChar = 0). The earlier
    //     INSERT-with-1 bug made items invisible — test 15 catches
    //     the direct SELECT count, but only this round-trip check
    //     guards against a future regression in the CharList path.
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
            Check(!list[0].items.empty(),
                "list returns starter items (Create/List bOwnerType match)");
        }
    }

    // 9. List ignores the group_id arg in the current world-pool
    //    layout — the SOCI service holds a single world pool pinned
    //    to one TGAME database; group_id is a wire-protocol artefact
    //    that doesn't fan-out to multiple world pools. A multi-world
    //    deployment would map group_id → world_pool lookup before
    //    List runs; until that lands, passing a different group_id
    //    still returns the same user's chars from the configured
    //    world. Test that contract so a future regression that
    //    accidentally null-returns on group mismatch gets caught.
    {
        const auto list_same  = svc.List(user_a, world);
        const auto list_other = svc.List(user_a, world + 1);
        Check(list_same.size() == list_other.size(),
            "List ignores group_id (single-world pool layout)");
    }

    // 10. Delete blocked by guild membership. TGUILDMEMBERTABLE is
    //     a world-pool (TGAME) table.
    {
        {
            auto lease = world_ref.Acquire();
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
            auto lease = world_ref.Acquire();
            *lease << "DELETE FROM \"TGUILDMEMBERTABLE\" WHERE \"dwCharID\" = :c",
                soci::use(alice_id);
        }
    }

    // 11. Hard delete (level <= 5). TCHARTABLE is world, TALLCHARTABLE
    //     is global — split the leases.
    {
        const auto r = svc.Delete(user_a, world, alice_id, "ignored");
        Check(r == DeleteCharResult::Success, "delete level-1 char → Success (hard)");

        int char_hits = 0;
        {
            auto wlease = world_ref.Acquire();
            *wlease << "SELECT COUNT(*) FROM \"TCHARTABLE\" WHERE \"dwCharID\" = :c",
                soci::use(alice_id), soci::into(char_hits);
        }
        Check(char_hits == 0, "TCHARTABLE row hard-deleted");
        int all_hits_alive = 0;
        {
            auto glease = pool.Acquire();
            *glease << "SELECT COUNT(*) FROM \"TALLCHARTABLE\" "
                       "WHERE \"dwCharID\" = :c AND \"bDelete\" = 0",
                soci::use(alice_id), soci::into(all_hits_alive);
        }
        Check(all_hits_alive == 0, "TALLCHARTABLE row marked deleted");
    }

    // 12. Soft delete (level > 5). TCHARTABLE is world.
    {
        const auto cr = svc.Create(MakeCreateReq(user_a, world, prefix + "Gamma", 2));
        Check(cr.status == CreateCharResult::Success, "create Gamma → Success");
        const int gamma_id = cr.char_id;

        // Bump level past the threshold.
        {
            auto lease = world_ref.Acquire();
            *lease << "UPDATE \"TCHARTABLE\" SET \"bLevel\" = 10 WHERE \"dwCharID\" = :c",
                soci::use(gamma_id);
        }

        const auto r = svc.Delete(user_a, world, gamma_id, "ignored");
        Check(r == DeleteCharResult::Success, "delete level-10 char → Success (soft)");

        auto lease = world_ref.Acquire();
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

    // 14. Choice-country char starts at CHOICE_COUNTRY_LEVEL (9).
    {
        const auto r = svc.Create(MakeCreateReq(
            user_a, world, prefix + "Epsi", 4, kCountryChoice));
        Check(r.status == CreateCharResult::Success, "create choice-country → Success");
        Check(r.starting_level == kChoiceStartLevel,
            "choice-country starting_level == 9");

        // Verify it landed in the row. TCHARTABLE → world pool.
        auto lease = world_ref.Acquire();
        int stored_level = 0;
        *lease << "SELECT \"bLevel\" FROM \"TCHARTABLE\" WHERE \"dwCharID\" = :c",
            soci::use(r.char_id), soci::into(stored_level);
        Check(stored_level == kChoiceStartLevel,
            "TCHARTABLE.bLevel == 9 for choice-country create");
    }

    // 15. Starter items: a fresh char gets TITEMTABLE rows with the
    //     canonical equipped-items triple (bStorageType=STORAGE_INVEN
    //     (= 0), dwStorageID=INVEN_EQUIP (= 0xFE), bOwnerType=
    //     TOWNER_CHAR (= 0)). Earlier modern revisions used different
    //     values on every column making the rows invisible to the
    //     CharList SELECT.
    {
        const auto r = svc.Create(MakeCreateReq(user_a, world, prefix + "Zeta", 5));
        Check(r.status == CreateCharResult::Success, "create Zeta → Success");

        // TITEMTABLE lives in the world pool.
        auto lease = world_ref.Acquire();
        int item_hits = 0;
        *lease << "SELECT COUNT(*) FROM \"TITEMTABLE\" "
                  "WHERE \"dwOwnerID\"    = :c "
                  "  AND \"bOwnerType\"   = 0 "
                  "  AND \"bStorageType\" = 0 "
                  "  AND \"dwStorageID\"  = 254",
            soci::use(r.char_id), soci::into(item_hits);
        Check(item_hits >= 2,
            "starter items inserted with (STORAGE_INVEN, INVEN_EQUIP, TOWNER_CHAR) triple");

        // Hard delete scrubs the inventory.
        const auto del = svc.Delete(user_a, world, r.char_id, "ignored");
        Check(del == DeleteCharResult::Success, "delete Zeta → Success (hard, level=1)");
        int items_after = 0;
        *lease << "SELECT COUNT(*) FROM \"TITEMTABLE\" "
                  "WHERE \"dwOwnerID\" = :c AND \"bOwnerType\" = 0",
            soci::use(r.char_id), soci::into(items_after);
        Check(items_after == 0, "hard-delete scrubs TITEMTABLE rows");
    }

    // 16. Veteran bonus boosts the starting level above the country floor.
    {
        // Seed a veteran row for level_option=7 that gives level 20.
        const std::uint8_t vet_opt = 7;
        const std::uint8_t vet_level = 20;
        {
            auto lease = pool.Acquire();
            *lease << "DELETE FROM \"TVETERANCHART\" WHERE \"bID\" = :o",
                soci::use(static_cast<int>(vet_opt));
            *lease << "INSERT INTO \"TVETERANCHART\" (\"bID\", \"bLevel\") "
                      "VALUES (:o, :l)",
                soci::use(static_cast<int>(vet_opt)),
                soci::use(static_cast<int>(vet_level));
        }

        // Spin up a fresh service so it reloads the veteran cache.
        SociCharService svc_vet(pool, world_ref);

        auto req = MakeCreateReq(
            user_a, world, prefix + "Vetx", /*slot=*/5, kCountryPeace);
        req.level_option = vet_opt;
        const auto r = svc_vet.Create(req);
        Check(r.status == CreateCharResult::Success,
            "create with veteran level_option → Success");
        Check(r.starting_level == vet_level,
            "veteran bonus overrides country floor (level == 20)");

        auto lease = pool.Acquire();
        *lease << "DELETE FROM \"TVETERANCHART\" WHERE \"bID\" = :o",
            soci::use(static_cast<int>(vet_opt));
    }

    // 17. GetVeteranLevels — three rows in chart → returns them sorted
    //     by bID in the three-slot ack tuple.
    {
        // Seed bID=1/2/3 → levels 11/22/33; bID=99 → 99 (must be ignored).
        {
            auto lease = pool.Acquire();
            soci::session& sql = *lease;
            sql << "DELETE FROM \"TVETERANCHART\"";
            sql << "INSERT INTO \"TVETERANCHART\" (\"bID\", \"bLevel\") VALUES (1, 11)";
            sql << "INSERT INTO \"TVETERANCHART\" (\"bID\", \"bLevel\") VALUES (2, 22)";
            sql << "INSERT INTO \"TVETERANCHART\" (\"bID\", \"bLevel\") VALUES (3, 33)";
            sql << "INSERT INTO \"TVETERANCHART\" (\"bID\", \"bLevel\") VALUES (99, 99)";
        }
        SociCharService svc_chart(pool, world_ref);
        const auto vl = svc_chart.GetVeteranLevels();
        Check(vl.first  == 11, "GetVeteranLevels: first (lowest bID)");
        Check(vl.second == 22, "GetVeteranLevels: second");
        Check(vl.third  == 33, "GetVeteranLevels: third");

        auto lease = pool.Acquire();
        *lease << "DELETE FROM \"TVETERANCHART\"";
    }

    // 18. GetVeteranLevels — empty chart → all zeros.
    {
        SociCharService svc_empty(pool, world_ref);
        const auto vl = svc_empty.GetVeteranLevels();
        Check(vl.first == 0 && vl.second == 0 && vl.third == 0,
            "GetVeteranLevels: empty chart → 0/0/0");
    }

    // 19. GetBrCharId / GetBowCharId — shard enrollment lookup. Mirrors
    //     legacy TFindBRPlayer / TFindBOWPlayer SPs. Used by the
    //     CharList handler to send CS_BOWPLAYERNOTIFY_ACK with the
    //     enrolled char's slot. Lookup contract: BOW first; fall
    //     through to BR if BOW returns 0.
    {
        const int br_char  = 8001;
        const int bow_char = 8002;

        // No enrollment → both return 0.
        Check(svc.GetBrCharId(user_a) == 0,
            "GetBrCharId: no enrollment → 0");
        Check(svc.GetBowCharId(user_a) == 0,
            "GetBowCharId: no enrollment → 0");
        Check(svc.GetBrCharId(0) == 0,
            "GetBrCharId(0) → 0 (no DB hit)");
        Check(svc.GetBowCharId(0) == 0,
            "GetBowCharId(0) → 0 (no DB hit)");

        // BR-only enrollment. TBR/BOWPLAYERTABLE are world-pool
        // (TGAME) tables. TBOWPLAYERTABLE is not in every legacy
        // world DB — wrap the BOW INSERT so the BOW-half of the
        // test SKIPs cleanly on those deploys.
        {
            auto lease = world_ref.Acquire();
            *lease << "INSERT INTO \"TBRPLAYERTABLE\" "
                      "(\"dwCharID\", \"dwUserID\") VALUES (:c, :u)",
                soci::use(br_char), soci::use(user_a);
        }
        Check(svc.GetBrCharId(user_a) == br_char,
            "GetBrCharId: BR enrolled → returns char_id");
        Check(svc.GetBowCharId(user_a) == 0,
            "GetBowCharId: BR enrolled but not BOW → still 0");

        // BOW enrollment added — handler will prefer this on charlist.
        bool bow_seeded = false;
        try
        {
            auto lease = world_ref.Acquire();
            *lease << "INSERT INTO \"TBOWPLAYERTABLE\" "
                      "(\"dwCharID\", \"dwUserID\") VALUES (:c, :u)",
                soci::use(bow_char), soci::use(user_a);
            bow_seeded = true;
        }
        catch (const std::exception&) { /* TBOWPLAYERTABLE absent */ }

        if (bow_seeded)
        {
            Check(svc.GetBowCharId(user_a) == bow_char,
                "GetBowCharId: BOW enrolled → returns char_id");
            // BR still returns its enrollment — both lookups
            // independent; the legacy "BOW takes priority" rule
            // lives in the handler.
            Check(svc.GetBrCharId(user_a) == br_char,
                "GetBrCharId: BR still returns char_id when BOW also set");
        }
        else
        {
            std::printf("  SKIP  BOW enrollment test — TBOWPLAYERTABLE "
                        "not deployed\n");
        }

        // Cleanup before next test.
        {
            auto lease = world_ref.Acquire();
            *lease << "DELETE FROM \"TBRPLAYERTABLE\" WHERE \"dwUserID\" = :u",
                soci::use(user_a);
            try
            {
                *lease << "DELETE FROM \"TBOWPLAYERTABLE\" WHERE \"dwUserID\" = :u",
                    soci::use(user_a);
            }
            catch (const std::exception&) { /* optional */ }
        }
    }

    // Cleanup.
    try
    {
        auto glease = pool.Acquire();
        auto wlease = world_ref.Acquire();
        WipeFixtures(*glease, *wlease, prefix);
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
                ? mssql_world_conn : "";
        if (world_conn.empty())
        {
            std::printf("  NOTE  TLOGINSVR_TEST_MSSQL_WORLD_CONN unset — "
                        "world-pool ops will share the global pool\n");
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
