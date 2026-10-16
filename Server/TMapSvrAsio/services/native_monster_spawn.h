#pragma once

#include "battle_regions.h"
#include "domain/monster.h"
#include <array>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <stdexcept>
#include <vector>

namespace tmapsvr {

// Only the template members consumed by CTMap::InitMonster. This projection
// must not be passed as a complete combat/AI monster template.
struct NativeMonsterAllocationTemplate {
    std::uint16_t wID{},wMonAttr{};
    std::uint8_t bLevel{},bClass{},bRace{};
    std::array<std::uint16_t,4> skills{};
};

// CTBLMonSpawn order and each parameterized CTBLMapMon/CTBLMonParty order
// are independent evidence. Archival ordinal order is not query order.
struct NativeMonsterCatalog {
    std::map<std::uint16_t,SpawnPoint> spawns;
    std::map<std::uint16_t,std::vector<std::uint16_t>> groups;
    // CTBLMonParty is executed once PER SPAWN, even for equal party IDs.
    std::map<std::uint16_t,std::vector<std::uint16_t>> parties;
    std::map<std::uint16_t,std::vector<MapMonEntry>> candidates;
    std::map<std::uint16_t,NativeMonsterAllocationTemplate> monsters;
    std::set<std::uint32_t> attributes; // MAKELONG(attribute FAMILY,level)
    std::set<std::uint16_t> skills,items,extra_spawn_ids;
    std::set<std::uint8_t> levels,classes,races;
    std::string order_sha256;
    std::string manifest;
    std::int64_t run_id{};
};

// Allocation state only. A slot with no template is an actual anonymous
// source slot, never a monster with template0. None of these objects may be
// projected into the live MonsterInstance registry before Initialize/AI.
struct NativeMonsterSlot {
    std::uint32_t id{},region{};
    std::uint16_t spawn{},map{};
    std::uint8_t channel{},country{},status{}; // CTMonster starts DISAPPEAR0
    bool essential{},default_inventory_template_present{};
    std::optional<std::uint16_t> monster;
    std::optional<std::uint32_t> attribute;
    std::optional<std::uint8_t> level_template,class_template,race_template;
    std::optional<std::uint8_t> level;
    std::set<std::uint16_t> skills; // original existing templates, level1
    // Original default inventory exists even if item-template3 is absent.
};
struct NativeMonsterSpawn {
    std::uint16_t id{};
    std::uint8_t regen_type{},status{}; // ETERNAL0/WAITSPAWN1; READY0
    std::uint32_t leader{};
    std::vector<std::uint32_t> slots;
    std::optional<std::uint16_t> missing_essential_attribute;
};
struct NativeMonsterMap {
    std::map<std::uint16_t,NativeMonsterSpawn> spawns;
    std::map<std::uint32_t,NativeMonsterSlot> slots;
};
using NativeMonsterMaps=std::map<std::uint32_t,NativeMonsterMap>;

class NativeMonsterInitializationRequired final : public std::runtime_error {
public:
    NativeMonsterInitializationRequired(std::uint16_t map,std::uint8_t channel,std::uint32_t monster)
        :std::runtime_error("Native source CTMonster::Initialize/AI is not implemented; map="+
            std::to_string(map)+" physical_channel="+std::to_string(channel)+" monster="+std::to_string(monster)){}
};

// Initialize is synchronous at each source insertion boundary, before the
// next slot exists. The caller must execute it or throw: collecting deferred
// callbacks would change group redirection and leader visibility in Regen.
using NativeMonsterInitialize=std::function<void(NativeMonsterMap&,std::uint32_t)>;
void AddNativeDefaultSpawns(const NativeMonsterCatalog&,const BattleRegions&,
                           NativeMonsterMaps&,const NativeMonsterInitialize&);
} // namespace tmapsvr
