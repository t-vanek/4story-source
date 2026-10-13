#pragma once
#include "domain/main_transfer.h"
#include <algorithm>
#include <tuple>
#include <stdexcept>

namespace tmapsvr {
inline const std::vector<MaintainedEffect>& MaintainedEffects(const CharacterPayload& p) {
    if(p.effects)return *p.effects;
    if(p.transfer_state)return p.transfer_state->buffs;
    static const std::vector<MaintainedEffect> empty;return empty;
}
inline auto MaintainedIdentity(const MaintainedEffect& e) {
    return std::tuple{e.skill,e.level,e.attack_type,e.attack_id,e.host_type,e.host_id,e.attack_country};
}
inline bool SameMaintainedRows(const std::vector<MaintainedEffect>& a,const std::vector<MaintainedEffect>& b) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)
        if(MaintainedIdentity(a[i])!=MaintainedIdentity(b[i])||a[i].remaining!=b[i].remaining)return false;
    return true;
}
inline std::vector<MaintainedEffect> SampleMaintainedRows(const std::vector<MaintainedEffect>& before,std::uint64_t now) {
    if(!now)throw std::runtime_error("Maintained timer sample has no clock");
    std::vector<MaintainedEffect> out;
    for(auto e:before) {
        if(e.remaining) {
            if(!e.sampled_ms||now<e.sampled_ms)throw std::runtime_error("Maintained timer observation is invalid");
            const auto elapsed=now-e.sampled_ms;
            if(elapsed>=e.remaining)continue; // finite expiry must never become indefinite zero
            e.remaining-=static_cast<std::uint32_t>(elapsed);
        }
        e.sampled_ms=now;out.push_back(e);
    }
    return out;
}
inline std::vector<std::uint16_t> ExpiredMaintainedSkills(const CharacterPayload& p,std::uint64_t now) {
    std::vector<std::uint16_t> expired;
    for(const auto& e:MaintainedEffects(p))if(e.remaining) {
        if(!e.sampled_ms||now<e.sampled_ms)throw std::runtime_error("Maintained timer observation is invalid");
        if(now-e.sampled_ms>=e.remaining)expired.push_back(e.skill);
    }
    return expired;
}
inline void SampleMaintainedEffects(CharacterPayload& p,std::uint64_t now) {
    if(!p.effects)throw std::runtime_error("Maintained timers were not hydrated");
    p.effect_sample=MaintainedEffectSample{*p.effects,now};
    p.effects=SampleMaintainedRows(p.effect_sample->before,now);
}
inline void CheckMaintainedProjection(const std::vector<MaintainedEffect>& stored,const CharacterPayload& p) {
    const auto& live=MaintainedEffects(p);
    if(!p.effect_sample) {
        if(!SameMaintainedRows(stored,live))throw std::runtime_error("Maintained effects changed outside their mutation");
        return;
    }
    const auto& source=p.effect_sample->before;
    if(stored.size()!=source.size())throw std::runtime_error("Maintained timer membership changed");
    for(std::size_t i=0;i<source.size();++i) {
        if(MaintainedIdentity(stored[i])!=MaintainedIdentity(source[i])||
           (!stored[i].remaining?source[i].remaining!=0:!source[i].remaining||source[i].remaining>stored[i].remaining))
            throw std::runtime_error("Maintained timer identity or duration changed");
    }
    if(!SameMaintainedRows(SampleMaintainedRows(source,p.effect_sample->at_ms),live))
        throw std::runtime_error("Maintained timer projection disagrees with its clock");
}
// Only the original eight durable fields participate. Constructor/default
// presentation values deliberately differ after source-compatible hydration.
inline std::string MaintainJson(const CharSnapshot& s) {
    if(!s.payload)throw std::runtime_error("Maintained effects require native hydration");
    const auto& effects=MaintainedEffects(*s.payload);
    if(effects.size()>255)throw std::runtime_error("Maintained effects exceed client count");
    std::string out="[";
    for(const auto& e:effects) {
        if(!e.skill||!e.level)throw std::runtime_error("Maintained effect identity is invalid");
        if(out.size()>1)out+=',';
        out+='['+std::to_string(e.skill)+','+std::to_string(e.level)+','+std::to_string(e.remaining)+','+
            std::to_string(e.attack_type)+','+std::to_string(e.attack_id)+','+std::to_string(e.host_type)+','+
            std::to_string(e.host_id)+','+std::to_string(e.attack_country)+']';
    }
    return out+']';
}
}
