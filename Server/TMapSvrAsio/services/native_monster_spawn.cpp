#include "native_monster_spawn.h"
#include <stdexcept>
#include <utility>
#include <limits>

namespace tmapsvr {
namespace {
void InitMonster(const NativeMonsterCatalog& catalog,const SpawnPoint& source,
                 std::uint8_t channel,NativeMonsterMap& map,NativeMonsterSpawn& spawn,
                 std::uint64_t& generation,const NativeMonsterInitialize& initialize) {
    const auto& candidates=catalog.candidates.at(source.wID);
    if(candidates.empty())return; // source: even ordinary slots are absent
    unsigned index=0;
    const auto insert=[&](NativeMonsterSlot slot,bool leader) {
        // The source BYTE index wraps and aliases a prior ID. Refuse that
        // invalid graph instead of retaining an unindexed/leaked monster.
        if(index>255)throw std::runtime_error("Source monster slot BYTE overflow");
        if(generation==std::numeric_limits<std::uint64_t>::max())throw std::runtime_error("Native monster generation exhausted");
        slot.generation=++generation;
        slot.id=(std::uint32_t{source.wID}<<16)|(std::uint32_t{channel}<<8)|index++;
        slot.spawn=source.wID;slot.map=source.wMapID;slot.channel=channel;
        slot.country=source.bCountry;slot.region=source.dwRegion;
        slot.default_inventory_template_present=catalog.items.contains(3);
        const auto id=slot.id;
        if(!map.slots.emplace(id,std::move(slot)).second)
            throw std::runtime_error("Duplicate source monster instance ID");
        spawn.slots.push_back(id);
        if(leader)spawn.leader=id;
        if(spawn.regen_type==0)initialize(map,id);
    };
    for(unsigned i=0;i<source.bCount;++i)insert(NativeMonsterSlot{},false);
    for(const auto& candidate:candidates)if(candidate.bEssential) {
        const auto found=catalog.monsters.find(candidate.wMonID);
        if(found==catalog.monsters.end())continue; // no index consumed
        const auto& monster=found->second;
        const auto attribute=std::uint32_t{monster.wMonAttr}|(std::uint32_t{monster.bLevel}<<16);
        if(!catalog.attributes.contains(attribute)) {
            spawn.missing_essential_attribute=monster.wID;
            return; // entire InitMonster, retaining earlier inserted slots
        }
        NativeMonsterSlot slot;slot.essential=true;slot.monster=monster.wID;
        slot.attribute=attribute;slot.level=monster.bLevel;
        if(catalog.levels.contains(monster.bLevel))slot.level_template=monster.bLevel;
        if(catalog.classes.contains(monster.bClass))slot.class_template=monster.bClass;
        if(catalog.races.contains(monster.bRace))slot.race_template=monster.bRace;
        for(const auto skill:monster.skills)if(catalog.skills.contains(skill))slot.skills.try_emplace(skill);
        insert(std::move(slot),candidate.bLeader!=0);
    }
}
}
void AddNativeDefaultSpawns(const NativeMonsterCatalog& catalog,const BattleRegions& environment,
                           NativeMonsterMaps& maps,std::uint64_t& generation,const NativeMonsterInitialize& initialize) {
    if(!initialize)throw std::invalid_argument("Source Initialize consumer required");
    // Source iterates spawn WORD map, then physical channel BYTE map.
    // Environment's packed map/channel keys must not reverse that order.
    std::map<std::uint8_t,std::map<std::uint16_t,const BattleMapEnvironment*>> channels;
    for(const auto& [key,map]:environment.maps)
        channels[static_cast<std::uint8_t>(key>>16)].emplace(static_cast<std::uint16_t>(key),&map);
    for(const auto& [id,source]:catalog.spawns) {
        if(source.bEvent!=0)continue; // nondefault templates remain catalogued
        for(const auto& [channel,channel_maps]:channels) {
            const auto found=channel_maps.find(source.wMapID);
            if(found==channel_maps.end())continue;
            const auto key=std::uint32_t{source.wMapID}|(std::uint32_t{channel}<<16);
            auto& map=maps[key];
            map.map_id=source.wMapID;map.physical_channel=channel;
            NativeMonsterSpawn spawn;spawn.id=id;
            if(source.wGroup) {
                const auto& group=catalog.groups.at(source.wGroup);
                if(!group.empty()&&group.back()!=id)spawn.regen_type=1;
            }
            auto [position,inserted]=map.spawns.emplace(id,std::move(spawn));
            if(!inserted)continue;
            if(!found->second->instance_template)
                InitMonster(catalog,source,channel,map,position->second,generation,initialize);
        }
    }
}
} // namespace tmapsvr
