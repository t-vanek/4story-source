#pragma once

#include "npc_service.h"
#include "quest_chart.h"
#include "stat_chart.h"
#include "monster_chart.h"
#include "spawn_chart.h"
#include "map_mon_chart.h"
#include "mon_attr_chart.h"
#include "mon_item_chart.h"
#include "skill_chart.h"
#include "skill_data_chart.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

// A complete immutable boot snapshot. Ownership transfers to the existing
// HandlerContext services only after every chart and contract has loaded.
struct PostgreSQLCatalog
{
    std::int64_t run_id = 0;
    std::string manifest_sha256;
    std::map<std::string, std::int64_t> source_counts;
    std::size_t missing_monster_attributes = 0;
    std::size_t unresolved_quest_terms = 0;
    std::size_t unresolved_quest_rewards = 0;
    std::unique_ptr<IStatChart> stats;
    std::unique_ptr<INpcService> npcs;
    std::unique_ptr<IQuestChart> quests;
    std::unique_ptr<IMonsterChart> monsters;
    std::unique_ptr<ISpawnChart> spawns;
    std::unique_ptr<IMapMonChart> map_mon;
    std::unique_ptr<IMonAttrChart> attributes;
    std::unique_ptr<IMonItemChart> drops;
    std::unique_ptr<ISkillTemplateChart> skills;
    std::unique_ptr<ISkillDataChart> skill_data;
};

// Requires native PostgreSQL and an explicit 64-character manifest fingerprint.
// Uses one pooled connection and one read-only repeatable-read transaction for
// all metadata, all fifteen catalog contracts and all fourteen chart queries.
// Does not activate content or grant permissions, and has no live reload path.
PostgreSQLCatalog LoadPostgreSQLCatalog(fourstory::db::SessionPool& pool,
                                      const std::string& expected_manifest);

} // namespace tmapsvr
