#pragma once

#include "battle_regions.h"
#include "domain/monster.h"
#include "native_map_cells.h"
#include "native_monster_presentation.h"
#include "domain/skill.h"
#include "domain/stat.h"
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <stdexcept>
#include <vector>

namespace tmapsvr {

// Source members consumed by InitMonster and fresh-world Initialize/Regen.
// This is not the complete combat/loot/pathfinding template.
struct NativeMonsterAllocationTemplate {
    std::uint16_t wID{},wMonAttr{};
    std::uint8_t bLevel{},bClass{},bRace{};
    std::array<std::uint16_t,4> skills{};
    std::uint8_t ai_type{},can_attack{},special{},remove{};
    float size{};
};
struct NativeMonsterBaseVitals {std::uint32_t hp{},mp{};};

// CTBLMonSpawn order and each parameterized CTBLMapMon/CTBLMonParty order
// are independent evidence. Archival ordinal order is not query order.
struct NativeMonsterCatalog {
    std::map<std::uint16_t,SpawnPoint> spawns;
    std::map<std::uint16_t,std::vector<std::uint16_t>> groups;
    // CTBLMonParty is executed once PER SPAWN, even for equal party IDs.
    std::map<std::uint16_t,std::vector<std::uint16_t>> parties;
    std::map<std::uint16_t,std::vector<MapMonEntry>> candidates;
    std::map<std::uint16_t,NativeMonsterAllocationTemplate> monsters;
    std::map<std::uint32_t,NativeMonsterBaseVitals> attributes; // MAKELONG(attribute FAMILY,level)
    std::map<std::uint16_t,SkillTemplate> skills;
    std::map<std::uint16_t,std::vector<SkillDataRow>> skill_data; // exact CTBLSkillData query order; present-empty is explicit
    std::shared_ptr<const SourceSkillDataCatalog> player_skill_data;
    std::string skill_data_order_sha256;
    std::set<std::uint16_t> items,extra_spawn_ids;
    std::set<std::uint8_t> levels;
    std::map<std::uint8_t,StatBlock> classes,races;
    std::map<std::uint8_t,FormulaRow> formulas;
    std::string order_sha256;
    std::string manifest;
    std::int64_t run_id{};
};

// Owned source learned object; source AI SkillUse updates these DWORD clocks.
struct NativeMonsterSkill {
    std::uint8_t level{1};
    std::uint32_t use_tick{},delay_tick{}; // CTSkill constructor, original DWORD reuse clocks
};
struct NativeMonsterAggro {
    std::uint8_t object_type{},country{3};
    std::uint32_t object{},host{},value{}; // source TAGGRO, actual mutation producers remain separate
};
// Owned source allocation/bootstrap state. A missing template is anonymous,
// never template0. Bootstrap cell membership is not another prototype registry.
// Populated actor admission remains barred until the complete source gameplay
// producer/consumer chain exists; conditional AI reads use these owned fields.
struct NativeMonsterSlot {
    std::uint64_t generation{}; // process-local lifetime, independent of reused source DWORD ID
    std::uint32_t id{},region{};
    std::uint16_t spawn{},map{};
    std::uint8_t channel{},country{3},aid_country{3},status{}; // CTObjBase neutral; CTMonster DISAPPEAR0
    bool essential{},default_inventory_template_present{};
    std::optional<std::uint16_t> monster;
    std::optional<std::uint32_t> attribute;
    std::optional<std::uint8_t> level_template,class_template,race_template;
    std::optional<std::uint8_t> level;
    std::map<std::uint16_t,NativeMonsterSkill> skills; // source WORD map; duplicate insert retains object/clocks
    std::uint8_t mode{},action{},target_type{1},help_count{},path_id{},path_direction{1};
    std::uint32_t host_key{},host{},target{},helper{},hp{},mp{},keeper{};
    std::uint32_t max_hp{},max_mp{},money{};
    bool maxima_ready{}; // actual derived-maxima producer, never missing object -> zero bars
    std::vector<NativeMonsterMaintain> maintained; // CTSkill vector order and raw DWORD clocks
    // CTObjBase constructor establishes actual empty remain vector/null
    // instance. These are distinct from learned skills (InitMonster adds no
    // remain membership). Full Defend/PerformSkill producers remain unported.
    std::vector<NativeMonsterMaintain> remaining;
    std::optional<NativeMonsterMaintain> instance_skill;
    std::optional<std::uint32_t> last_attack_tick,attack_wait_tick;
    std::uint32_t recover_hp_tick{},recover_mp_tick{}; // CTObjBase constructor0; ChgMode battle tick+5000
    std::map<std::int64_t,NativeMonsterAggro> aggro;
    std::map<std::int64_t,std::int64_t> damage; // source MAPINT64 keys AND values
    std::map<std::uint32_t,std::uint32_t> guild_damage;
    std::set<std::uint32_t> cell_memberships; // lifetime-owned indexes, including source disabled-old-cell remnants
    std::optional<std::uint8_t> leave_count;
    std::optional<std::uint16_t> next_skill;
    std::uint8_t keeper_type{};
    std::uint16_t pitch{},direction{},regen_delete_spawn{};
    std::uint8_t mouse{4},key{4}; // TKDIR_N, initialized by CTObjBase
    bool remove{},entered_map{}; // EnterMAP publication, not existence of the owning map
    bool getaway{}; // CTObjBase constructor FALSE; separate Getaway command remains unported
    std::uint8_t can_attack{1},special{};
    float x{},y{},z{},start_x{},start_y{},start_z{},next_x{},next_y{},next_z{},size{0.5f}; // OBJECT_SIZE/2
    // Original default inventory always exists and initially has no items;
    // template3 presence is separate. Bootstrap cannot accept populated items,
    // maintained effects, host/aggro/damage or deferred regen-delete state.
};
struct NativeMonsterSpawn {
    std::uint16_t id{};
    std::uint8_t regen_type{},status{}; // ETERNAL0/WAITSPAWN1; READY0
    std::uint32_t leader{};
    std::vector<std::uint32_t> slots;
    std::optional<std::uint16_t> missing_essential_attribute;
};
struct NativeMonsterMap {
    std::uint16_t map_id{};
    std::uint8_t physical_channel{};
    std::uint16_t party{}; // ordinary source maps are party0; no synthetic instance map
    NativeMapCells cells;
    std::map<std::uint32_t,NativeCellPlayer> players; // map affiliation, even outside enabled cells
    std::map<std::uint16_t,NativeMonsterSpawn> spawns;
    std::map<std::uint32_t,NativeMonsterSlot> slots;
};
using NativeMonsterMaps=std::map<std::uint32_t,NativeMonsterMap>;

class NativeMonsterInitializationRequired final : public std::runtime_error {
public:
    NativeMonsterInitializationRequired(std::uint16_t map,std::uint8_t channel,std::uint32_t monster,
                                        const std::string& boundary)
        :std::runtime_error("Native monster integration requires "+boundary+"; map="+
            std::to_string(map)+" physical_channel="+std::to_string(channel)+" monster="+std::to_string(monster)){}
};

// Initialize is synchronous at each source insertion boundary, before the
// next slot exists. The caller must execute it or throw: collecting deferred
// callbacks would change group redirection and leader visibility in Regen.
using NativeMonsterInitialize=std::function<void(NativeMonsterMap&,std::uint32_t)>;
void AddNativeDefaultSpawns(const NativeMonsterCatalog&,const BattleRegions&,
                           NativeMonsterMaps&,std::uint64_t& generation,const NativeMonsterInitialize&);
} // namespace tmapsvr
