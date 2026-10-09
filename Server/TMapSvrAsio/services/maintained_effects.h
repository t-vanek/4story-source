#pragma once
#include "domain/main_transfer.h"
#include <stdexcept>

namespace tmapsvr {
inline const std::vector<MaintainedEffect>& MaintainedEffects(const CharacterPayload& p) {
    if(p.effects)return *p.effects;
    if(p.transfer_state)return p.transfer_state->buffs;
    static const std::vector<MaintainedEffect> empty;return empty;
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
