#pragma once
#include "domain/character.h"
#include <algorithm>
#include <stdexcept>

namespace tmapsvr {
inline bool SupportedPostures(const CharacterPayload& p) {
    return p.effects&&std::all_of(p.effects->begin(),p.effects->end(),[](const auto& e){
        return (e.skill==131||e.skill==132)&&e.level==1&&!e.remaining;
    });
}
// ForceMaintain -> UpdateBuffSkill -> ChangeEquipItem -> CheckEquipSkill.
// This slice implements the permanent 131/132 templates, not general buff
// collisions, timed expiration or posture-dependent third-party effects.
template<class Refresh>
void ApplyEquipmentPostures(CharSnapshot& s,std::uint16_t automatic,InventoryMoveCommit& result,Refresh refresh) {
    if(!SupportedPostures(*s.payload))throw std::runtime_error("Equipment active effects require supported posture semantics");
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    const auto update=[&]{refresh(s);p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;};
    const auto erase=[&](std::size_t i,auto& events){
        p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
        const auto id=p->effects->at(i).skill;p->effects->erase(p->effects->begin()+i);
        update();events.push_back({false,id,std::make_shared<const CharSnapshot>(s)});
    };
    if(automatic&&std::none_of(p->effects->begin(),p->effects->end(),[&](const auto& e){return e.skill==automatic;})) {
        // Both supported templates are positive postures. Source UpdateBuffSkill
        // erases every old posture in vector order before pushing the new one.
        while(!p->effects->empty())erase(0,result.effects_before);
        p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
        MaintainedEffect e;e.skill=automatic;e.level=1;e.attack_type=e.host_type=1;
        e.attack_id=e.host_id=s.dwCharID;
        e.attack_country=p->aid_country!=3?p->aid_country:s.bCountry; // GetWarCountry
        e.hit=0;e.attack_level=0;e.attacker_level=0;e.position={s.fPosX,s.fPosY,s.fPosZ};
        p->effects->push_back(e);update();
        result.effects_before.push_back({true,automatic,std::make_shared<const CharSnapshot>(s)});
    }else update();
    result.equipment_display=std::make_shared<const CharSnapshot>(s);
    for(std::size_t i=0;i<p->effects->size();) {
        const auto& e=p->effects->at(i);
        const auto& def=p->posture_templates.at(e.skill-131);
        if(def.skill!=e.skill)throw std::runtime_error("Posture has no pinned template");
        bool suitable=!def.weapons;
        for(const auto& bag:p->bags)if(bag.bag.bInvenID==254)for(const auto& item:bag.items)
            if(item.bKind&&item.bKind<=32&&(def.weapons&(std::uint32_t{1}<<(item.bKind-1))))suitable=true;
        // IsEquipSkillItem deliberately includes broken equipment.
        if(e.attack_type==1&&e.attack_id==s.dwCharID&&!suitable)erase(i,result.effects_after);
        else ++i;
    }
}
}
