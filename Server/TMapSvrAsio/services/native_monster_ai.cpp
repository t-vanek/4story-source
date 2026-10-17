#include "native_monster_ai.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>
#include <utility>

namespace tmapsvr {
namespace {
std::uint32_t FreshMaximum(std::uint32_t raw) {
    // Even with no effects, CalcAbilityValue casts DWORD to Win32 int and
    // clamps negatives to0. GetMaxHP/MP then turns0 into1. Preserve the bits
    // explicitly, including raw values with the sign bit set.
    const auto signed_value=std::bit_cast<std::int32_t>(raw);
    return static_cast<std::uint32_t>(std::max(signed_value,std::int32_t{1}));
}
}
NativeMonsterAiExecutor::NativeMonsterAiExecutor(const NativeMonsterCatalog& catalog,const NativeMonsterAiCatalog& ai,
    const BattleRegions& regions,EnterMap enter,LeaveMap leave,Schedule schedule,Cancel cancel,Draw draw,std::uint32_t tick)
    :m_catalog(catalog),m_ai(ai),m_regions(regions),m_enter(std::move(enter)),
     m_leave(std::move(leave)),m_schedule(std::move(schedule)),m_cancel(std::move(cancel)),m_draw(std::move(draw)),m_tick(tick) {
    if(!m_enter||!m_leave||!m_schedule||!m_cancel||!m_draw)throw std::invalid_argument("Actual source AI side-effect consumers required");
}
void NativeMonsterAiExecutor::Step() {
    // Security limit, not a source gameplay rule. Fail before serving a graph
    // that recurses indefinitely or exhausts one boot/batch turn's resources.
    if(++m_steps>1'000'000)throw std::runtime_error("Source AI executor work limit");
}
std::uint32_t NativeMonsterAiExecutor::Random() {
    const auto value=m_draw();
    if(value>32767)throw std::runtime_error("Source AI requires Windows15-bit random draws");
    return value;
}
const std::vector<NativeAiBinding>* NativeMonsterAiExecutor::Bindings(const NativeMonsterSlot& slot,
    std::uint8_t trigger,std::uint32_t id) const {
    std::uint8_t type=1; // DEFAULT_AI, only when own template AI pointer absent
    if(slot.monster) {
        const auto requested=m_catalog.monsters.at(*slot.monster).ai_type;
        if(m_ai.types.contains(requested))type=requested;
    }
    if(!m_ai.types.contains(type))throw std::runtime_error("Source AI fallback template absent");
    const auto found=m_ai.bindings.find({type,trigger,id});
    return found==m_ai.bindings.end()?nullptr:&found->second;
}
bool NativeMonsterAiExecutor::Conditions(const NativeAiCommand& command,const NativeMonsterSlot& slot) {
    for(const auto& condition:command.conditions) {
        if(condition.kind==0&&Random()%100>=condition.id)return false;
        if(condition.kind==1&&slot.mode!=condition.id)return false;
        // AN_CHGHOST2 actually has no return in original CheckCondition.
        // It and unknown kinds therefore pass. Do not repair its intent.
    }
    return true;
}
bool NativeMonsterAiExecutor::CanRun(const NativeAiCommand& command,const NativeMonsterSlot& slot) {
    switch(command.kind) {
    case 0:if(slot.status!=0||slot.remove)return false;break; // REGEN
    case 2:if(slot.mode==1||slot.status==3)return false;break; // SETHOST
    case 12:return true; // REMOVE bypasses even base conditions
    case 1:case 14: // LEAVE/LOTTERY: first source predicate is DEAD
        if(slot.status!=3)return false;
        throw NativeMonsterInitializationRequired(slot.map,slot.channel,slot.id,"dead-object AI/loot lifecycle");
    case 3:case 4:case 5:case 6:case 7:case 8:case 9:case 10:case 11:case 13:
        // The source first tests WAKEUP in every one of these commands. No
        // actor is admitted in this bounded executor; no-host SetHost never wakes.
        if(slot.status!=1)return false;
        throw NativeMonsterInitializationRequired(slot.map,slot.channel,slot.id,"awake-object AI/client host");
    default:break; // CreateCMD's actual base-command fallback
    }
    return Conditions(command,slot);
}
std::uint32_t NativeMonsterAiExecutor::Delay(const NativeAiBinding& binding,const NativeMonsterSlot& slot) {
    switch(m_ai.commands.at(binding.command).kind) {
    case 0:return m_catalog.spawns.at(slot.spawn).dwDelay;
    case 2:case 3:case 4:case 5:case 6:case 8:case 10:case 12:case 13:return 0;
    case 11: {
        if(!binding.delay)return 0;
        const std::uint32_t bound=std::uint32_t{4}*binding.delay; // source DWORD wrap
        if(!bound)throw std::runtime_error("Source ROAM delay modulo-zero branch");
        return binding.delay+Random()%bound;
    }
    default:return binding.delay;
    }
}
void NativeMonsterAiExecutor::Initialize(NativeMonsterMap& map,std::uint32_t id) {
    if(!map.players.empty())throw std::runtime_error("Native Initialize requires full hosted actor AI before player coexistence");
    // Capture this vector once, as source Initialize does. Regen can replace
    // the monster/AI type but does not change this iteration's command list.
    const auto* bindings=Bindings(map.slots.at(id),6,0); // AT_DELETE
    if(!bindings)return;
    for(const auto& binding:*bindings) {
        Step();
        const auto found=map.slots.find(id);
        if(found==map.slots.end())return; // do not reproduce use-after-Remove
        if(CanRun(m_ai.commands.at(binding.command),found->second))
            (void)Execute(map,id,binding,{}); // direct, delay/repeat ignored
    }
}
void NativeMonsterAiExecutor::Event(NativeMonsterMap& map,std::uint32_t id,std::uint8_t trigger,
    std::uint32_t trigger_id,Arguments arguments) {
    if(m_depth>=256)throw std::runtime_error("Source AI event recursion limit");
    ++m_depth;
    struct Depth {std::size_t& value;~Depth(){--value;}} depth{m_depth};
    const auto found=map.slots.find(id);
    if(found==map.slots.end())return;
    const auto* bindings=Bindings(found->second,trigger,trigger_id);
    if(!bindings)return;
    for(const auto& binding:*bindings) {
        Step();
        const auto current=map.slots.find(id);
        if(current==map.slots.end())return;
        DoCommand(map,id,binding,arguments);
    }
}
void NativeMonsterAiExecutor::DoCommand(NativeMonsterMap& map,std::uint32_t id,const NativeAiBinding& binding,Arguments args) {
    const auto& slot=map.slots.at(id);
    const auto& command=m_ai.commands.at(binding.command);
    auto delay=Delay(binding,slot); // DoAICMD: BEFORE CanRun, even if false
    if(slot.monster&&m_catalog.monsters.at(*slot.monster).remove&&command.kind==1)delay=1;
    if(delay) {
        m_schedule({binding.source_row,slot.generation,id,slot.host_key,args.event_host,args.real_host,
            delay,map.map_id,map.party,map.physical_channel,args.real_type});
    }else if(CanRun(command,slot)) {
        (void)Execute(map,id,binding,args); // source synchronous repeat ignored
    }
}
void NativeMonsterAiExecutor::Acknowledge(NativeMonsterMap& map,const NativeAiInvocation& call) {
    const auto found=map.slots.find(call.monster);
    if(found==map.slots.end()||found->second.generation!=call.generation||found->second.host_key!=call.host_key)return;
    if(!map.players.empty())throw std::runtime_error("Native AI ACK requires full hosted actor execution before player coexistence");
    const auto binding=m_ai.by_source_row.find(call.binding);
    if(binding==m_ai.by_source_row.end())throw std::runtime_error("Native AI ACK command-instance identity absent");
    const Arguments args{call.event_host,call.real_host,call.real_type};
    if(CanRun(m_ai.commands.at(binding->second.command),found->second)&&Execute(map,call.monster,binding->second,args)) {
        const auto current=map.slots.find(call.monster);
        if(current!=map.slots.end()&&current->second.generation==call.generation)
            DoCommand(map,call.monster,binding->second,args);
        // Repeat intentionally captures the CURRENT host key, not the ACK key.
        // AICOMPLETE may have changed it. Never reschedule a replaced object.
    }
}
bool NativeMonsterAiExecutor::Complete(NativeMonsterMap& map,std::uint32_t id,const NativeAiBinding& binding,Arguments args) {
    Event(map,id,0,binding.command,args); // AT_AICOMPLETE before GetDelay
    const auto found=map.slots.find(id);
    if(found==map.slots.end())return false; // source's later dereference is unsafe
    return binding.loop&&Delay(binding,found->second)!=0;
}
void NativeMonsterAiExecutor::ResolveTemplate(NativeMonsterSlot& slot,const NativeMonsterAllocationTemplate& monster) {
    slot.maxima_ready=false;
    slot.monster=monster.wID;
    slot.level_template=m_catalog.levels.contains(monster.bLevel)?std::optional{monster.bLevel}:std::nullopt;
    slot.class_template=m_catalog.classes.contains(monster.bClass)?std::optional{monster.bClass}:std::nullopt;
    slot.race_template=m_catalog.races.contains(monster.bRace)?std::optional{monster.bRace}:std::nullopt;
    const auto key=std::uint32_t{monster.wMonAttr}|(std::uint32_t{monster.bLevel}<<16);
    slot.attribute=m_catalog.attributes.contains(key)?std::optional{key}:std::nullopt;
    if(slot.attribute) {
        slot.level=monster.bLevel;
        for(const auto skill:monster.skills)if(m_catalog.skills.contains(skill))slot.skills.insert(skill);
    }
}
void NativeMonsterAiExecutor::ResetHost(NativeMonsterSlot& slot) {
    if(slot.host||slot.target||slot.helper||slot.help_count)
        throw NativeMonsterInitializationRequired(slot.map,slot.channel,slot.id,"populated host/aggro state");
    slot.action=0;slot.mode=0;slot.last_attack_tick=m_tick; // LoadData0 or actual current AI tick
    slot.host=slot.target=slot.helper=0;slot.target_type=0;slot.help_count=0;
}
bool NativeMonsterAiExecutor::Execute(NativeMonsterMap& map,std::uint32_t id,const NativeAiBinding& binding,Arguments args) {
    Step();
    auto& slot=map.slots.at(id);
    const auto kind=m_ai.commands.at(binding.command).kind;
    if(kind==0)return Regen(map,id,binding,args);
    if(kind==2) { // actual SetHost with a proven empty actor population
        ++slot.host_key;ResetHost(slot);return false; // no completion event
    }
    if(kind==12) {
        auto& spawn=map.spawns.at(slot.spawn);
        if(m_catalog.spawns.at(slot.spawn).bEvent!=0)
            throw NativeMonsterInitializationRequired(slot.map,slot.channel,id,"dynamic spawn deletion");
        m_leave(map,id,true);
        NativeAiInvocation identity;identity.monster=id;identity.generation=slot.generation;
        identity.map=map.map_id;identity.channel=map.physical_channel;identity.party=map.party;
        m_cancel(identity); // eager retirement of every stage for this exact lifetime
        std::erase(spawn.slots,id);map.slots.erase(id);return false;
    }
    if(kind<=14)throw NativeMonsterInitializationRequired(slot.map,slot.channel,id,"concrete awake/dead AI executor");
    return Complete(map,id,binding,args); // actual source default/base behavior
}
bool NativeMonsterAiExecutor::Regen(NativeMonsterMap& map,std::uint32_t id,const NativeAiBinding& binding,Arguments args) {
    auto* slot=&map.slots.at(id);
    const auto& initial=m_catalog.spawns.at(slot->spawn);
    if(!initial.bEvent&&initial.wGroup) {
        const auto& group=m_catalog.groups.at(initial.wGroup);
        if(!group.empty()) {
            const auto index=static_cast<std::uint8_t>(Random()%group.size());
            const auto selected=map.spawns.find(group.at(index));
            if(selected!=map.spawns.end()&&!selected->second.slots.empty())
                slot=&map.slots.at(selected->second.slots.back());
        }
    }
    // Source does not run CanRun/conditions/host-key checks after redirection.
    const auto& source=m_catalog.spawns.at(slot->spawn);
    const bool regen=static_cast<std::uint8_t>(Random()%100)<source.bProb;
    if(!regen)return Complete(map,slot->id,binding,args);
    if(!slot->monster) {
        auto random=Random(); // drawn even when total weight will be zero
        std::uint32_t total=0;
        const auto& candidates=m_catalog.candidates.at(source.wID);
        for(const auto& candidate:candidates)if(!candidate.bEssential)total+=candidate.bProb;
        random=total?random%total:0;total=0;
        for(const auto& candidate:candidates)if(!candidate.bEssential) {
            total+=candidate.bProb;
            if(random<total) {
                const auto monster=m_catalog.monsters.find(candidate.wMonID);
                if(monster==m_catalog.monsters.end())throw std::runtime_error("Selected source Regen monster template absent");
                ResolveTemplate(*slot,monster->second);
                if(!slot->attribute)return false; // template assignment survives
                break;
            }
        }
    }
    if(!slot->monster)return false;
    if(!slot->attribute)throw std::runtime_error("Source Regen would dereference absent attribute after prior selection");
    if(slot->entered_map||slot->regen_delete_spawn||slot->host||slot->target||slot->helper||!slot->maintained.empty())
        throw NativeMonsterInitializationRequired(slot->map,slot->channel,slot->id,"non-fresh Regen lifecycle");
    slot->path_id=0;slot->path_direction=1;
    const auto leader_id=map.spawns.at(slot->spawn).leader;
    const auto leader=leader_id?map.slots.find(leader_id):map.slots.end();
    const bool follow=leader!=map.slots.end()&&leader_id!=slot->id&&leader->second.status==1;
    float dx=0,dz=0;
    if(source.bRange) {
        const auto length=static_cast<float>(Random()%source.bRange);
        const float angle=static_cast<float>(Random()%360)*std::numbers::pi_v<float>/180.0f;
        dx=length*std::cos(angle);dz=length*std::sin(angle);
    }
    if(follow||source.bRange) {
        slot->x=slot->start_x=slot->next_x=(follow?leader->second.next_x:source.fPosX)+dx;
        slot->z=slot->start_z=slot->next_z=(follow?leader->second.next_z:source.fPosZ)+dz;
    }else {
        slot->x=slot->start_x=slot->next_x=source.fPosX;
        slot->z=slot->start_z=slot->next_z=source.fPosZ;
    }
    if(follow){slot->path_id=leader->second.path_id;slot->path_direction=leader->second.path_direction;}
    slot->keeper_type=0;slot->keeper=0;
    slot->y=slot->start_y=slot->next_y=source.fPosY;slot->direction=source.wDir;
    const auto& monster=m_catalog.monsters.at(*slot->monster);
    slot->size=monster.size/2;slot->attack_wait_tick=0;slot->mode=0;
    slot->can_attack=monster.can_attack;slot->special=monster.special;
    slot->leave_count=0;slot->money=0;slot->remove=false;
    if(source.wLocalID) {
        const auto local=m_regions.locals.find(source.wLocalID);
        if(local!=m_regions.locals.end()&&local->second.type==LocalType::Occupation&&local->second.valid)
            slot->country=local->second.country;
    }
    slot->next_skill.reset();ResetHost(*slot);
    const auto& attributes=m_catalog.attributes.at(*slot->attribute);
    slot->max_hp=slot->hp=FreshMaximum(attributes.hp);
    slot->max_mp=slot->mp=FreshMaximum(attributes.mp);
    slot->maxima_ready=true;
    // No source maintained effects/items/host exist in this fresh scope.
    const auto target=slot->id;
    m_enter(map,target); // must actually register cells/visibility or throw
    if(map.slots.contains(target))Event(map,target,7,0,{}); // EnterMAP -> AT_ENTER
    return false; // successful Regen does not call base completion
}
} // namespace tmapsvr
