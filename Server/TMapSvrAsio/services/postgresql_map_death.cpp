#include "postgresql_map_service.h"
#include "posture_effects.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <algorithm>
#include <stdexcept>

namespace tmapsvr {
bool PostgreSQLMapService::DropNativeDamage(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::span<const std::byte> body,const QuestMapFacts& facts,QuestPackets& packets) const {
    wire::Reader r(body.data(),body.size());std::uint32_t target=0,damage=0;
    std::uint8_t type=0,wire_channel=0;std::uint16_t wire_map=0;
    if(!r.Read(target)||!r.Read(type)||!r.Read(damage)||!r.Read(wire_channel)||!r.Read(wire_map)||!r.Eof())
        throw std::runtime_error("Invalid native DROPDAMAGE body");
    // Source parses but does not use the trailing channel/map. Authority is
    // the actual local map/claim, never client-supplied routing metadata.
    (void)wire_channel;(void)wire_map;
    if(target!=c.char_id||type!=1||!s.dwHP)return false;
    if(!s.recovery||!s.payload||!facts.sample_ms||s.payload->tournament_player||s.payload->arena_id||
       s.cluster.riding||s.cluster.tactics||(s.wMapID>=500&&s.wMapID<533))
        throw std::runtime_error("DROPDAMAGE requires supported ordinary PC state");
    const auto hp=s.dwHP,mp=s.dwMP;
    RefreshEquipment(sql,s,true); // actual graph/companion/maintained/stat proof
    if(s.dwHP!=hp||s.dwMP!=mp)throw std::runtime_error("Drop damage would silently clamp current resources");
    if(CellOwner(sql,c,s.wMapID,s.fPosX,s.fPosZ)!=m_config.server)
        throw std::runtime_error("Drop damage requires the current physical Map owner");
    const bool lethal=damage>=s.dwHP;
    if(lethal) {
        int published=0,unsupported=0;const int map=s.wMapID;
        // The original castle/local/GodBall branches require their complete
        // registries. Check publication and absence in one SQL snapshot, never
        // combine an old release proof with rows from a later publication.
        sql<<"SELECT (SELECT count(*) FROM death_compat.catalog_release WHERE routing_manifest=:routing AND status='verified'), "
             "(SELECT count(*) FROM death_compat.\"TBATTLEZONECHART\" WHERE (\"wMapID\"::integer & 65535)=:map OR \"wID\"=0) + "
             "(SELECT count(*) FROM death_compat.\"TGODBALLCHART\" WHERE (\"wMapID\"::integer & 65535)=:map)",
            soci::use(m_config.routing_manifest,"routing"),soci::use(map,"map"),soci::into(published),soci::into(unsupported);
        if(published!=1)throw std::runtime_error("Lethal drop requires the pinned death context publication");
        if(unsupported)throw std::runtime_error("Death requires unsupported local/castle/GodBall gameplay");
    }
    s.dwHP=s.dwHP>damage?s.dwHP-damage:0;
    s.recovery->hp_tick=s.recovery->mp_tick=static_cast<std::uint32_t>(facts.sample_ms)+5000U;
    using tnetlib::protocol::MessageId;
    std::vector<std::byte> drop;wire::WritePOD(drop,c.char_id);wire::WritePOD(drop,std::uint8_t{1});wire::WritePOD(drop,damage);
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_DROPDAMAGE_ACK),std::move(drop),QuestAudience::Neighbors);
    // Original DROP publishes current MP before OnDie clears it. Do not add a
    // replacement full/dead-resource ACK or notify silent death effect removal.
    packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK),
        EncodeHpMpAck(c.char_id,1,s.dwMaxHP,s.dwHP,s.dwMaxMP,s.dwMP),QuestAudience::Neighbors);
    if(lethal) {
        s.dwHP=s.dwMP=0;s.bDead=1;
        auto p=std::make_shared<CharacterPayload>(*s.payload);s.payload=p;
        std::erase_if(*p->effects,[&](const MaintainedEffect& effect) {
            if(IsNativePosture(effect.skill))return false; // actual pinned static template
            const auto def=std::find_if(p->progress_effect_templates.begin(),p->progress_effect_templates.end(),
                [&](const auto& d){return d.skill==effect.skill;});
            if(def==p->progress_effect_templates.end())throw std::runtime_error("Death effect has no supported template");
            return !def->is_static; // ReleaseMaintain defaults to bNotify=FALSE
        });
        RefreshEquipment(sql,s,true);
        packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_DIE_ACK),EncodeDieAck(c.char_id,1),QuestAudience::Neighbors);
    }
    return true;
}
}
