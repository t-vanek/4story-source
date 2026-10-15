#include "postgresql_map_service.h"
#include "posture_effects.h"
#include "MessageId.h"
#include <soci/soci.h>
#include <bit>
#include <limits>
#include <stdexcept>

namespace tmapsvr {
void PostgreSQLMapService::ChangeNativeMode(CharSnapshot& s,std::uint8_t mode,std::uint64_t now,QuestPackets& packets) {
    if(!s.recovery)throw std::runtime_error("Mode change requires the actual local ENTERCHAR clocks");
    const auto tick=static_cast<std::uint32_t>(now);
    s.cluster.mode=mode;s.recovery->last_attack_tick=tick;
    if(mode==1)s.recovery->hp_tick=s.recovery->mp_tick=tick+5000U;
    std::vector<std::byte> body;wire::WritePOD(body,s.dwCharID);
    wire::WritePOD(body,std::uint8_t{1});wire::WritePOD(body,mode);
    packets.emplace_back(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_CHGMODE_ACK),std::move(body),QuestAudience::Neighbors);
}
void PostgreSQLMapService::AdvanceRecovery(soci::session& sql,const MapSessionClaim& c,CharSnapshot& s,
    std::uint64_t now,QuestPackets& packets) {
    if(s.bDead||!s.dwHP)return;
    const auto hp=s.dwHP,mp=s.dwMP;
    RefreshEquipment(sql,s,true); // current cluster/graph, never a formerly valid cached sheet
    if(s.dwHP!=hp||s.dwMP!=mp)throw std::runtime_error("Recovery would silently clamp current resources");
    if(!s.recovery||!s.payload||!s.payload->statistics||!s.payload->recovery_rates||!SupportedPostures(*s.payload))
        throw std::runtime_error("Recovery requires supported actual statistics and local clocks");
    // HaveStopRecover scans actual maintained templates, independent of action.
    for(const auto& effect:*s.payload->effects) {
        const int skill=std::bit_cast<std::int16_t>(effect.skill);int stopped=0;
        sql<<"SELECT count(*) FROM character_compat.\"TSKILLDATA\" WHERE \"wSkillID\"=:skill AND \"bType\"=6 AND \"bExec\"=48",
            soci::use(skill),soci::into(stopped);
        if(stopped)return;
    }
    const auto tick=static_cast<std::uint32_t>(now);bool changed=false;
    const auto advance=[&](std::uint32_t& current,std::uint32_t maximum,std::uint32_t rate,std::uint32_t& previous) {
        if(tick<static_cast<std::uint32_t>(previous+3000U)||current>=maximum)return;
        // Source adds DWORD before clamping. Wrapped resource/death semantics
        // are outside this bounded path; never substitute saturation.
        if(rate>std::numeric_limits<std::uint32_t>::max()-current)
            throw std::runtime_error("Recovery DWORD addition overflow is not supported");
        current+=rate;if(current>maximum)current=maximum;
        previous=tick;changed=true; // zero gain still changes the source clock
    };
    if(s.cluster.mode==0)advance(s.dwHP,s.dwMaxHP,(*s.payload->recovery_rates)[0],s.recovery->hp_tick);
    advance(s.dwMP,s.dwMaxMP,(*s.payload->recovery_rates)[1],s.recovery->mp_tick);
    if(changed)packets.emplace_back(static_cast<std::uint16_t>(tnetlib::protocol::MessageId::CS_HPMP_ACK),
        EncodeHpMpAck(c.char_id,1,s.dwMaxHP,s.dwHP,s.dwMaxMP,s.dwMP),QuestAudience::Neighbors);
}
void PostgreSQLMapService::AdvanceBattleMode(CharSnapshot& s,std::uint64_t now,QuestPackets& packets) {
    if(s.cluster.mode!=1)return;
    if(!s.recovery)throw std::runtime_error("Battle timeout requires the actual local ENTERCHAR clock");
    if(static_cast<std::uint32_t>(s.recovery->last_attack_tick+5000U)<static_cast<std::uint32_t>(now))
        ChangeNativeMode(s,0,now,packets);
}
}
