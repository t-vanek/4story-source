#include "native_monster_skills.h"
#include "skill_engine.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace tmapsvr {
namespace {
FormulaRow Formula(const NativeMonsterCatalog& c,FormulaType type) {
    const auto found=c.formulas.find(type);
    // GetFormulaInit/RateX/RateY explicitly return0 for an absent formula.
    return found==c.formulas.end()?FormulaRow{}:found->second;
}
std::uint32_t Truncate(float value) {
    if(!std::isfinite(value)||value<0||static_cast<double>(value)>std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("Monster pure statistic outside source DWORD conversion range");
    return static_cast<std::uint32_t>(value);
}
NativeCellNotice Notice(const NativeMonsterSlot& slot,const MapSessionClaim& recipient,NativeCellNoticeKind kind) {
    NativeCellNotice out;out.kind=kind;out.recipient=recipient;
    out.monster_id=slot.id;out.monster_generation=slot.generation;
    if(!slot.generation)throw std::runtime_error("Native monster host output requires actual object generation");
    return out;
}
}
bool NativeMonsterCheckPrevAct(const NativeMonsterSlot& slot,std::uint16_t active) {
    return !active||std::any_of(slot.maintained.begin(),slot.maintained.end(),
        [active](const auto& effect){return effect.skill==active;}); // source does not filter expired records here
}
std::uint32_t NativeMonsterPureMaximum(const NativeMonsterCatalog& c,const NativeMonsterSlot& slot,FormulaType type) {
    if(type!=FTYPE_HP&&type!=FTYPE_MP)throw std::invalid_argument("Monster pure maximum requires HP or MP formula");
    const auto f=Formula(c,type);
    float primary=0;
    // GetBaseStatValue returns0 if either source object pointer is absent.
    // This is not treating a missing class as an existing all-zero class.
    if(slot.class_template&&slot.race_template) {
        const auto& klass=c.classes.at(*slot.class_template);const auto& race=c.races.at(*slot.race_template);
        const auto base=static_cast<std::uint16_t>(kBaseStat+(type==FTYPE_HP?klass.wCON+race.wCON:klass.wMEN+race.wMEN));
        // m_bLevel is really0 in CTObjBase until the actual attribute producer
        // assigns it; level0 uses exponent -1, never a fabricated level1 clamp.
        primary=static_cast<float>(base*std::pow(static_cast<double>(Formula(c,FTYPE_1ST).fRateX),int(slot.level.value_or(0))-1));
    }
    return f.dwInit+Truncate(primary*f.fRateX); // FLOAT product then DWORD cast, unsigned init addition wraps
}
bool NativeMonsterCanUseSkillNow(const NativeMonsterCatalog& c,const NativeMonsterSlot& slot,
                                std::uint16_t id,std::uint32_t tick) {
    const auto found=slot.skills.find(id);
    if(found==slot.skills.end())return false; // source null CTSkill
    const auto& state=found->second;const auto& t=c.skills.at(id);
    if(!slot.attack_wait_tick)throw NativeMonsterInitializationRequired(slot.map,slot.channel,slot.id,"observed attack-wait tick for skill selection");
    const std::uint32_t at=tick+*slot.attack_wait_tick;
    const std::uint32_t elapsed=at-state.use_tick;
    if(state.use_tick&&elapsed<state.delay_tick)return false;
    // The original switch's unknown resource type returns0, just like type0.
    const auto cost=[&](std::uint8_t type,std::uint32_t use,std::uint32_t maximum) {
        return type>2?0U:skill_engine::RequiredResource(t,type,use,maximum,state.level);
    };
    if(cost(t.bUseHPType,t.dwUseHP,NativeMonsterPureMaximum(c,slot,FTYPE_HP))>slot.hp)return false;
    if(cost(t.bUseMPType,t.dwUseMP,NativeMonsterPureMaximum(c,slot,FTYPE_MP))>slot.mp)return false;
    return NativeMonsterCheckPrevAct(slot,t.wPrevActiveID);
}
float NativeMonsterTargetDistance(const NativeMonsterMap& map,const NativeMonsterSlot& slot) {
    const auto host=FindNativeMonsterHost(map,slot.x,slot.z,slot.host);
    if(!host)return 65535.0f; // actual source absent-host sentinel
    if(slot.target_type==7||slot.target_type==11||slot.target_type==18)
        throw NativeMonsterInitializationRequired(slot.map,slot.channel,slot.id,"actual recall/self/companion target lifetime and size");
    if(slot.target_type!=1)return 65535.0f;
    // OT_PC uses the host itself, not a guessed lookup of m_dwTargetID.
    const auto& target=map.players.at(host->char_id);
    const float dx=target.x-slot.x,dz=target.z-slot.z,size=slot.size+target.size;
    if(!std::isfinite(dx)||!std::isfinite(dz)||!std::isfinite(size))throw std::runtime_error("Nonfinite source monster target geometry");
    const float distance=std::sqrt(dx*dx+dz*dz);
    if(!std::isfinite(distance))throw std::runtime_error("Source monster target distance overflow");
    return distance>size?distance-size:0.0f;
}
std::optional<NativeCellNotice> SelectNativeMonsterSkill(const NativeMonsterCatalog& c,const NativeMonsterMap& map,
    NativeMonsterSlot& slot,const std::optional<MapSessionClaim>& host,bool new_host,std::uint32_t tick) {
    if(!host)return {}; // no state change when pHost is null
    const auto actor=map.players.find(host->char_id);
    if(actor==map.players.end()||!SameNativeCellPlayer(actor->second.claim,*host))throw std::runtime_error("Monster skill host lifetime is stale");
    const std::uint16_t previous=slot.next_skill?c.skills.at(*slot.next_skill).wID:0;
    const float distance=NativeMonsterTargetDistance(map,slot);
    std::optional<std::uint16_t> second;
    slot.next_skill.reset();
    const auto can=[&](auto id){return NativeMonsterCanUseSkillNow(c,slot,id,tick);};
    for(const auto& learned:slot.skills) {
        const auto id=learned.first;
        const auto& t=c.skills.at(id);
        if(!NativeMonsterCheckPrevAct(slot,t.wPrevActiveID))continue;
        if(distance>=t.wMinRange&&distance<=t.wMaxRange) {
            if(!slot.next_skill)slot.next_skill=id;
            else {
                const auto& old=c.skills.at(*slot.next_skill);
                if(can(*slot.next_skill)) {if(can(id)&&old.dwReuseDelay<t.dwReuseDelay)slot.next_skill=id;}
                else if(can(id))slot.next_skill=id;
                else if(old.wMinRange<t.wMinRange)slot.next_skill=id;
                else if(t.wMinRange==old.wMinRange&&t.dwReuseDelay<old.dwReuseDelay)slot.next_skill=id;
            }
        } else if(t.wMinRange<distance) {
            if(!second)second=id;
            else {
                const auto& old=c.skills.at(*second);
                if(can(*second)) {
                    if(can(id)&&(old.dwReuseDelay<t.dwReuseDelay||
                        (old.dwReuseDelay==t.dwReuseDelay&&old.wMaxRange<t.wMaxRange)))second=id;
                } else if(can(id))second=id;
                else if(old.wMinRange<t.wMinRange)second=id;
                else if(t.wMinRange==old.wMinRange&&t.dwReuseDelay<old.dwReuseDelay)second=id;
            }
        }
    }
    if(slot.next_skill&&second&&!can(*slot.next_skill)&&can(*second)&&
       c.skills.at(*slot.next_skill).wMinRange<=c.skills.at(*second).wMinRange)slot.next_skill=second;
    if(!slot.next_skill)slot.next_skill=second;
    if(!slot.next_skill||(!new_host&&*slot.next_skill==previous))return {};
    auto out=Notice(slot,*host,NativeCellNoticeKind::MonsterSkill);
    out.skill_id=*slot.next_skill;out.attack_range=c.skills.at(*slot.next_skill).wMaxRange;return out;
}
NativeCellNotices NotifyNativeMonsterHost(const NativeMonsterMap& map,const NativeMonsterSlot& slot,
                                        const std::optional<MapSessionClaim>& previous,bool previous_accepted) {
    NativeCellNotices out;
    if(previous) {
        auto notice=Notice(slot,*previous,NativeCellNoticeKind::MonsterHost); // FALSE first
        notice.accepted_recipient=previous_accepted;out.push_back(std::move(notice));
    }
    const auto position=NativeCellAt(slot.x,slot.z);
    if(!position)throw std::runtime_error("Native host notification requires source WORD position");
    const auto cells=NativeNeighborCells(map.cells,*position);
    for(auto cell=cells.rbegin();cell!=cells.rend();++cell) {
        const auto& players=map.cells.at(*cell).players;
        for(auto player=players.rbegin();player!=players.rend();++player) {
            if(previous&&previous->char_id==player->first)continue;
            const auto actor=map.players.find(player->first);
            if(actor==map.players.end()||!SameNativeCellPlayer(actor->second.claim,player->second.claim))
                throw std::runtime_error("Native host notification has stale neighbor index");
            auto notice=Notice(slot,actor->second.claim,NativeCellNoticeKind::MonsterHost);
            notice.flag=slot.host==player->first;out.push_back(std::move(notice));
        }
    }
    return out;
}
} // namespace tmapsvr
