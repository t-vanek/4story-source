#include "postgresql_catalog.h"

#include "soci_npc_service.h"
#include "soci_quest_chart.h"
#include "soci_stat_chart.h"
#include "soci_monster_chart.h"
#include "soci_spawn_chart.h"
#include "soci_map_mon_chart.h"
#include "soci_mon_attr_chart.h"
#include "soci_mon_item_chart.h"
#include "soci_skill_chart.h"
#include "soci_skill_data_chart.h"
#include "fourstory/db/session_pool.h"

#include <soci/soci.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <stdexcept>
#include <string_view>

namespace tmapsvr {

PostgreSQLCatalog LoadPostgreSQLCatalog(fourstory::db::SessionPool& pool,
                                      const std::string& expected_manifest)
{
    using fourstory::db::Backend;
    if (pool.GetBackend() != Backend::PostgreSQL)
        throw std::runtime_error("Catalog snapshot requires native PostgreSQL");
    if (expected_manifest.size() != 64 || !std::all_of(expected_manifest.begin(),
            expected_manifest.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
        throw std::runtime_error("content.manifest_sha256 must be an explicit lowercase SHA256 fingerprint");

    auto lease = pool.Acquire();
    auto& sql = *lease;
    soci::transaction tx(sql);
    sql << "SET TRANSACTION ISOLATION LEVEL REPEATABLE READ, READ ONLY";
    sql << "SET LOCAL statement_timeout = '30s'";
    sql << "SET LOCAL lock_timeout = '5s'";
    sql << "SET LOCAL search_path TO pg_catalog, game_compat";

    PostgreSQLCatalog result;
    long long run = 0;
    std::string status;
    sql << "SELECT run_id,manifest_sha256,status FROM game_compat.catalog_release",
        soci::into(run), soci::into(result.manifest_sha256), soci::into(status);
    if (!sql.got_data() || status != "verified" || result.manifest_sha256 != expected_manifest)
        throw std::runtime_error("Active catalog does not match the configured verified manifest");
    result.run_id = run;

    constexpr std::array<const char*, 15> names = {
        "TCLASSCHART", "TFORMULACHART", "TITEMCHART", "TMAPMONCHART",
        "TMONATTRCHART", "TMONITEMCHART", "TMONSPAWNCHART", "TMONSTERCHART",
        "TNPCCHART", "TQREWARDCHART", "TQUESTCHART", "TQUESTTERMCHART",
        "TRACECHART", "TSKILLCHART", "TSKILLDATA"
    };
    soci::rowset<soci::row> checkpoints = (sql.prepare <<
        "SELECT source_table,row_count FROM game_compat.catalog_source_tables "
        "WHERE source_sha256=target_sha256 AND source_bytes_sha256=target_bytes_sha256");
    for (const auto& row : checkpoints)
        result.source_counts.emplace(row.get<std::string>(0), row.get<long long>(1));
    if (result.source_counts.size() != names.size())
        throw std::runtime_error("Catalog requires all fifteen reconciled source tables and original text bytes");
    long long attribute_rows = 0;
    for (const char* name : names)
    {
        const auto expected = result.source_counts.find(name);
        if (expected == result.source_counts.end())
            throw std::runtime_error("Catalog source table contract is incomplete");
        // Identifiers come only from this fixed contract list, never configuration.
        long long rows = 0;
        sql << std::string("SELECT count(*) FROM game_compat.\"") + name + "\"", soci::into(rows);
        if (std::string_view(name) == "TMONATTRCHART") attribute_rows = rows;
        else if (rows != expected->second)
            throw std::runtime_error(std::string("Catalog row-count mismatch: ") + name);
    }
    long long gaps = 0;
    sql << "SELECT count(*) FROM game_compat.catalog_missing_monster_attributes", soci::into(gaps);
    if (attribute_rows + gaps != result.source_counts.at("TMONSTERCHART"))
        throw std::runtime_error("Catalog monster attribute cardinality mismatch");
    result.missing_monster_attributes = static_cast<std::size_t>(gaps);

    result.stats = std::make_unique<SociStatChart>(sql);
    result.npcs = std::make_unique<SociNpcService>(sql);
    auto quests = std::make_unique<SociQuestChart>(sql);
    result.unresolved_quest_terms = quests->UnresolvedTerms();
    result.unresolved_quest_rewards = quests->UnresolvedRewards();
    result.quests = std::move(quests);
    result.monsters = std::make_unique<SociMonsterChart>(sql);
    result.spawns = std::make_unique<SociSpawnChart>(sql);
    result.map_mon = std::make_unique<SociMapMonChart>(sql);
    result.attributes = std::make_unique<SociMonAttrChart>(sql);
    result.drops = std::make_unique<SociMonItemChart>(sql);
    result.skills = std::make_unique<SociSkillChart>(sql, result.stats->Formula(FTYPE_1ST).fRateX);
    result.skill_data = std::make_unique<SociSkillDataChart>(sql);
    if (result.attributes->Size() != static_cast<std::size_t>(attribute_rows))
        throw std::runtime_error("Catalog monster attribute keys collapsed during C++ conversion");
    tx.commit();
    spdlog::info("PostgreSQL catalog ready: run={} manifest={} source_tables={} "
                 "monster_attributes={} unresolved_monster_attributes={} "
                 "unresolved_quest_terms={} unresolved_quest_rewards={}",
        result.run_id, result.manifest_sha256, result.source_counts.size(),
        result.attributes->Size(), result.missing_monster_attributes,
        result.unresolved_quest_terms, result.unresolved_quest_rewards);
    return result;
}

} // namespace tmapsvr
