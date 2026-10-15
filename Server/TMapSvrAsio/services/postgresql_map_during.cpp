#include "postgresql_map_service.h"
#include "domain/main_transfer.h"
#include "posture_effects.h"
#include "secure_code.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <bit>
#include <ctime>

namespace tmapsvr {
namespace {
std::string DuringJson(const std::vector<DuringItem>& items) {
    if(items.size()>1)throw std::runtime_error("Ambiguous source EXP entitlement slot");
    std::string out="[";
    for(const auto& item:items)out+='['+std::to_string(item.item)+','+std::to_string(item.type)+','+
        std::to_string(item.remaining)+','+std::to_string(item.expires)+']';
    return out+']';
}
void RequireExpTemplate(soci::session& sql,std::uint16_t item,int& use) {
    const int id=std::bit_cast<std::int16_t>(item);int kind=0;
    sql << "SELECT \"bKind\",\"bUseType\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",
        soci::use(id),soci::into(kind),soci::into(use);
    if(!sql.got_data()||kind!=56)throw std::runtime_error("During item requires additional native premium/PCBang semantics");
}
}
void PostgreSQLMapService::HydrateDuringItems(soci::session& sql,const MapSessionClaim& c,CharacterPayload& p) {
    // TGetPcBangData is account-owned. Until premium effects, PCBang daily
    // state and their pet branches exist, do not admit them as an ordinary EXP
    // bonus. Their archived/native rows remain untouched by this repository.
    const int user=SqlInt32Identity(c.user_id),world=c.group,character=SqlInt32Identity(c.char_id);int blocked=0;
    sql << "SELECT CASE WHEN EXISTS(SELECT 1 FROM app_global.during_items WHERE user_id=:u) OR "
        "EXISTS(SELECT 1 FROM app_global.\"TCURRENTUSER\" WHERE \"dwUserID\"=:u AND \"dwPcBangID\"<>0) THEN 1 ELSE 0 END",
        soci::use(user,"u"),soci::into(blocked);
    if(blocked)throw std::runtime_error("Account premium/PCBang runtime integration is incomplete");
    DuringItemState state;state.entered_at=std::time(nullptr);
    if(p.transfer_state) {
        if(p.transfer_state->pc_bang)throw std::runtime_error("Transferred PCBang runtime integration is incomplete");
        state.items=p.transfer_state->during_items;
    }else {
        soci::rowset<soci::row> rows=(sql.prepare <<
            "SELECT item_id,item_type,remaining,expires FROM app_world.exp_entitlements WHERE world_id=:w AND char_id=:c",
            soci::use(world,"w"),soci::use(character,"c"));
        for(const auto& row:rows)state.items.push_back({static_cast<std::uint16_t>(row.get<int>(0)),
            static_cast<std::uint8_t>(row.get<int>(1)),static_cast<std::uint32_t>(row.get<long long>(2)),row.get<long long>(3)});
    }
    for(const auto& row:state.items){int use=0;RequireExpTemplate(sql,row.item,use);}
    state.durable_json=DuringJson(state.items);p.during=std::move(state);
}
void PostgreSQLMapService::CheckDuringItems(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) {
    if(!s.payload||!s.payload->during)throw std::runtime_error("During items were not hydrated");
    if(s.payload->transfer_state)return; // exact graph/quest ledger fences are authoritative
    const int world=c.group,character=SqlInt32Identity(c.char_id);int matches=0;
    sql << "SELECT CASE WHEN app_world.map_exp_state(CAST(:w AS smallint),CAST(:c AS integer))=CAST(:state AS jsonb) THEN 1 ELSE 0 END",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(s.payload->during->durable_json,"state"),soci::into(matches);
    if(!matches)throw std::runtime_error("EXP entitlement changed since hydration");
}
void PostgreSQLMapService::WriteDuringItems(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s) {
    CheckDuringItems(sql,c,s);
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    const auto json=DuringJson(p->during->items);
    if(!p->transfer_state&&json!=p->during->durable_json) {
        const int world=c.group,character=SqlInt32Identity(c.char_id);
        sql << "DELETE FROM app_world.exp_entitlements WHERE world_id=:w AND char_id=:c",soci::use(world,"w"),soci::use(character,"c");
        sql << "INSERT INTO app_world.exp_entitlements(world_id,char_id,item_id,item_type,remaining,expires) "
            "SELECT :w,:c,(e->>0)::integer,(e->>1)::smallint,(e->>2)::bigint,(e->>3)::bigint FROM jsonb_array_elements(CAST(:state AS jsonb)) e",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(json,"state");
    }
    p->during->durable_json=json;
}
void PostgreSQLMapService::EraseProgressEffect(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::size_t index,QuestPackets& packets) {
    using tnetlib::protocol::MessageId;
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    const auto id=p->effects->at(index).skill;p->effects->erase(p->effects->begin()+index);
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_SKILLEND_ACK),EncodeSkillEnd(c.char_id,id),QuestAudience::Neighbors);
    RefreshEquipment(sql,s,true);
    if(!s.payload->statistics)throw std::runtime_error("Progression effect removal requires source statistics");
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_CHARSTATINFO_ACK),EncodeCharacterStatistics(s,*s.payload->statistics));
}
void PostgreSQLMapService::ForceProgressEffect(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::uint16_t id,std::uint32_t remaining,QuestPackets& packets) {
    using tnetlib::protocol::MessageId;
    if(!s.payload||!SupportedPostures(*s.payload)||!s.payload->event_ms)
        throw std::runtime_error("Forced status requires supported source effects and event clock");
    const auto def=std::find_if(s.payload->progress_effect_templates.begin(),s.payload->progress_effect_templates.end(),
        [id](const auto& v){return v.skill==id;});
    if(def==s.payload->progress_effect_templates.end())return; // original missing FindTSkill
    // EraseProgressEffect refreshes/replaces payload and its template vector.
    // Copy incoming metadata before any removal, never retain its iterator.
    const auto incoming=*def;const auto duration=remaining?remaining:incoming.duration;
    const bool was_empty=s.payload->effects->empty();bool erased=false,collision=false;
    for(std::size_t i=0;i<s.payload->effects->size();) {
        const auto effect=s.payload->effects->at(i);
        if(IsNativePosture(effect.skill)){++i;continue;}
        const auto old=std::find_if(s.payload->progress_effect_templates.begin(),s.payload->progress_effect_templates.end(),
            [&](const auto& value){return value.skill==effect.skill;});
        if(old==s.payload->progress_effect_templates.end())throw std::runtime_error("Forced status found an unsupported old template");
        // Source HaveSkillData requires equal type/attribute/nonzero exec.
        // Every admitted row is SA_BUFF/SDT_STATUS at rank1; STATUS_NONE800
        // therefore never collides, including with another retained800.
        if(!incoming.status||old->status!=incoming.status||old->attribute!=incoming.attribute){++i;continue;}
        bool remove=false;
        if(old->priority<incoming.priority)remove=true;
        else if(old->priority>incoming.priority)collision=true;
        else if(old->value>incoming.value)collision=true;
        else if(old->value<incoming.value)remove=true;
        else remove=true; // incoming self-PC wins the equal-value case; no admitted aura
        if(remove){EraseProgressEffect(sql,c,s,i,packets);erased=true;}else ++i;
    }
    // UpdateBuffSkill permits a push if ANY old row was removed, even if
    // another stronger matching row survived. Do not replace it with a global
    // strongest-wins test or invent a DEFEND for a fully blocked collision.
    if(!was_empty&&!erased&&collision)return;
    if(s.payload->effects->size()>=255)throw std::runtime_error("Forced status exceeds original effect count");
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    MaintainedEffect e;e.skill=id;e.level=1;e.remaining=duration;
    e.attack_id=e.host_id=c.char_id;e.attack_type=e.host_type=1;e.attack_country=p->aid_country!=3?p->aid_country:s.bCountry;
    e.hit=0;e.attacker_level=0;e.attack_level=0;e.position={s.fPosX,s.fPosY,s.fPosZ};
    e.sampled_ms=e.charge_ms=p->event_ms;p->effects->push_back(e);
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_DEFEND_ACK),EncodeForcedStatusDefend(s,id,duration),QuestAudience::Neighbors);
}
void PostgreSQLMapService::HangExpBenefit(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,QuestPackets& packets) {
    if(!s.payload->during)throw std::runtime_error("EXP benefit has no entitlement state");
    const auto& rows=s.payload->during->items;
    if(!rows.empty()&&rows.front().type)
        ForceProgressEffect(sql,c,s,903,static_cast<std::uint32_t>(rows.front().remaining*1000U),packets);
}
void PostgreSQLMapService::EnterDuringItems(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,QuestPackets& packets) {
    if(!s.payload->during)throw std::runtime_error("During item admission has no state");
    if(!s.payload->during->pending_entry)return;
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    auto& state=*p->during;
    // LOADCHAR's maintained-skill loop discards STATUS_PCBANG before creating
    // the player. Suppress its old visual at serialization and retire that
    // durable projection here without a spurious SKILLEND/CHARSTAT sequence.
    std::erase_if(*p->effects,[](const auto& e){return e.skill==903;});
    // Original LOADCHAR derives the active type from TITEMCHART, not the
    // persisted bType. Tutorial admission deliberately skips these entries.
    if(s.bStartAct==1&&s.wMapID==2010)state.items.clear();
    for(auto& item:state.items) {
        int use=0;RequireExpTemplate(sql,item.item,use);
        if(use&1)item.type=item.remaining?1:0;
        else if(use&2) {
            item.remaining=static_cast<std::uint32_t>(item.expires>p->event_time?item.expires-p->event_time:0);
            item.type=item.remaining?2:0;
        }else {item.type=0;item.remaining=0;}
    }
    std::erase_if(state.items,[](const auto& item){return !item.type;});
    state.pending_entry=false;
    // EnterMAP erases the first stale PCBang visual when no benefit remains,
    // then HangPremiumItem calls HangExpBuff. The ordinary EXP branch has no
    // RESETPCBANG_ACK and never creates a premium pet.
    if(state.items.empty()) {
        const auto i=std::find_if(p->effects->begin(),p->effects->end(),[](const auto& e){return e.skill==903;});
        if(i!=p->effects->end())EraseProgressEffect(sql,c,s,static_cast<std::size_t>(i-p->effects->begin()),packets);
    }
    HangExpBenefit(sql,c,s,packets);
}
void PostgreSQLMapService::AdvanceDuringItems(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,QuestPackets& packets) {
    if(!s.payload->during||s.payload->during->pending_entry)throw std::runtime_error("During timer precedes EnterMAP");
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    auto& state=*p->during;
    // CTPlayer::OnTimer uses wall-clock whole seconds with DWORD conversion
    // and a ten-second cap, after CheckMaintainSkill. No offline catch-up.
    const auto passed=std::min<std::uint32_t>(static_cast<std::uint32_t>(p->event_time-state.entered_at),10);
    state.entered_at=p->event_time;
    if(state.items.empty())return;
    auto& item=state.items.front();
    if((item.type==2&&item.expires<=p->event_time)||(item.type==1&&!item.remaining)) {
        state.items.clear();
        for(std::size_t i=0;i<s.payload->effects->size();)
            if(s.payload->effects->at(i).skill==903)EraseProgressEffect(sql,c,s,i,packets);else ++i;
        // EraseMaintainSkill(903) calls HangExpBuff again; the cleared EXP
        // slot and the admitted non-PCBang profile intentionally produce none.
    }else item.remaining=item.remaining>passed?item.remaining-passed:0;
}
}
