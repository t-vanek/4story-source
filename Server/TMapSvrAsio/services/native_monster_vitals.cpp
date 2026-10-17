#include "native_monster_ai.h"
#include "native_monster_skills.h"
#include "native_skill_state.h"
#include "wire_codec.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace tmapsvr {
namespace {
void Work(std::size_t& visited) {
    if(++visited>1'000'000)throw std::runtime_error("Source monster transformation work limit");
}
bool Type(const NativeMonsterCatalog& catalog,std::uint16_t skill,std::uint8_t type,std::size_t& visited) {
    for(const auto& row:NativeMonsterSkillData(catalog,skill)) {
        Work(visited);if(row.bType==type)return true;
    }
    return false;
}
bool Posture(const NativeMonsterCatalog& catalog,std::uint16_t skill,std::size_t& visited) {
    for(const auto& row:NativeMonsterSkillData(catalog,skill)) {
        Work(visited);
        if(row.bType==SDT_STATUS&&(row.bExec==7||row.bExec==26||row.bExec==27))return true;
    }
    return false; // source ATKMODE/CRAZEMODE/DEFENDMODE, any action
}
// Source UpdateBuffSkill's branches that do not remove another effect. Refuse
// an actual replacement dependency; never pretend that EraseMaintainSkill ran.
bool AdmitTransformation(const NativeMonsterCatalog& catalog,const NativeMonsterSlot& slot,std::uint16_t skill,
                         std::size_t& visited) {
    bool transformed=false;
    for(const auto& old:slot.maintained) {
        Work(visited);if(Type(catalog,old.skill,SDT_TRANS,visited)){transformed=true;break;}
    }
    if(transformed&&Type(catalog,skill,SDT_TRANS,visited))return false;
    const auto& incoming=catalog.maintain_templates.at(skill);
    if(!(incoming.positive%2))return true; // same ID is already excluded by IsTransformation
    if(slot.maintained.empty())return true;
    const bool posture=Posture(catalog,skill,visited);
    for(const auto& old:slot.maintained) {
        Work(visited);
        if(!(catalog.maintain_templates.at(old.skill).positive%2))continue;
        const bool old_posture=Posture(catalog,old.skill,visited);
        if(posture) {
            const auto required=catalog.maintain_templates.at(old.skill).posture;
            if(old_posture||(required&&required!=skill&&old.attack_type==2&&old.attack_id==slot.id))
                throw NativeMonsterInitializationRequired(slot.map,slot.channel,slot.id,"transformation posture replacement and EraseMaintainSkill side effects");
        } else if(!old_posture)for(const auto& prior:NativeMonsterSkillData(catalog,old.skill)) {
            Work(visited);if(prior.bAction!=SA_BUFF)continue;
            for(const auto& next:NativeMonsterSkillData(catalog,skill)) {
                Work(visited);
                if(prior.bType==next.bType&&prior.bAttr==next.bAttr&&next.bExec&&prior.bExec==next.bExec)
                    throw NativeMonsterInitializationRequired(slot.map,slot.channel,slot.id,"transformation buff collision: source priority/value/self/radius and EraseMaintainSkill");
            }
        }
    }
    return true; // source no collision, including the real empty vector
}
NativeCellNotices VitalNotices(const NativeMonsterMap& map,const NativeMonsterSlot& slot,
                              std::optional<NativeMonsterTransformation> transformed={}) {
    const auto position=NativeCellAt(slot.x,slot.z);
    if(!position||!slot.generation)throw std::runtime_error("Source monster vital output requires defined WORD coordinates and actual lifetime");
    const NativeMonsterVitals vitals{slot.max_hp,slot.hp,slot.max_mp,slot.mp};
    NativeCellNotices out;
    const auto cells=NativeNeighborCells(map.cells,*position);
    for(auto cell=cells.rbegin();cell!=cells.rend();++cell) {
        const auto& players=map.cells.at(*cell).players;
        for(auto player=players.rbegin();player!=players.rend();++player) {
            if(out.size()>NativeCellNoticeLimit-(transformed?2:1))throw std::runtime_error("Source monster vital notice limit");
            NativeCellNotice notice{NativeCellNoticeKind::MonsterVitals,player->second.claim,{}};
            notice.monster_id=slot.id;notice.monster_generation=slot.generation;notice.vitals=vitals;
            out.push_back(notice);
            if(transformed) {
                notice.kind=NativeCellNoticeKind::MonsterTransformation;notice.transformation=transformed;
                out.push_back(std::move(notice)); // HPMP then DEFEND for EACH recipient
            }
        }
    }
    return out;
}
}
std::vector<std::byte> EncodeNativeMonsterVitals(std::uint32_t id,const NativeMonsterVitals& vitals) {
    std::vector<std::byte> out;out.reserve(21);
    wire::WriteLE(out,id);wire::WriteLE<std::uint8_t>(out,2);
    wire::WriteLE(out,vitals.max_hp);wire::WriteLE(out,vitals.hp);
    wire::WriteLE(out,vitals.max_mp);wire::WriteLE(out,vitals.mp);
    return out; // CSSender does not serialize its bLevel parameter
}
std::vector<std::byte> EncodeNativeMonsterTransformation(std::uint32_t id,const NativeMonsterTransformation& source) {
    std::vector<std::byte> out;out.reserve(84);
    wire::WriteLE(out,id);wire::WriteLE(out,id);
    wire::WriteLE<std::uint8_t>(out,2);wire::WriteLE<std::uint8_t>(out,2);
    wire::WriteLE<std::uint32_t>(out,0);wire::WriteLE<std::uint8_t>(out,0); // wire host differs from maintained host
    wire::WriteLE<std::uint32_t>(out,0);wire::WriteLE<std::uint32_t>(out,0); // act/animation
    wire::WriteLE<std::uint8_t>(out,1);wire::WriteLE<std::uint32_t>(out,0); // source sends literal maintain TRUE/tick0
    wire::WriteLE<std::uint8_t>(out,0);wire::WriteLE<std::uint8_t>(out,1); // hit0 / HT_NORMAL
    wire::WriteLE<std::uint16_t>(out,1);wire::WriteLE(out,source.level);
    for(unsigned i=0;i<4;++i)wire::WriteLE<std::uint32_t>(out,0);
    wire::WriteLE<std::uint8_t>(out,1);wire::WriteLE<std::uint8_t>(out,0); // select/cancel
    wire::WriteLE(out,source.country);wire::WriteLE(out,source.aid_country);
    wire::WriteLE(out,source.skill);wire::WriteLE<std::uint8_t>(out,1);
    wire::WriteLE<std::uint16_t>(out,0);wire::WriteLE<std::uint8_t>(out,1); // back skill/perform
    for(unsigned i=0;i<2;++i) {wire::WriteLE(out,source.x);wire::WriteLE(out,source.y);wire::WriteLE(out,source.z);}
    wire::WriteLE<std::uint8_t>(out,0); // actual NULL mapSkillDamage
    return out;
}
bool NativeMonsterAiExecutor::Refill(NativeMonsterMap& map,std::uint32_t id,const NativeAiBinding& binding,Arguments args) {
    auto& slot=map.slots.at(id);
    if(slot.map!=map.map_id||slot.channel!=map.physical_channel)
        throw std::runtime_error("Source REFILL object has mismatched owning map");
    slot.max_hp=slot.hp=NativeMonsterMaximum(m_catalog,slot,true);
    slot.max_mp=slot.mp=NativeMonsterMaximum(m_catalog,slot,false);slot.maxima_ready=true;
    const auto generation=slot.generation;
    m_publish(VitalNotices(map,slot));
    const auto current=map.slots.find(id);
    if(current==map.slots.end()||current->second.generation!=generation)return false;
    return Complete(map,id,binding,args); // no host-key increment in source REFILL
}
bool NativeMonsterAiExecutor::Transformation(NativeMonsterMap& map,std::uint32_t id) {
    auto& slot=map.slots.at(id);
    if(!slot.monster)throw std::runtime_error("Source monster Transformation has no actual template");
    const auto skill=m_catalog.monsters.at(*slot.monster).transform_skill;
    if(!skill||!m_catalog.skills.contains(skill))return false;
    for(const auto& effect:slot.maintained) {
        Step();if(effect.skill==skill)return false; // IsTransformation: no action/expiry/rank filter
    }
    std::size_t visited=0;bool maintain=false;
    for(const auto& row:NativeMonsterSkillData(m_catalog,skill)) {
        Work(visited);if(row.bAction==SA_BUFF){maintain=true;break;}
    }
    // Source returns TRUE even when MaintainSkill returns NULL. Do not
    // transform that observable branch into death or a fabricated effect.
    if(!maintain||!AdmitTransformation(m_catalog,slot,skill,visited))return true;
    const auto& definition=m_catalog.maintain_templates.at(skill);
    NativeMonsterMaintain effect;effect.skill=skill;
    effect.host_id=slot.host;effect.host_type=1;effect.attack_id=id;effect.attack_type=2;
    effect.hit=0;effect.attack_level=0;effect.attacker_level=0;
    effect.attack_country=slot.aid_country!=3?slot.aid_country:slot.country;
    effect.position={slot.x,slot.y,slot.z};
    effect.duration=definition.duration+definition.duration_increment*static_cast<std::uint32_t>(int(effect.level)-1);
    effect.start_tick=effect.duration?std::max(m_tick,std::uint32_t{1}):0;effect.charge_tick=m_tick;
    for(const auto& row:NativeMonsterSkillData(m_catalog,skill)) {
        Work(visited);
        if(row.bType==SDT_STATUS&&row.bExec==SDT_STATUS_SILENCE&&slot.next_skill)effect.silenced_skill=*slot.next_skill;
    }
    if(!definition.static_skill&&slot.status==3)return false; // source PushMaintainSkill guard
    slot.maintained.push_back(effect); // real owned CTSkill producer, not only a wire projection
    for(const auto& row:NativeMonsterSkillData(m_catalog,skill)) {
        Work(visited);if(row.bType!=SDT_TRANS)continue;
        const auto model=m_catalog.monsters.find(row.wValue?row.wValue:*slot.monster);
        if(model!=m_catalog.monsters.end())slot.size=model->second.size/2; // missing nonzero model leaves size unchanged
    }
    slot.max_hp=slot.hp=NativeMonsterMaximum(m_catalog,slot,true);
    slot.max_mp=slot.mp=NativeMonsterMaximum(m_catalog,slot,false);slot.maxima_ready=true;
    const NativeMonsterTransformation reply{skill,slot.level.value_or(0),slot.country,slot.aid_country,slot.x,slot.y,slot.z};
    m_publish(VitalNotices(map,slot,reply));
    return true;
}
void NativeMonsterAiExecutor::DieFromRoam(NativeMonsterMap& map,std::uint32_t id) {
    auto& slot=map.slots.at(id);
    if(slot.status==3||Transformation(map,id))return;
    // Do not expose an invented corpse after only zeroing bars. Source zero-
    // damage deaths can still create money/items and execute dead AI.
    throw NativeMonsterInitializationRequired(slot.map,slot.channel,slot.id,
        "normal monster OnDie base/ReleaseMaintain/DIE, rewards/quest/event drops, AddItem/pets and AT_DEAD");
}
} // namespace tmapsvr
