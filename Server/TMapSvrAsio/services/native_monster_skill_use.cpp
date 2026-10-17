#include "native_monster_skills.h"
#include "native_skill_state.h"
#include "native_skill_value.h"
#include "skill_timing.h"
#include <algorithm>
#include <bit>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace tmapsvr {
namespace {
constexpr std::uint8_t BaseSkillKind=101; // TMapType.h MONSTER_BASE_SKILL_KIND
void Row(std::size_t& visited) {
    if(++visited>1'000'000)throw std::runtime_error("Monster timing/status row work limit");
}
std::int32_t Add(std::int32_t left,std::int32_t right) {
    const auto sum=std::int64_t{left}+right;
    if(sum<std::numeric_limits<std::int32_t>::min()||sum>std::numeric_limits<std::int32_t>::max())
        throw std::runtime_error("Monster timing sum outside source INT domain");
    return static_cast<std::int32_t>(sum);
}
std::int32_t Ability(const NativeMonsterCatalog& c,const NativeMonsterMaintain& skill,
                     std::uint32_t base,std::uint8_t action,std::uint8_t exec,std::size_t& visited,bool all_actions=false) {
    std::int32_t sum=0;
    for(const auto& row:NativeMonsterSkillData(c,skill.skill)) {
        Row(visited);
        if((all_actions||row.bAction==action)&&row.bType==SDT_ABILITY&&row.bExec==exec)
            sum=Add(sum,native_skill_value::Calculate(row,c.skills.at(skill.skill),skill.level,base));
    }
    return sum;
}
void Use(NativeMonsterSkill& state,std::uint32_t tick,std::uint32_t delay) {
    if(NativeMonsterSkillRemaining(state,tick)<delay){state.use_tick=tick;state.delay_tick=delay;}
}
}
NativeMonsterAbility CalculateNativeMonsterAbility(const NativeMonsterCatalog& c,const NativeMonsterSlot& slot,
    std::uint32_t base,std::uint8_t exec,const NativeMonsterMaintain* instance_override) {
    // CalcAbilityValue: real instance, maintained and remain objects
    // are independent. InitMonster does NOT turn its learned skills into passives.
    std::size_t visited=0;
    const auto* selected=instance_override;
    if(!selected&&slot.instance_skill)selected=&*slot.instance_skill;
    std::int32_t instance=selected?Ability(c,*selected,base,0,exec,visited,true):0;
    std::int32_t buffs=0,cure=0;
    for(const auto& effect:slot.maintained) {
        buffs=Add(buffs,Ability(c,effect,base,SA_BUFF,exec,visited));
        std::int32_t effect_cure=0;
        if(slot.instance_skill)for(const auto& row:NativeMonsterSkillData(c,effect.skill)) {
            Row(visited);
            if(row.bType==SDT_CURE&&row.bExec==exec)
                for(const auto& target:NativeMonsterSkillData(c,slot.instance_skill->skill)) {
                    Row(visited);
                    if(row.bAttr==target.bAttr)effect_cure=Add(effect_cure,native_skill_value::Calculate(row,c.skills.at(effect.skill),effect.level,base));
                }
        }
        cure=Add(cure,effect_cure); // source CalcCure returns each object's INT subtotal before the outer sum
    }
    const auto combined=Add(instance,cure);
    instance=(instance>=0&&combined<0)||(instance<=0&&combined>0)?0:combined;
    buffs=Add(buffs,instance);
    auto value=static_cast<std::uint32_t>(std::max(std::int32_t{0},Add(std::bit_cast<std::int32_t>(base),buffs)));
    if(buffs) {
        std::uint32_t magic=0;
        // ApplyEffectionBuff: every STATUS_MAGIC row, regardless of action.
        // Its product, division and mixed signed/unsigned accumulation are
        // DWORD operations; preserve those bits without signed overflow.
        for(const auto& effect:slot.maintained)for(const auto& row:NativeMonsterSkillData(c,effect.skill)) {
            Row(visited);
            if(row.bType==SDT_STATUS&&row.bExec==SDT_STATUS_MAGIC) {
                const auto raw=native_skill_value::SignedValue(row,c.skills.at(effect.skill),effect.level);
                const auto product=static_cast<std::uint32_t>(std::uint64_t{value}*static_cast<std::uint32_t>(raw));
                const auto term=product/100U;
                if(row.bInc==SVI_INCREASE)magic+=term;else magic-=term;
            }
        }
        buffs=Add(buffs,std::bit_cast<std::int32_t>(magic));
        value+=magic;
    }
    std::int32_t remain=0;
    for(const auto& skill:slot.remaining) {
        remain=Add(remain,Ability(c,skill,base,SA_CONTINUE,exec,visited));
        remain=Add(remain,Ability(c,skill,base,SA_PASSIVE,exec,visited));
    }
    // No equipped inventory exists in normal native monster allocation: its
    // actual constructor/default inventory contains no equipment. Equipment
    // producers must supply their own complete graph before extending this path.
    return {static_cast<std::uint32_t>(std::max(std::int32_t{0},Add(std::bit_cast<std::int32_t>(value),remain))),Add(buffs,remain)};
}
std::uint32_t NativeMonsterMaximum(const NativeMonsterCatalog& c,const NativeMonsterSlot& slot,bool hp) {
    if(!slot.attribute)throw NativeMonsterInitializationRequired(slot.map,slot.channel,slot.id,"actual monster attribute object for GetMaxHP/GetMaxMP");
    const auto& attribute=c.attributes.at(*slot.attribute);
    // NetCode: MTYPE_MHP50 / MTYPE_MMP51. These consume raw monster attributes,
    // not the player/formula pure maxima used by percentage resource costs.
    const auto value=CalculateNativeMonsterAbility(c,slot,hp?attribute.hp:attribute.mp,hp?50:51).value;
    return value?value:1;
}
std::uint32_t NativeMonsterSkillRemaining(const NativeMonsterSkill& state,std::uint32_t tick) {
    const std::uint32_t elapsed=tick-state.use_tick;
    return !state.use_tick||elapsed>=state.delay_tick?0:state.delay_tick-elapsed;
}
bool NativeMonsterCanSilenceAttack(const NativeMonsterCatalog& c,const NativeMonsterSlot& slot,std::uint16_t skill) {
    const auto definition=c.skills.find(skill);
    if(definition==c.skills.end())return false; // source global FindTSkill, not template0
    std::size_t visited=0;
    for(const auto& effect:slot.maintained)for(const auto& row:NativeMonsterSkillData(c,effect.skill)) {
        Row(visited);
        if(row.bType==SDT_STATUS&&row.bExec==SDT_STATUS_SILENCE&&definition->second.bKind!=BaseSkillKind)return false;
    }
    return true;
}
SkillAttackTiming NativeMonsterAttackTiming(const NativeMonsterCatalog& c,const NativeMonsterSlot& slot,std::uint8_t speed) {
    if(speed<1||speed>3)return {0,100}; // actual TAD_NONE/unknown switch branch
    const auto rate=CalculateNativeMonsterAbility(c,slot,100,static_cast<std::uint8_t>(53+speed)).value; // MTYPE_PAS54/LAS55/MPAS56
    std::uint32_t delay=0;
    if(slot.class_template&&slot.race_template) {
        const auto& klass=c.classes.at(*slot.class_template);const auto& race=c.races.at(*slot.race_template);
        const auto found=c.formulas.find(speed==3?FTYPE_MNAS:FTYPE_NAS);
        const auto f=found==c.formulas.end()?FormulaRow{}:found->second; // original missing formula getters return0
        const auto raw=skill_timing::BaseAttackDelay(f.dwInit,f.fRateX,f.fRateY,
            speed==3?race.wWIS:race.wDEX,speed==3?klass.wWIS:klass.wDEX);
        delay=static_cast<std::uint32_t>(std::max(std::int32_t{0},std::bit_cast<std::int32_t>(raw)));
    }
    return {delay,rate};
}
void UseNativeMonsterSkill(const NativeMonsterCatalog& c,NativeMonsterSlot& slot,std::uint16_t skill,std::uint32_t tick) {
    if(!slot.skills.contains(skill))throw std::runtime_error("Monster SkillUse has no owned learned object");
    const auto& definition=c.skills.at(skill);
    const auto timing=NativeMonsterAttackTiming(c,slot,definition.bSpeedApply);
    auto states=slot.skills; // validate arithmetic/metadata before mutating any real learned clock
    Use(states.at(skill),tick,skill_timing::ReuseDelay(definition,states.at(skill).level,timing));
    if(definition.dwKindDelay)for(auto& [id,state]:states)
        if(c.skills.at(id).bKind==definition.bKind)Use(state,tick,definition.dwKindDelay);
    slot.skills=std::move(states); // SDELAY_KIND bypasses attack-speed/rate scaling, including the used skill
}
} // namespace tmapsvr
