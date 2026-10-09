#include "postgresql_map_service.h"
#include "domain/skill_data.h"
#include "client_senders.h"
#include "main_transfer_codec.h"
#include "main_transfer_runtime.h"
#include "skill_engine.h"
#include "skill_reagent.h"
#include "skill_timing.h"
#include "wire_codec.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace tmapsvr {
namespace {
bool SameTarget(const SkillTarget& a,const SkillTarget& b) {return a.id==b.id&&a.type==b.type;}
// Validate a possible original random expansion without drawing again. There
// are at most 16 output occurrences; duplicate requested IDs remain legal.
bool ValidTargets(const std::vector<SkillTarget>& input,const std::vector<SkillTarget>& output,
    const std::optional<SkillMultiAttack>& multi) {
    if(!multi)return input.size()==output.size()&&std::equal(input.begin(),input.end(),output.begin(),SameTarget);
    if(multi->count>16)return false;
    if(input.empty()||!multi->count)return output.empty();
    if(output.size()!=multi->count)return false;
    bool seen[17][17]{};
    std::function<bool(std::size_t,std::size_t)> match=[&](std::size_t i,std::size_t j) {
        if(j==output.size())return true;
        if(i==input.size())return std::all_of(output.begin()+j,output.end(),[&](const auto& t){return SameTarget(t,input.front());});
        if(seen[i][j])return false;
        seen[i][j]=true;
        for(std::size_t n=1;n<=std::max<unsigned>(1,multi->target_hit)&&j+n<=output.size();++n) {
            if(!SameTarget(input[i],output[j+n-1]))break;
            if(match(i+1,j+n))return true;
        }
        return false;
    };
    return match(0,0);
}
auto SnapshotBytes(const CharSnapshot& s,std::uint32_t key) {
    SkillCooldownTracker timers;timers.Restore(s.dwCharID,s.payload->skills,0);
    return transfer::Encode(transfer::Capture(s,key,timers,0));
}
}
CharSnapshot PostgreSQLMapService::ValidateSkillCast(soci::session& sql,const MapSessionClaim& c,
    const SkillCastRequest& request,const CharSnapshot& before,const CharSnapshot& after) const {
    if(!before.payload||!after.payload||before.persistence_uncertain||after.persistence_uncertain||
       before.dwCharID!=c.char_id||after.dwCharID!=c.char_id||
       bool(before.payload->transfer_state)!=bool(after.payload->transfer_state)||
       before.payload->last_cast_id!=after.payload->last_cast_id)
        throw std::runtime_error("Invalid native cast snapshot");
    auto expected=before;RefreshEquipment(sql,expected,true);
    if(expected.dwHP!=before.dwHP||expected.dwMP!=before.dwMP)
        throw std::runtime_error("Cast resources exceed source maximum");
    const auto& p=*expected.payload;
    const auto learned=std::find_if(p.skills.begin(),p.skills.end(),[&](const auto& s){return s.wSkillID==request.skill;});
    const auto def=std::find_if(p.skill_templates.begin(),p.skill_templates.end(),[&](const auto& t){return t.wID==request.skill;});
    if(learned==p.skills.end()||!learned->bLevel||def==p.skill_templates.end()||!def->attack_profile)
        throw std::runtime_error("Cast has no authoritative learned skill");
    const auto& t=*def;const auto rank=learned->bLevel;
    SkillUseAckFields ack;std::uint8_t channel=0,count=0;std::uint16_t map=0;
    wire::Reader wire(request.request);
    if(!wire.Read(ack.attack_id)||!wire.Read(ack.attack_type)||!wire.Read(channel)||!wire.Read(map)||!wire.Read(ack.skill_id)||
       (!request.loop&&(!wire.Read(ack.action_id)||!wire.Read(ack.act_id)||!wire.Read(ack.ani_id)))||
       !wire.Read(ack.gnd_x)||!wire.Read(ack.gnd_y)||!wire.Read(ack.gnd_z)||!wire.Read(count)||
       ack.attack_id!=c.char_id||ack.attack_type!=1||channel!=c.channel||map!=before.wMapID||ack.skill_id!=request.skill||
       !std::isfinite(ack.gnd_x)||!std::isfinite(ack.gnd_y)||!std::isfinite(ack.gnd_z))
        throw std::runtime_error("Cast wire identity or header is invalid");
    std::vector<SkillTarget> input,targets;
    for(unsigned i=0;i<count;++i) {
        SkillTarget target;std::uint8_t selected=0;
        if(!wire.Read(target.id)||!wire.Read(target.type)||!wire.Read(selected))throw std::runtime_error("Truncated cast targets");
        if(selected&&input.size()<16)input.push_back(target);
    }
    if(!wire.Eof())throw std::runtime_error("Trailing cast bytes");
    const std::size_t fixed=request.loop?45:62;
    if(request.acknowledgement.size()<fixed)throw std::runtime_error("Truncated accepted cast ACK");
    const auto hits=std::to_integer<unsigned>(request.acknowledgement[fixed-1]);
    if(hits>16||request.acknowledgement.size()!=fixed+hits*5)throw std::runtime_error("Invalid cast ACK target count");
    wire::Reader target_wire(request.acknowledgement.data()+fixed,hits*5);
    for(unsigned i=0;i<hits;++i){SkillTarget target;target_wire.Read(target.id);target_wire.Read(target.type);targets.push_back(target);}
    if(!ValidTargets(input,targets,t.multi_attack))throw std::runtime_error("Cast target expansion disagrees with source");
    if((!request.loop&&t.wMapID!=0xffff&&t.wMapID!=before.wMapID)||(request.loop?t.wTargetActiveID:t.wPrevActiveID)||
       learned->dwRemainTick||t.items==SkillItemGate::Unsupported||t.items==SkillItemGate::Unsuitable)
        throw std::runtime_error("Accepted cast failed source eligibility gates");
    const auto hp=skill_engine::RequiredHP(t,expected.dwMaxHP,rank),mp=skill_engine::RequiredMP(t,expected.dwMaxMP,rank);
    if(expected.dwMP<mp||(request.loop?expected.dwHP<hp:expected.dwHP<=hp))throw std::runtime_error("Cast is not affordable");
    std::vector<SkillItemDebit> selected;
    if(t.items==SkillItemGate::Reagent) {
        if(const auto item=FindSkillReagent(expected,t.wUseItem))selected.push_back({*item,1});
    }else if(t.items==SkillItemGate::Ammunition)selected=FindSkillAmmunition(expected,t.bAmmoKind,static_cast<std::uint8_t>(hits));
    if(((t.items==SkillItemGate::Reagent||t.items==SkillItemGate::Ammunition)&&selected.empty())||selected.size()!=request.debits.size())
        throw std::runtime_error("Cast item selection is invalid");
    for(std::size_t i=0;i<selected.size();++i) {
        const auto& a=selected[i];const auto& b=request.debits[i];
        if(a.count!=b.count||a.before.dlID!=b.before.dlID||a.before.durable_hash!=b.before.durable_hash||
           !b.before.source||transfer::EncodeItem(*a.before.source)!=transfer::EncodeItem(*b.before.source))
            throw std::runtime_error("Cast item debit disagrees with source order");
    }
    SkillAttackTiming timing;
    if(t.bSpeedApply) {
        if(t.bSpeedApply>3||!p.skill_attack_timing)throw std::runtime_error("Cast timing projection unavailable");
        timing=(*p.skill_attack_timing)[t.bSpeedApply-1];
    }
    std::vector<std::uint16_t> same_kind;
    if(!request.loop&&t.dwKindDelay)for(const auto& other:p.skill_templates)if(other.bKind==t.bKind)same_kind.push_back(other.wID);
    SkillCooldownTracker timers;timers.Restore(c.char_id,p.skills,0);
    if(!timers.TryUse(c.char_id,request.skill,0,request.loop?skill_timing::LoopDelay(t,timing):skill_timing::ReuseDelay(t,rank,timing),
        same_kind,request.loop?0:t.dwKindDelay))throw std::runtime_error("Cast reuse gate changed");
    // The powers belong to the pre-removal instance projection. Never rebuild
    // this ACK after the zero-MP eternal-buff mutation in the transaction.
    const auto power=*t.attack_profile;
    ack.result=SKILL_SUCCESS;ack.skill_level=rank;ack.attacker_level=expected.bLevel;
    ack.country=expected.bCountry;ack.aid_country=p.aid_country;
    ack.attack_level=request.loop||power.attack_type==1?power.physical_level:power.magic_level;
    ack.pys_min_power=power.physical_min;ack.pys_max_power=power.physical_max;
    ack.mg_min_power=power.magic_min;ack.mg_max_power=power.magic_max;ack.cp=power.critical;
    if((request.loop?EncodeLoopSkillAck(ack,targets):EncodeSkillUseAck(ack,targets))!=request.acknowledgement)
        throw std::runtime_error("Cast ACK disagrees with authoritative projection");
    if(!selected.empty())ConsumeSkillItemProjection(expected,selected);
    expected.dwHP-=hp;expected.dwMP-=mp;
    expected=transfer::PersistenceSnapshot(expected,c.key,timers,request.elapsed_ms);
    if(CoreFingerprint(c,expected)!=CoreFingerprint(c,after)||SnapshotBytes(expected,c.key)!=SnapshotBytes(after,c.key))
        throw std::runtime_error("Cast changes unrelated state or has incorrect costs/timers");
    return expected;
}
}
