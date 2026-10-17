#include "native_monster_ai.h"
#include "native_monster_skills.h"
#include <cmath>
#include <utility>

namespace tmapsvr {
void NativeMonsterAiExecutor::ChangeMode(NativeMonsterMap& map,std::uint32_t id,std::uint8_t mode) {
    auto& slot=map.slots.at(id);
    const auto position=NativeCellAt(slot.x,slot.z);
    if(!position)throw std::runtime_error("Native ChgMode requires source WORD coordinates");
    const auto cells=NativeNeighborCells(map.cells,*position);
    slot.mode=mode;slot.last_attack_tick=m_tick;
    if(mode==1)slot.recover_hp_tick=slot.recover_mp_tick=m_tick+std::uint32_t{5000}; // RECOVER_INIT
    NativeCellNotices out;
    std::optional<NativeMonsterColorState> color;
    for(auto cell=cells.rbegin();cell!=cells.rend();++cell) {
        const auto& players=map.cells.at(*cell).players;
        for(auto player=players.rbegin();player!=players.rend();++player) {
            if(!color)color=ProjectNativeMonsterColor(slot,m_catalog,m_ai); // no ADDMON/vitals dependency
            NativeCellNotice notice{NativeCellNoticeKind::MonsterMode,player->second.claim,{}};
            notice.monster_id=id;notice.monster_generation=slot.generation;notice.mode=mode;
            out.push_back(notice); // mode then color for EACH recipient, not two grouped broadcasts
            notice.kind=NativeCellNoticeKind::MonsterColor;notice.color=color;out.push_back(std::move(notice));
        }
    }
    m_publish(std::move(out));
}
void NativeMonsterAiExecutor::ResetHost(NativeMonsterMap& map,std::uint32_t id) {
    auto& slot=map.slots.at(id);
    const auto previous=m_find_player(slot.host); // global accepted registry, not neighboring cells
    if(previous&&previous->char_id!=slot.host)throw std::runtime_error("Native global old-host identity mismatch");
    slot.action=0;
    ChangeMode(map,id,0); // capture mode/color BEFORE clearing host/target/aggro
    slot.host=slot.target=slot.helper=0;slot.target_type=0;slot.help_count=0;
    slot.aggro.clear();slot.damage.clear();slot.guild_damage.clear();
    // Keeper, status, next skill, current bars and movement keys are untouched.
    if(!previous)return;
    m_publish(NotifyNativeMonsterHost(map,slot,previous,true));
    const float dx=slot.start_x-slot.x,dz=slot.start_z-slot.z;
    const float distance=std::sqrt(dx*dx+dz*dz);
    if(!std::isfinite(distance))throw std::runtime_error("Native reset distance requires finite source geometry");
    if(distance>32.0f) {
        NativeAiInvocation call;call.kind=NativeAiMessageKind::ResetHost;call.generation=slot.generation;
        call.monster=id;call.map=map.map_id;call.channel=map.physical_channel;call.party=map.party;
        m_schedule(call); // same source AI FIFO, then batch ACK; no artificial timer or host-key fence
    }
}
void NativeMonsterAiExecutor::ResetHostAcknowledgement(NativeMonsterMap& map,const NativeAiInvocation& call) {
    if(call.kind!=NativeAiMessageKind::ResetHost)throw std::runtime_error("Native reset ACK has wrong message kind");
    const auto found=map.slots.find(call.monster);
    if(found==map.slots.end()||found->second.generation!=call.generation)return;
    if(!map.players.empty())throw std::runtime_error("Native reset ACK requires full actor coexistence before player admission");
    auto& slot=found->second;
    if(!slot.monster||slot.status==0||slot.host)return; // source FindMonster + m_pMAP + !host
    Step();
    const auto& spawn=m_catalog.spawns.at(slot.spawn); // current real spawn, not captured randomized start position
    slot.start_x=slot.next_x=spawn.fPosX;slot.start_y=slot.next_y=slot.y=spawn.fPosY;
    slot.start_z=slot.next_z=spawn.fPosZ;slot.direction=spawn.wDir;slot.path_id=0;slot.path_direction=1;
    if(m_move(map,call.monster,spawn.fPosX,spawn.fPosZ))Event(map,call.monster,8,0,{}); // one AT_LEAVE after cells
    const auto current=map.slots.find(call.monster);
    if(current==map.slots.end()||current->second.generation!=call.generation)return; // safe after reentrant Remove
    const auto& moved=current->second;
    const auto position=NativeCellAt(moved.x,moved.z);
    if(!position)throw std::runtime_error("Native reset MONMOVE requires source WORD coordinates");
    const NativeMonsterMotion motion{moved.x,moved.y,moved.z,moved.pitch,moved.direction,4,4,0};
    NativeCellNotices out;
    const auto cells=NativeNeighborCells(map.cells,*position);
    for(auto cell=cells.rbegin();cell!=cells.rend();++cell) {
        const auto& players=map.cells.at(*cell).players;
        for(auto player=players.rbegin();player!=players.rend();++player) {
            NativeCellNotice notice{NativeCellNoticeKind::MonsterMove,player->second.claim,{}};
            notice.monster_id=moved.id;notice.monster_generation=moved.generation;notice.motion=motion;
            out.push_back(std::move(notice));
        }
    }
    m_publish(std::move(out)); // forced wire keys/stand do not overwrite source stored mouse/key/action
}
} // namespace tmapsvr
