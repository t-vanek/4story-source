#include "native_monster_ai.h"
#include "native_monster_skills.h"
#include <bit>
#include <optional>
#include <utility>

namespace tmapsvr {
namespace {
std::int64_t Key(std::uint32_t object,std::uint8_t type) {
    // TMapType::MAKEINT64(object,type). Preserve signed map ordering without
    // shifting a signed value into its sign bit (undefined in the source).
    return std::bit_cast<std::int64_t>((std::uint64_t{object}<<32)|type);
}
void AddAggro(NativeMonsterSlot& slot,std::uint32_t host,std::uint32_t object,
              std::uint8_t type,std::uint8_t country,std::uint32_t amount) {
    const auto key=Key(object,type);
    const auto found=slot.aggro.find(key);
    if(found!=slot.aggro.end())found->second.value+=amount; // DWORD wrap; preserve existing host/country
    else slot.aggro.emplace(key,NativeMonsterAggro{type,country,object,host,amount});
}
bool Neighbor(const NativeMonsterMap& map,const NativeMonsterSlot& slot,const NativeMonsterAggro& aggro) {
    const auto position=NativeCellAt(slot.x,slot.z);
    if(!position)throw std::runtime_error("Native LeaveAggro requires source WORD position");
    if(aggro.object_type==1)return FindNativeMonsterHost(map,slot.x,slot.z,aggro.object).has_value();
    if(aggro.object_type==7||aggro.object_type==11||aggro.object_type==18)
        throw NativeMonsterInitializationRequired(slot.map,slot.channel,slot.id,"actual recall/self/companion neighbor lifetime for LeaveAggro");
    if(aggro.object_type==2)for(auto key:NativeNeighborCells(map.cells,*position)) {
        if(!map.cells.at(key).monsters.contains(aggro.object))continue;
        if(!map.slots.contains(aggro.object))throw std::runtime_error("Native monster neighbor index outlived its owned object");
        return true; // FindNeighbor does not apply FindMonster's template/status filter
    }
    return false; // source FindNeighbor switch has no lookup for other object types
}
}
bool NativeMonsterAiExecutor::CheckAttack(const NativeMonsterSlot& slot) const {
    if(slot.getaway||!slot.can_attack)return false;
    if(!slot.maintained.empty())throw NativeMonsterInitializationRequired(slot.map,slot.channel,slot.id,
        "maintained status/AI metadata for source CheckAttack");
    return true; // exact empty maintained-vector branch, not an assumed absence of effects
}
bool NativeMonsterAiExecutor::LeaveAggro(NativeMonsterMap& map,std::uint32_t id,Arguments args) {
    const auto generation=map.slots.at(id).generation;
    bool recursive=false;
    for(;;) {
        Step();
        const auto current=map.slots.find(id);
        if(current==map.slots.end()||current->second.generation!=generation)return false;
        auto& slot=current->second;
        const auto found=slot.aggro.find(Key(args.real_host,args.real_type));
        if(found!=slot.aggro.end())slot.aggro.erase(found);
        else if(args.real_host==slot.target&&args.real_type==slot.target_type) {
            Event(map,id,2,0,args);return false; // AT_LEAVELB before even the GOHOME test
        }
        std::uint32_t value=0;
        std::optional<NativeMonsterAggro> best;
        for(const auto& [key,aggro]:slot.aggro)if(value<aggro.value&&aggro.country!=slot.country) {
            value=aggro.value;best=aggro; // strict comparison; signed-key tie order; raw country, not war country
        }
        if(slot.mode==2)return false;
        if(!best||!best->object) {Event(map,id,2,0,args);return false;}
        const Arguments target{best->host,best->object,best->object_type};
        if(Neighbor(map,slot,*best)) {
            if(best->object_type!=slot.target_type||best->object!=slot.target) {
                Event(map,id,5,0,target); // AT_DEFEND may synchronously remove or retarget this object
                return !recursive;
            }
            return false;
        }
        // Source recursively removes the absent best target, then discards
        // that recursive result and returns FALSE. Iterate without stack growth
        // while keeping the same callback arguments and outer return value.
        args=target;recursive=true;
    }
}
void NativeMonsterAiExecutor::SetEventToFollower(NativeMonsterMap& map,std::uint32_t id) {
    const auto& initial=map.slots.at(id);
    const auto generation=initial.generation;
    const auto spawn_id=initial.spawn;
    const bool leader=map.spawns.at(spawn_id).leader==id;
    const auto notify=[&](std::uint16_t spawn) {
        // Source re-reads the vector length and indexed member after each event.
        // No borrowed iterator/reference survives synchronous Remove/Regen.
        for(std::size_t index=0;;++index) {
            Step();
            const auto source=map.slots.find(id);
            if(source==map.slots.end()||source->second.generation!=generation)return false;
            const auto group=map.spawns.find(spawn);
            if(group==map.spawns.end()||index>=group->second.slots.size())break;
            const auto member=map.slots.find(group->second.slots[index]);
            if(member==map.slots.end())throw std::runtime_error("Native follower spawn member has no owned object");
            if(member->second.monster&&member->second.status!=0&&!member->second.target)
                Event(map,member->first,5,0,{source->second.host,source->second.target,source->second.target_type});
        }
        return true;
    };
    if(leader) { (void)notify(spawn_id);return; } // source leader branch wins even if a party is present
    if(m_catalog.spawns.at(spawn_id).wPartyID)
        for(auto spawn:m_catalog.parties.at(spawn_id))if(!notify(spawn))return;
}
bool NativeMonsterAiExecutor::ChangeHost(NativeMonsterMap& map,std::uint32_t id,const NativeAiBinding& binding,Arguments args) {
    auto& slot=map.slots.at(id);
    const auto generation=slot.generation;
    const auto old=FindNativeMonsterHost(map,slot.x,slot.z,slot.host);
    const auto next=FindNativeMonsterHost(map,slot.x,slot.z,args.event_host);
    if(!next) {
        ++slot.host_key;
        if(LeaveAggro(map,id,args))return false;
        const auto current=map.slots.find(id);
        if(current==map.slots.end()||current->second.generation!=generation)return false;
        ResetHost(map,id);Event(map,id,7,0,{}); // AT_ENTER after reset, original zero arguments
        return false; // no base completion or repeat on missing new host
    }
    const auto country=m_player_country(*next);
    if(slot.country==country.country)return false; // source GetCountry, before host/target/aggro mutation
    if(!old||!SameNativeCellPlayer(*old,*next)) {
        ++slot.host_key;slot.host=args.event_host;
        m_publish(NotifyNativeMonsterHost(map,slot,old));
        if(auto skill=SelectNativeMonsterSkill(m_catalog,map,slot,next,true,m_tick))
            m_publish(NativeCellNotices{std::move(*skill)});
    }
    // SelectSkill above deliberately observes the OLD target, as in source.
    slot.target=args.real_host;slot.target_type=args.real_type;
    AddAggro(slot,args.event_host,args.real_host,args.real_type,country.war_country,1);
    SetEventToFollower(map,id);
    const auto current=map.slots.find(id);
    if(current==map.slots.end()||current->second.generation!=generation)return false;
    return Complete(map,id,binding,args);
}
} // namespace tmapsvr
