#include "postgresql_map_service.h"
#include "native_skill_state.h"
#include "services/skill_reagent.h"
#include "services/posture_effects.h"
#include "services/maintained_effects.h"
#include "services/skill_cooldown.h"
#include "services/native_cure.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <bit>
#include <cmath>
#include <limits>

namespace tmapsvr {
void PostgreSQLMapService::RequireSkillContext(soci::session& sql,std::uint16_t skill) const {
    int verified=0,guild=0;const int id=std::bit_cast<std::int16_t>(skill);
    sql<<"SELECT count(*),CASE WHEN EXISTS(SELECT 1 FROM skill_context_compat.\"TGUILDSKILLCHART\" WHERE \"wSkillID\"=:id) THEN 1 ELSE 0 END "
        "FROM skill_context_compat.catalog_release WHERE character_manifest=:c AND routing_manifest=:r AND status='verified'",
        soci::use(id,"id"),soci::use(m_config.character_manifest,"c"),soci::use(m_config.routing_manifest,"r"),soci::into(verified),soci::into(guild);
    if(verified!=1)throw std::runtime_error("Skill requires verified guild-skill registry publication");
    if(guild)throw std::runtime_error("Guild skill requires native guild-role authority");
}
std::optional<SkillTemplate> PostgreSQLMapService::ReadActItemSkill(soci::session& sql,const CharSnapshot& s,std::uint16_t skill) const {
    if(!s.payload)throw std::runtime_error("Action item requires native source skill authority");
    if(!NativePlayerSkillAvailable(*s.payload,skill))return {};
    const int id=std::bit_cast<std::int16_t>(skill);soci::row row;
    sql<<"SELECT * FROM character_compat.\"TSKILLCHART\" WHERE \"wID\"=:id",soci::use(id),soci::into(row);
    if(!sql.got_data())throw std::runtime_error("Available action-item template missing from pinned catalog");
    RequireSkillContext(sql,skill);
    const auto u8=[&](const char* column){return static_cast<std::uint8_t>(row.get<int>(column));};
    const auto u16=[&](const char* column){return static_cast<std::uint16_t>(row.get<int>(column));};
    // Kind21's source finish uses its trigger's current learned rank (possibly
    // zero), unlike the fixed rank1 ACT acknowledgement. Only the outgoing
    // positive-template CURE is applied; the trigger is not an ordinary cast.
    // This definition is also read by a pending FINISH after ordinary death.
    // Its pure HP/MP targets are then skipped before Defend. Actual ITEMUSE
    // and ACT initiation retain their separate living-actor gates.
    if(!s.payload||!s.payload->statistics||!s.recovery||!s.payload->item_cooldowns||!SupportedPostures(*s.payload)||
       !s.cluster.hydrated||s.cluster.guild||s.cluster.tactics||s.cluster.riding||s.payload->arena_id||s.payload->tournament_player||
       !s.payload->recalls.empty()||(s.wMapID>=500&&s.wMapID<533)||
       (s.payload->transfer_state&&(!s.payload->transfer_state->companions.empty()||!s.payload->transfer_state->companion_items.empty()||
        !s.payload->transfer_state->recalls.empty()||!s.payload->transfer_state->recall_buffs.empty()||s.payload->transfer_state->local_id))||
       u8("bKind")!=21||u8("bPositive")%2!=1||u8("bRunFromServer")||u8("bCheckAttacker")==3||
       (u8("bTarget")!=1&&u8("bTarget")!=3)||u8("bHitTest")!=1||u8("bHitInit")!=100||u8("bHitInc")||
       row.get<int>("dwDuration")||row.get<int>("dwActionTime")||u16("wTargetActiveID")||u16("wPosture")||
       u8("bTargetRange")||u8("bIsDismount")||row.get_indicator("bRepeatCount")==soci::i_null||u8("bRepeatCount"))
        throw std::runtime_error("Action item requires unsupported source skill/actor semantics");
    SkillTemplate out;out.wID=skill;out.wUseItem=u16("wItemID");out.wMapID=u16("wMapID");
    out.wPrevActiveID=u16("wPrevActiveID");out.bKind=21;out.bTargetRange=u8("bTargetRange");
    SkillAttackProfile power;const auto& stats=*s.payload->statistics;
    power.level=s.bLevel;power.aftermath=s.bAftermath;power.rank=0;power.critical=0;
    const auto trigger=u16("wTriggerID");
    const auto learned=std::find_if(s.payload->skills.begin(),s.payload->skills.end(),[&](const auto& v){return v.wSkillID==trigger;});
    if(learned!=s.payload->skills.end()) {
        const auto metadata=std::find_if(s.payload->skill_templates.begin(),s.payload->skill_templates.end(),[&](const auto& v){return v.wID==trigger;});
        if(metadata==s.payload->skill_templates.end()||!metadata->attack_profile||
           metadata->attack_profile->rank!=learned->bLevel||metadata->attack_profile->level!=s.bLevel||
           metadata->attack_profile->aftermath!=s.bAftermath)
            throw std::runtime_error("Learned action-item trigger lacks current source attack metadata");
        power.rank=learned->bLevel;
        // GetCritical runs before kind21 installs any instance skill. Select
        // baseline stats by the TRIGGER's type; its instance-adjusted critical
        // and abilities must not modify this outgoing cure's attack profile.
        power.critical=metadata->attack_profile->attack_type==3?stats.magic_critical:stats.physical_critical;
    }
    power.physical_level=stats.attack_level;power.magic_level=stats.magic_attack_level;
    power.physical_min=stats.min_physical;power.physical_max=stats.max_physical;
    power.magic_min=stats.min_magic;power.magic_max=stats.max_magic;
    bool final_type=false;
    for(const auto& d:NativePlayerSkillData(*s.payload,skill)) {
        const auto type=d.bType,exec=d.bExec;
        const auto attr=d.bAttr;
        const auto calc=d.bCalc;
        const auto inc=d.bInc;
        if(d.bAction!=SA_ONCE||type!=SDT_CURE||(exec!=SCT_HP&&exec!=SCT_MP))
            throw std::runtime_error("Action item effects require unsupported Defend behavior");
        out.instant_cure.push_back({SA_ONCE,type,attr,exec,inc,d.wValue,
            d.wValueInc,calc});
        if(!final_type) {
            if(attr==1||attr==2){power.attack_type=1;final_type=true;}
            else if(attr==3){power.attack_type=3;final_type=true;}
            else if(attr>=4&&attr<=9)power.attack_type=3;
        }
    }
    if(out.instant_cure.empty())throw std::runtime_error("Action item has no supported actual effect");
    if(std::any_of(out.instant_cure.begin(),out.instant_cure.end(),[](const auto& d){return d.bCalc==2;})) {
        // FTYPE_1ST34 is SQL FLOAT53; the original loader stores it in FLOAT.
        soci::row formula;
        sql<<"SELECT \"fRateX\" FROM character_compat.\"TFORMULACHART\" WHERE \"bID\"=34",soci::into(formula);
        if(!sql.got_data())throw std::runtime_error("Action item requires pinned FTYPE_1ST formula");
        const double growth=formula.get<double>(0);
        if(!std::isfinite(growth)||growth<std::numeric_limits<float>::lowest()||growth>std::numeric_limits<float>::max())
            throw std::runtime_error("Action item growth coefficient outside source FLOAT domain");
        out.f1stRateX=static_cast<float>(growth);
        out.bStartLevel=u8("bLevel");out.bNextLevel=u8("bNextLevel");
    }
    out.attack_profile=power;return out;
}
std::optional<std::uint8_t> PostgreSQLMapService::ResolveNativeCureEffectRank(const CharSnapshot& s,std::uint16_t skill,std::uint8_t trigger_rank) {
    if(!s.payload||!s.payload->effects||!SupportedPostures(*s.payload))
        throw std::runtime_error("Cure exception requires actual supported maintained state");
    // Source128/229 erases matching maintained129/230 rows before Defend. Those
    // richer effects are not admitted by the current native graph; prove their
    // actual absence rather than silently skipping a present erase branch.
    if((skill==128||skill==229)&&std::any_of(s.payload->effects->begin(),s.payload->effects->end(),
        [&](const auto& effect){return effect.skill==skill+1;}))
        throw std::runtime_error("Cure partner-effect removal is unsupported");
    if(skill!=3604)return trigger_rank;
    const auto learned=std::find_if(s.payload->skills.begin(),s.payload->skills.end(),
        [](const auto& row){return row.wSkillID==3603;});
    if(learned==s.payload->skills.end())return std::nullopt;
    // FindTSkill tests object presence, including a present rank0. Only the
    // effect rank changes; trigger critical and outgoing powers stay intact.
    return learned->bLevel;
}
void PostgreSQLMapService::PreflightActItemCure(const CharSnapshot& s,const SkillTemplate& definition) {
    if(!s.payload||!definition.attack_profile)throw std::runtime_error("Action item preflight lacks current source profile");
    const auto& power=*definition.attack_profile;
    const auto effect_rank=ResolveNativeCureEffectRank(s,definition.wID,power.rank);
    // ITEMUSE/ACT do not look up3603 in the source. Missing3603 terminates a
    // later eligible FINISH before Defend, including its MISS802 branch. It
    // must not suppress the source initiation, activity, reuse or item debit.
    if(!effect_rank)return;
    if(s.cluster.mode==2||power.critical==255) {
        if(s.payload->avoid_effect_exists&&!s.payload->avoid_effect_supported)
            throw std::runtime_error("Action item MISS requires unsupported actual avoid802 effects");
        return; // source MISS skips GetValue/combine evaluation
    }
    // Validate the current-rank cure before hooks/debit, including checked
    // exponential conversion and the original signed-INT variance product.
    for(const auto& row:definition.instant_cure)
        (void)native_cure::Delta(row,definition,*effect_rank,row.bExec==SCT_HP?s.dwMaxHP:s.dwMaxMP);
}
bool PostgreSQLMapService::UseNativeActItems(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::span<const std::byte> body,QuestPackets& packets,std::vector<std::byte>& accepted) const {
    using tnetlib::protocol::MessageId;
    wire::Reader r(body.data(),body.size());SkillUseAckFields ack;std::uint8_t channel{};std::uint16_t map{};float x{},y{},z{};SkillTarget target;
    ack.can_select=0; // source failure and success both use0, unlike the generic native default
    if(!r.Read(ack.attack_id)||!r.Read(ack.attack_type)||!r.Read(channel)||!r.Read(map)||!r.Read(ack.skill_id)||
       !r.Read(ack.action_id)||!r.Read(ack.act_id)||!r.Read(ack.ani_id)||!r.Read(x)||!r.Read(y)||!r.Read(z)||
       !r.Read(target.id)||!r.Read(target.type)||!r.Eof()||!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||
       ack.attack_id!=c.char_id||ack.attack_type!=1||target.id!=c.char_id||target.type!=1)
        throw std::runtime_error("ACTITEMSUSE requires exact bounded own-PC request");
    const auto reply=[&](std::uint8_t result){ack.result=result;packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_SKILLUSE_ACK),EncodeSkillUseAck(ack,{}));};
    if(s.cluster.riding){reply(28);return false;}
    if(CellOwner(sql,c,s.wMapID,s.fPosX,s.fPosZ)!=m_config.server)
        throw std::runtime_error("Action item actor is outside its physical Map owner");
    if(!s.payload)throw std::runtime_error("Action item actor has no source skill authority");
    if(!NativePlayerSkillAvailable(*s.payload,ack.skill_id)){reply(SKILL_NOTFOUND);return false;}
    const auto hp=s.dwHP,mp=s.dwMP;
    RefreshEquipment(sql,s,true);
    if(s.dwHP!=hp||s.dwMP!=mp)throw std::runtime_error("Action item resources exceed actual source maxima");
    if(s.bDead||!s.dwHP)throw std::runtime_error("Action item initiation requires a living native actor");
    const auto definition=ReadActItemSkill(sql,s,ack.skill_id);
    if(!definition){reply(SKILL_NOTFOUND);return false;}
    PreflightActItemCure(s,*definition);
    if(definition->wMapID!=0xffff&&definition->wMapID!=s.wMapID){reply(SKILL_WRONGREGION);return false;}
    if(definition->wPrevActiveID&&std::none_of(s.payload->effects->begin(),s.payload->effects->end(),[&](const auto& e){return e.skill==definition->wPrevActiveID;})) {
        reply(SKILL_NEEDPREVACT);return false;
    }
    if(!s.recovery||!s.payload->item_cooldowns||!s.payload->event_ms)throw std::runtime_error("ACTITEMSUSE requires actual entry clocks");
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    p->last_attack_ms=p->event_ms;s.recovery->last_attack_tick=static_cast<std::uint32_t>(p->event_ms);
    EraseNativeActionEffects(sql,c,s,ack.skill_id,false,packets);
    if(!s.dwMP)for(std::size_t i=0;i<s.payload->effects->size();)
        if(ErasesAtZeroMp(*s.payload,s.payload->effects->at(i).skill))EraseProgressEffect(sql,c,s,i,packets);else ++i;
    const int item_id=std::bit_cast<std::int16_t>(definition->wUseItem);soci::row item;
    sql<<"SELECT \"wDelayGroupID\",\"dwDelay\",\"bKind\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:i",soci::use(item_id),soci::into(item);
    if(!sql.got_data())throw std::runtime_error("ACTITEMSUSE requires a pinned consumed item");
    const auto group=static_cast<std::uint16_t>(item.get<int>(0));const auto delay=static_cast<std::uint32_t>(item.get<int>(1));
    // Source compares absolute DWORD ticks directly. Do not silently substitute
    // monotonic64 arithmetic across a DWORD wrap in this bounded producer.
    const auto now=s.payload->event_ms;
    if(delay>0xffffffffULL-static_cast<std::uint32_t>(now))throw std::runtime_error("Action item delay crosses unsupported source DWORD wrap");
    p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;auto& reuse=*p->item_cooldowns;
    SampleItemCooldowns(reuse,p->event_ms);
    const auto prior=reuse.deadlines.find(group);
    if(prior!=reuse.deadlines.end()&&(prior->second>>32)!=(now>>32))
        throw std::runtime_error("Action item prior deadline crosses unsupported source DWORD wrap");
    if(prior!=reuse.deadlines.end()&&prior->second>=p->event_ms){reply(SKILL_SPEEDYUSE);return true;}
    const auto timer=std::find_if(reuse.timers.begin(),reuse.timers.end(),[&](const auto& v){return v.group==group;});
    if(timer==reuse.timers.end())reuse.timers.push_back({group,delay});else timer->remaining=delay;
    reuse.deadlines[group]=p->event_ms+delay;OrderItemCooldowns(reuse.timers);
    if(reuse.timers.size()>255)throw std::runtime_error("Action item delay groups exceed source count");
    std::vector<std::byte> item_ack;wire::WritePOD<std::uint8_t>(item_ack,0);wire::WritePOD(item_ack,group);
    wire::WritePOD<std::uint8_t>(item_ack,static_cast<std::uint8_t>(item.get<int>(2)));wire::WritePOD(item_ack,delay);
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_ITEMUSE_ACK),std::move(item_ack));
    const auto owned=FindSkillConsumable(s,definition->wUseItem);
    if(!owned){reply(29);return true;} // source commits reuse/erase/activity before NEEDITEM
    DebitNativeItem(sql,c,s,owned->bInvenID,owned->bItemID,owned->dlID,1,packets);
    ack.result=0;ack.skill_level=1;ack.can_select=0;ack.attacker_level=s.bLevel;ack.country=s.bCountry;ack.aid_country=s.payload->aid_country;
    ack.gnd_x=s.fPosX;ack.gnd_y=s.fPosY;ack.gnd_z=s.fPosZ;
    for(const auto& bag:s.payload->bags)if(bag.bag.bInvenID==254)for(const auto& equipped:bag.items) {
        const int id=std::bit_cast<std::int16_t>(equipped.wItemID);int special=0;
        sql<<"SELECT \"bIsSpecial\" FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:i",soci::use(id),soci::into(special);
        if(!sql.got_data())throw std::runtime_error("Equipped item lost its pinned template");
        if(special)ack.equip_special=1;
    }
    accepted=EncodeSkillUseAck(ack,{target});
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_SKILLUSE_ACK),accepted,QuestAudience::Nearby);return true;
}
}
