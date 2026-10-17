#include "postgresql_map_service.h"
#include "native_skill_state.h"
#include "posture_effects.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tmapsvr {
namespace {
using tnetlib::protocol::MessageId;
std::uint32_t Interval(std::uint8_t step) {
    return static_cast<std::uint32_t>(27+step*0.3)*1000U;
}
std::vector<std::byte> Aftermath(const CharSnapshot& s) {
    std::vector<std::byte> out;wire::WritePOD(out,s.dwCharID);wire::WritePOD(out,s.bAftermath);return out;
}
void Sheet(const CharSnapshot& s,QuestPackets& packets) {
    if(!s.payload||!s.payload->statistics)throw std::runtime_error("Revival requires supported actual statistics");
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_CHARSTATINFO_ACK),EncodeCharacterStatistics(s,*s.payload->statistics));
}
int Cell(float value) {
    if(!std::isfinite(value)||value<0||value>=65536)throw std::runtime_error("Revival position outside source cell range");
    return static_cast<std::uint16_t>(value)/64;
}
}
void PostgreSQLMapService::RebaseAftermath(const CharSnapshot& s,CharacterPayload& p,std::uint64_t now) {
    if(s.bAftermath>100||(s.bAftermath&&s.bLevel<10))
        throw std::runtime_error("Loaded aftermath requires unsupported source step normalization");
    p.aftermath_tick=s.bAftermath?static_cast<std::uint32_t>(now)+Interval(s.bAftermath):0;
}
bool PostgreSQLMapService::AdvanceAftermath(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::uint64_t now,QuestPackets& packets) {
    if(!s.bAftermath)return false;
    if(s.bLevel<10||s.bAftermath>100)throw std::runtime_error("Unsupported aftermath decay context");
    if(!(s.payload->aftermath_tick<static_cast<std::uint32_t>(now)))return false;
    const auto hp=s.dwHP,mp=s.dwMP;
    RefreshEquipment(sql,s,true); // derive the actual powered MTYPE_AFTERMATH sum
    const auto decrement=static_cast<std::uint8_t>(1U+s.payload->aftermath_item_bonus);
    // Source BYTE subtraction can underflow. Its >100 statistic model is not
    // supported here; never turn that branch into an invented clamped recovery.
    if(decrement>s.bAftermath)throw std::runtime_error("Aftermath decrement underflow semantics are not implemented");
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
    s.bAftermath=static_cast<std::uint8_t>(s.bAftermath-decrement);
    p->aftermath_tick+=Interval(s.bAftermath);
    RefreshEquipment(sql,s,true);
    if(s.dwHP!=hp||s.dwMP!=mp)throw std::runtime_error("Aftermath recovery would clamp source current resources");
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_AFTERMATH_ACK),Aftermath(s),QuestAudience::Neighbors);
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK),EncodeHpMpAck(c.char_id,1,s.dwMaxHP,s.dwHP,s.dwMaxMP,s.dwMP),QuestAudience::Neighbors);
    Sheet(s,packets);return true;
}
bool PostgreSQLMapService::ReviveNative(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::span<const std::byte> body,const QuestMapFacts& facts,QuestPackets& packets) const {
    wire::Reader r(body.data(),body.size());float x=0,y=0,z=0;std::uint8_t type=0;
    if(!r.Read(x)||!r.Read(y)||!r.Read(z)||!r.Read(type)||!r.Eof()||
       !std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))
        throw std::runtime_error("Invalid native REVIVAL body");
    if(!s.bDead||s.dwHP)return false; // intentional dead/current-resource hardening
    if(!s.payload||!s.payload->statistics||!SupportedPostures(*s.payload)||!facts.sample_ms||
       s.payload->tournament_player||s.payload->arena_id||s.cluster.riding||s.cluster.guild||s.cluster.tactics||
       (s.wMapID>=500&&s.wMapID<533)||s.bAftermath>100||(s.bLevel<10&&s.bAftermath))
        throw std::runtime_error("Revival requires supported ordinary source context");
    if(s.payload->revival_effect_exists&&!s.payload->revival_effect_supported)
        throw std::runtime_error("Existing revival800 template requires unsupported source effects");
    if(Cell(x)!=Cell(s.fPosX)||Cell(z)!=Cell(s.fPosZ)||
       CellOwner(sql,c,s.wMapID,x,z)!=m_config.server)
        throw std::runtime_error("Cross-cell revival requires full source movement admission");
    s.fPosX=x;s.fPosY=y;s.fPosZ=z;
    const auto old=s.bAftermath;
    if(s.bLevel>=10)s.bAftermath=static_cast<std::uint8_t>(std::min<unsigned>(100,s.bAftermath+(type==0?20:10)));
    if(s.bAftermath!=old) {
        auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
        p->aftermath_tick=static_cast<std::uint32_t>(facts.sample_ms)+Interval(s.bAftermath);
        RefreshEquipment(sql,s,true);
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_AFTERMATH_ACK),Aftermath(s),QuestAudience::Neighbors);
        Sheet(s,packets);
    }else RefreshEquipment(sql,s,true);
    if(!s.dwMaxHP)throw std::runtime_error("Revival zero maximum-HP semantics are not supported");
    auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;p->last_attack_ms=facts.sample_ms;
    ChangeNativeMode(s,0,facts.sample_ms,packets); // NORMAL retains prior recovery deadlines
    const double rate=type==0?0.3:0.4;
    s.dwHP=std::max<std::uint32_t>(1,static_cast<std::uint32_t>(s.dwMaxHP*rate));
    s.dwMP=static_cast<std::uint32_t>(s.dwMaxMP*rate);s.bDead=0;
    s.movement.action=0; // TPlayer::Revival sets TA_STAND on the existing object
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_REVIVAL_ACK),EncodeRevivalAck(c.char_id,x,y,z),QuestAudience::Neighbors);
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK),EncodeHpMpAck(c.char_id,1,s.dwMaxHP,s.dwHP,s.dwMaxMP,s.dwMP),QuestAudience::Neighbors);
    ForceProgressEffect(sql,c,s,800,0,packets);return true;
}
void PostgreSQLMapService::EraseNativeActionEffects(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::uint16_t skill,bool defend,QuestPackets& packets,std::vector<EquipmentEffectEvent>* ended) {
    if(!skill||IsNativePosture(skill))return;
    // IsPosture and the attack-only IsRecall exception inspect every source
    // row; SER_SKILL12 is deliberately excluded from that recall predicate.
    for(const auto& d:NativePlayerSkillData(*s.payload,skill))
        if((d.bType==SDT_STATUS&&(d.bExec==7||d.bExec==26||d.bExec==27))||
           (!defend&&d.bType==SDT_RECALL&&d.bExec!=12))return;
    for(std::size_t i=0;i<s.payload->effects->size();) {
        const auto effect=s.payload->effects->at(i).skill;
        const auto definition=std::find_if(s.payload->progress_effect_templates.begin(),s.payload->progress_effect_templates.end(),[&](const auto& d){return d.skill==effect;});
        if((effect==800||effect==802)&&definition!=s.payload->progress_effect_templates.end()&&(definition->erase_act&(defend?2:1))) {
            EraseProgressEffect(sql,c,s,i,packets);
            if(ended)ended->push_back({false,effect,std::make_shared<const CharSnapshot>(s)});
        }else ++i;
    }
}
}
