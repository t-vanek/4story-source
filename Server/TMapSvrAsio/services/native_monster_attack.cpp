#include "native_monster_ai.h"
#include "native_monster_skills.h"
#include <stdexcept>
#include <utility>

namespace tmapsvr {
bool NativeMonsterAiExecutor::Attack(NativeMonsterMap& map,std::uint32_t id,const NativeAiBinding& binding,Arguments args,bool begin) {
    const auto generation=map.slots.at(id).generation;
    const auto current=[&]() -> NativeMonsterSlot* {
        const auto found=map.slots.find(id);
        return found!=map.slots.end()&&found->second.generation==generation?&found->second:nullptr;
    };
    auto* slot=current();
    // ATTACK filters the event's real-host ID/type; BEGINATK does not.
    if(!begin&&(slot->target!=args.real_host||slot->target_type!=args.real_type))return Complete(map,id,binding,args);
    const auto host=FindNativeMonsterHost(map,slot->x,slot->z,slot->host);
    bool sent=false;
    if(host&&slot->next_skill) {
        const auto skill=*slot->next_skill;
        const auto state=slot->skills.find(skill);
        if(state==slot->skills.end())throw std::runtime_error("Source monster next skill has no owned learned object");
        const auto ready=[&]{return !NativeMonsterSkillRemaining(state->second,m_tick);};
        const auto silence=[&]{return NativeMonsterCanSilenceAttack(m_catalog,*slot,skill);};
        // Short-circuit order is different in the two original commands.
        if(begin?(silence()&&ready()):(ready()&&silence())) {
            const auto& definition=m_catalog.skills.at(skill);
            NativeCellNotice notice{NativeCellNoticeKind::MonsterAttack,*host,{}};
            notice.monster_id=id;notice.monster_generation=generation;
            notice.attack=NativeMonsterAttack{definition.bTargetRange?id:slot->target,
                definition.bTargetRange?std::uint8_t{2}:slot->target_type,skill};
            m_publish(NativeCellNotices{std::move(notice)}); // source host only, before SkillUse/reselection
            slot=current();if(!slot)return false;
            UseNativeMonsterSkill(m_catalog,*slot,skill,m_tick); // no HP/MP debit in source SkillUse
            sent=true;
        }
    }
    // BEGINATK selects only after actually using a skill. ATTACK selects after
    // matching its event even when no next skill is ready (null host is a no-op).
    if(!begin||sent) {
        slot=current();if(!slot)return false;
        if(auto notice=SelectNativeMonsterSkill(m_catalog,map,*slot,host,false,m_tick))
            m_publish(NativeCellNotices{std::move(*notice)});
    }
    slot=current();if(!slot)return false;
    if(begin)++slot->host_key; // unconditional BEGINATK increment, including no send; ATTACK keeps its key
    return Complete(map,id,binding,args);
}
} // namespace tmapsvr
