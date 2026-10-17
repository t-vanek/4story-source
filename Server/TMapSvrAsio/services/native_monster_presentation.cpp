#include "native_monster_presentation.h"
#include "native_monster_ai.h"
#include "wire_codec.h"
#include <stdexcept>

namespace tmapsvr {
std::vector<std::byte> EncodeNativeMonAttackAck(std::uint32_t monster,const NativeMonsterAttack& attack) {
    std::vector<std::byte> out;out.reserve(12);
    wire::WritePOD(out,monster);wire::WritePOD(out,attack.target);
    wire::WritePOD<std::uint8_t>(out,2);wire::WritePOD(out,attack.target_type);wire::WritePOD(out,attack.skill);
    return out;
}
NativeMonsterColorState ProjectNativeMonsterColor(const NativeMonsterSlot& slot,const NativeMonsterCatalog& catalog,
                                                  const NativeMonsterAiCatalog& ai) {
    NativeMonsterColorState out{slot.keeper,slot.mode,slot.country,slot.aid_country,slot.keeper_type,{}};
    if(slot.monster) {
        const auto type=catalog.monsters.at(*slot.monster).ai_type;
        if(ai.types.contains(type))out.own_ai_type=type;
    }
    return out;
}
NativeMonsterAppearance ProjectNativeMonsterAppearance(const NativeMonsterSlot& slot,const NativeMonsterCatalog& catalog,
                                                       const NativeMonsterAiCatalog& ai,std::uint32_t tick) {
    if(!slot.monster||!slot.generation||!slot.attribute||!slot.maxima_ready)
        throw NativeMonsterInitializationRequired(slot.map,slot.channel,slot.id,"real monster template and derived current maxima for ADDMON");
    // The current derived-maxima producer is fresh empty-state Regen. Effects
    // must not be projected with old unmodified maxima just because they exist.
    if(!slot.maintained.empty()||!slot.remaining.empty()||slot.instance_skill)
        throw NativeMonsterInitializationRequired(slot.map,slot.channel,slot.id,"effect-aware monster GetMaxHP/GetMaxMP producer");
    const auto& monster=catalog.monsters.at(*slot.monster);
    NativeMonsterAppearance out;
    out.generation=slot.generation;out.id=slot.id;out.monster=monster.wID;
    out.template_level=monster.bLevel; // CSSender sends m_pMON->m_bLevel, not m_bLevel
    out.max_hp=slot.max_hp;out.hp=slot.hp;out.max_mp=slot.max_mp;out.mp=slot.mp;
    out.x=slot.x;out.y=slot.y;out.z=slot.z;out.pitch=slot.pitch;out.direction=slot.direction;
    out.mouse=slot.mouse;out.key=slot.key;out.action=slot.action;out.mode=slot.mode;
    out.country=slot.country;out.aid_country=slot.aid_country;out.region=slot.region;
    out.keeper=slot.keeper;out.keeper_type=slot.keeper_type;out.tick=tick;
    if(ai.types.contains(monster.ai_type))out.own_ai_type=monster.ai_type;
    // Deliberate source BYTE narrowing: 256 effects serialize zero,257 one.
    const auto count=static_cast<std::uint8_t>(slot.maintained.size());
    out.maintained.assign(slot.maintained.begin(),slot.maintained.begin()+count);
    return out;
}
std::uint8_t NativeMonsterColor(const NativeMonsterColorState& monster,const NativeMonsterObserver& observer) {
    const auto war=monster.aid_country!=3?monster.aid_country:monster.country;
    if(war==observer.war_country)return 0; // ALLI before any ownership/mode checks
    if(war!=3)return 3; // ENEMY
    if(monster.mode==2)return 2; // GOHOME -> PIECE before keeper
    const auto keeper=observer.party?observer.party:observer.character;
    const std::uint8_t keeper_type=observer.party?2:1; // OWNER_PARTY/PRIVATE
    if(monster.keeper==keeper&&monster.keeper_type==keeper_type)return 3;
    if(monster.keeper)return 1; // NO_REWARD
    if(monster.mode==1)return 3; // BATTLE
    // Source GetColor dereferences the OWN template AI pointer here. Unlike
    // OnEvent, it does not fall back to DEFAULT_AI. Refuse the unsafe branch.
    if(!monster.own_ai_type)throw std::runtime_error("Native monster GetColor requires its actual own AI template");
    return *monster.own_ai_type==1?3:2; // FIRSTATK -> ENEMY, otherwise PIECE
}
std::uint8_t NativeMonsterColor(const NativeMonsterAppearance& monster,const NativeMonsterObserver& observer) {
    return NativeMonsterColor(NativeMonsterColorState{monster.keeper,monster.mode,monster.country,monster.aid_country,
        monster.keeper_type,monster.own_ai_type},observer);
}
std::vector<std::byte> EncodeNativeAddMonAck(const NativeMonsterAppearance& m,const NativeMonsterObserver& observer,bool fresh) {
    if(!m.generation||!observer.character)throw std::runtime_error("Native ADDMON requires owned source lifetime and actual PC observer");
    std::vector<std::byte> out;
    const auto count=static_cast<std::uint8_t>(m.maintained.size());
    out.reserve(51+51*std::size_t{count});
    wire::WritePOD(out,m.id);wire::WritePOD(out,m.monster);wire::WritePOD(out,m.template_level);
    wire::WritePOD(out,m.max_hp);wire::WritePOD(out,m.hp);wire::WritePOD(out,m.max_mp);wire::WritePOD(out,m.mp);
    wire::WritePOD(out,m.x);wire::WritePOD(out,m.y);wire::WritePOD(out,m.z);
    wire::WritePOD(out,m.pitch);wire::WritePOD(out,m.direction);
    wire::WritePOD(out,m.mouse);wire::WritePOD(out,m.key);wire::WritePOD(out,m.action);wire::WritePOD(out,m.mode);
    wire::WritePOD<std::uint8_t>(out,fresh?1:0);wire::WritePOD(out,m.country);
    wire::WritePOD(out,NativeMonsterColor(m,observer));wire::WritePOD(out,m.region);wire::WritePOD(out,count);
    for(unsigned i=0;i<count;++i) {
        const auto& e=m.maintained[i];
        const std::uint32_t elapsed=m.tick-e.start_tick; // original DWORD subtraction, including wrap
        const std::uint32_t remaining=!e.start_tick||elapsed>=e.duration?0:e.duration-elapsed;
        wire::WritePOD(out,e.skill);wire::WritePOD(out,e.level);wire::WritePOD(out,remaining);
        wire::WritePOD(out,e.attack_id);wire::WritePOD(out,e.attack_type);wire::WritePOD(out,e.host_id);wire::WritePOD(out,e.host_type);
        wire::WritePOD(out,e.hit);wire::WritePOD(out,e.attack_level);wire::WritePOD(out,e.attacker_level);
        for(auto power:e.powers)wire::WritePOD(out,power);
        wire::WritePOD(out,e.can_select);wire::WritePOD(out,e.attack_country);
        for(auto position:e.position)wire::WritePOD(out,position);
    }
    return out;
}
} // namespace tmapsvr
