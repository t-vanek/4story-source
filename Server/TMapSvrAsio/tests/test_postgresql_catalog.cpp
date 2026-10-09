#include "services/postgresql_catalog.h"
#include "services/spawn_manager.h"
#include "services/monster_registry.h"
#include "fourstory/db/session_pool.h"
#include <soci/soci.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {
int failures = 0;
void Check(bool value, const char* label)
{
    std::printf("%s %s\n", value ? "PASS" : "FAIL", label);
    if (!value) ++failures;
}
}

int main()
{
    const char* options = std::getenv("FOURSTORY_CATALOG_PG_CONNINFO");
    const char* manifest = std::getenv("FOURSTORY_CATALOG_MANIFEST");
    if (!options || !manifest)
    {
        std::puts("SKIP: verified backup-derived native catalog fixture not configured");
        return 77;
    }
    try
    {
        fourstory::db::SessionPool pool(fourstory::db::Backend::PostgreSQL, options, 1);
        bool wrong_manifest = false;
        try { auto rejected = tmapsvr::LoadPostgreSQLCatalog(pool, std::string(64, '0')); }
        catch (const std::runtime_error&) { wrong_manifest = true; }
        Check(wrong_manifest, "unselected manifest is refused without fallback");
        auto catalog = tmapsvr::LoadPostgreSQLCatalog(pool, manifest);
        std::int64_t source_rows = 0;
        for (const auto& [table, count] : catalog.source_counts) source_rows += count;
        Check(catalog.source_counts.size() == 15 && source_rows == 106692, "all fifteen pinned backup tables verified");
        Check(catalog.stats->FormulaCount() == 34 && catalog.stats->ClassCount() == 6 && catalog.stats->RaceCount() == 88,
              "real C++ stat caches loaded");
        Check(catalog.npcs->Size() == 2623 && catalog.quests->Size() == 6261, "real C++ NPC and quest caches loaded");
        Check(catalog.monsters->Size() == 3534 && catalog.spawns->Size() == 20357 && catalog.map_mon->Size() == 23381,
              "real C++ monster and spawn caches loaded");
        Check(catalog.attributes->Size() == 3495 && catalog.missing_monster_attributes == 39, "source attribute mappings and all gaps retained");
        Check(catalog.drops->Size() == 16466 && catalog.skills->Size() == 765 && catalog.skill_data->Size() == 1172,
              "real C++ loot and skill caches loaded");
        Check(catalog.unresolved_quest_terms == 0 && catalog.unresolved_quest_rewards == 62,
              "unmatched original quest rewards are observable");
        {
            auto lease = pool.Acquire();
            int id=0, level=0, hp=0, ap=0, dp=0;
            soci::statement st = (lease->prepare <<
                "SELECT wid,blevel,dwmaxhp,wap,wdp FROM game_compat.tmonattrchart",
                soci::into(id), soci::into(level), soci::into(hp), soci::into(ap), soci::into(dp));
            st.execute(false);
            bool equal = true;
            std::size_t rows = 0;
            while (st.fetch())
            {
                auto value = catalog.attributes->Find(static_cast<std::uint16_t>(id), static_cast<std::uint8_t>(level));
                equal = equal && value && value->dwMaxHP == static_cast<std::uint32_t>(hp)
                    && value->wAP == static_cast<std::uint16_t>(ap) && value->wDP == static_cast<std::uint16_t>(dp);
                ++rows;
            }
            Check(equal && rows == 3495, "native DB-to-domain attribute values retain protocol-width bits");
            bool denied = false;
            try { *lease << "SELECT 1 FROM legacy_game.\"TMONSTERCHART\""; }
            catch (const soci::soci_error&) { denied = true; }
            Check(denied, "catalog reader cannot access original snapshots");
            denied = false;
            try { *lease << "UPDATE game_compat.tmonattrchart SET dwmaxhp=1"; }
            catch (const soci::soci_error&) { denied = true; }
            Check(denied, "catalog reader cannot overwrite combat data");
        }
        tmapsvr::InMemoryMonsterRegistry registry;
        std::uint32_t next_id = 1;
        const auto spawned = tmapsvr::SpawnAllStatic(*catalog.spawns, *catalog.map_mon,
            *catalog.monsters, *catalog.attributes, registry, next_id);
        bool faithful = true;
        for (const auto& monster : registry.All())
        {
            const auto tmpl = catalog.monsters->Find(monster.wTemplateID);
            const auto attr = tmpl ? catalog.attributes->Find(tmpl->wID, tmpl->bLevel) : std::nullopt;
            faithful = faithful && attr && attr->dwMaxHP != 0 && monster.dwHP == attr->dwMaxHP;
        }
        Check(faithful && spawned > 0 && next_id == spawned + 1, "actual spawn population uses only recovered combat HP");
        std::printf("Native catalog integration: run=%lld source_rows=%lld spawned=%zu\n",
            static_cast<long long>(catalog.run_id), static_cast<long long>(source_rows), spawned);
    }
    catch (const std::exception&)
    {
        std::puts("FAIL: native catalog integration failed (backend detail suppressed)");
        return 1;
    }
    return failures ? 1 : 0;
}
