#include "native_monster_motion.h"
#include "native_monster_ai.h"
#include "native_monster_skills.h"
#include "wire_codec.h"
#include "packet_codec.h"
#include <cmath>
#include <map>
#include <utility>

namespace tmapsvr {
namespace {
float Distance(const NativeMonsterSlot& slot) {
    const float dx=slot.start_x-slot.x,dz=slot.start_z-slot.z;
    const float distance=std::sqrt(dx*dx+dz*dz); // source FLOAT order, sqrtf
    if(!std::isfinite(distance))throw std::runtime_error("Native monster movement distance is not finite");
    return distance;
}
void WriteMotion(std::vector<std::byte>& out,std::uint32_t id,const NativeMonsterMotion& m) {
    wire::WriteLE(out,id);wire::WriteLE<std::uint8_t>(out,2); // ordinary OT_MON
    wire::WriteLE(out,m.x);wire::WriteLE(out,m.y);wire::WriteLE(out,m.z);
    wire::WriteLE(out,m.pitch);wire::WriteLE(out,m.direction);
    wire::WriteLE(out,m.mouse);wire::WriteLE(out,m.key);wire::WriteLE(out,m.action);
}
}
std::optional<std::vector<NativeMonsterMoveInput>> DecodeNativeMonMoveRequest(std::span<const std::byte> body) {
    if(body.size()>tnetlib::kMaxPacketSize-tnetlib::kPacketHeaderSize)return {};
    wire::Reader reader(body.data(),body.size());
    std::uint16_t count{};
    if(!reader.ReadLE(count)||reader.Remaining()!=std::size_t{count}*27)return {};
    std::vector<NativeMonsterMoveInput> out;out.reserve(count);
    for(unsigned i=0;i<count;++i) {
        NativeMonsterMoveInput record;auto& m=record.motion;
        if(!reader.ReadLE(record.id)||!reader.ReadLE(record.type)||!reader.ReadLE(record.channel)||!reader.ReadLE(record.map)||
           !reader.ReadLE(m.x)||!reader.ReadLE(m.y)||!reader.ReadLE(m.z)||!reader.ReadLE(m.pitch)||!reader.ReadLE(m.direction)||
           !reader.ReadLE(m.mouse)||!reader.ReadLE(m.key)||!reader.ReadLE(m.action)||
           !std::isfinite(m.x)||!std::isfinite(m.y)||!std::isfinite(m.z))return {};
        out.push_back(record);
    }
    return out;
}
std::vector<std::byte> EncodeNativeMonActionAck(std::uint32_t id,const NativeMonsterAction& action) {
    std::vector<std::byte> out;out.reserve(22);
    wire::WriteLE(out,id);wire::WriteLE(out,action.action);
    wire::WriteLE(out,action.x);wire::WriteLE(out,action.y);wire::WriteLE(out,action.z);
    wire::WriteLE(out,action.target);wire::WriteLE(out,action.target_type);
    return out;
}
std::vector<std::byte> EncodeNativeMonMoveAck(const std::vector<NativeMonsterMoveRecord>& records) {
    if(records.empty())return {}; // CSSender::SendCS_MONMOVE_ACK(map): no packet, not WORD0
    if(records.size()>(tnetlib::kMaxPacketSize-tnetlib::kPacketHeaderSize-2)/24)
        throw std::runtime_error("Native MONMOVE output exceeds source framing");
    std::vector<std::byte> out;out.reserve(2+24*records.size());
    wire::WriteLE(out,static_cast<std::uint16_t>(records.size()));
    for(const auto& record:records) {
        if(!record.generation)throw std::runtime_error("Native MONMOVE record has no captured source lifetime");
        WriteMotion(out,record.id,record.motion);
    }
    return out;
}
bool NativeMonsterAiExecutor::Follow(NativeMonsterMap& map,std::uint32_t id,const NativeAiBinding& binding,Arguments args) {
    auto& slot=map.slots.at(id);const auto generation=slot.generation;
    if(const auto host=FindNativeMonsterHost(map,slot.x,slot.z,slot.host)) {
        const auto pose=m_player_pose(*host); // actual accepted object, including its real Y
        const auto& player=map.players.at(host->char_id);
        if(pose.map!=map.map_id||pose.x!=player.x||pose.z!=player.z||
           !std::isfinite(pose.x)||!std::isfinite(pose.y)||!std::isfinite(pose.z))
            throw std::runtime_error("Native FOLLOW pose differs from actual source host/map lifetime");
        NativeCellNotice notice{NativeCellNoticeKind::MonsterAction,*host,{}};
        notice.monster_id=id;notice.monster_generation=generation;
        notice.monster_action=NativeMonsterAction{9,pose.x,pose.y,pose.z,slot.target,slot.target_type}; // TA_FOLLOW
        m_publish(NativeCellNotices{std::move(notice)});
        auto found=map.slots.find(id);
        if(found==map.slots.end()||found->second.generation!=generation)return false;
        if(auto skill=SelectNativeMonsterSkill(m_catalog,map,found->second,host,false,m_tick))
            m_publish(NativeCellNotices{std::move(*skill)});
    }
    auto found=map.slots.find(id);
    if(found==map.slots.end()||found->second.generation!=generation)return false;
    ++found->second.host_key; // source increments even when FindHost fails
    return Complete(map,id,binding,args);
}
void NativeMonsterAiExecutor::MoveHostedMonsters(NativeMonsterMap& map,const MapSessionClaim& reporter,
                                                std::span<const NativeMonsterMoveInput> records) {
    const auto player=map.players.find(reporter.char_id);
    if(player==map.players.end()||!SameNativeCellPlayer(player->second.claim,reporter))
        throw std::runtime_error("Native MONMOVE has no current reporting player/map lifetime");
    if(records.size()>(tnetlib::kMaxPacketSize-tnetlib::kPacketHeaderSize-2)/27)
        throw std::runtime_error("Native MONMOVE input exceeds source framing");
    // First insertion owns the lifetime, just as source mapMon retains the first
    // pointer per DWORD. Final projection is delayed until the WHOLE input ends.
    std::map<std::uint32_t,std::uint64_t> accepted;
    for(const auto& record:records) {
        Step();
        if(record.type!=2) {
            if(record.type==7||record.type==11||record.type==18)
                throw std::runtime_error("Native MONMOVE requires actual recall/self/companion object lifetimes");
            continue; // source switch has no object for other types
        }
        auto found=map.slots.find(record.id);
        if(found==map.slots.end()||!found->second.monster||found->second.status==0)continue; // CTMap::FindMonster
        auto& initial=found->second;
        if(initial.map!=map.map_id||initial.channel!=map.physical_channel)
            throw std::runtime_error("Native MONMOVE source object/map index mismatch");
        if(initial.host!=reporter.char_id||initial.action==7)continue;
        const auto& m=record.motion;
        if(m.x<=0||m.z<=0)continue;
        if(!std::isfinite(m.y)||!NativeCellAt(m.x,m.z))
            throw std::runtime_error("Native MONMOVE requires finite defined source WORD coordinates");
        const auto generation=initial.generation;
        const auto current=[&]() -> NativeMonsterSlot* {
            const auto object=map.slots.find(record.id);
            return object!=map.slots.end()&&object->second.generation==generation?&object->second:nullptr;
        };
        initial.mouse=initial.hp?m.mouse:4;initial.key=initial.hp?m.key:4;
        initial.action=m.action;initial.pitch=m.pitch;initial.direction=m.direction;initial.y=m.y;
        if(m_move(map,record.id,m.x,m.z))Event(map,record.id,8,0,{}); // AT_LEAVE once, after cells
        auto* slot=current();if(!slot)continue;
        // Preserve the source's post-OnMove comparison. It compares the NEW
        // action unless synchronous AT_LEAVE changed it; do not invent oldAction.
        if(slot->action!=0&&m.action==0)slot->recover_mp_tick=m_tick+std::uint32_t{5000};
        if(slot->mode==1) {
            // OT_PC deliberately means the reporter itself, not target-ID lookup.
            // Other target types use reporter-owned registries still unported.
            if(slot->target_type!=1)
                throw NativeMonsterInitializationRequired(slot->map,slot->channel,slot->id,
                    "MONMOVE battle target recall/self/companion producer and lookup");
            const auto actor=map.players.find(reporter.char_id);
            if(actor==map.players.end()||!SameNativeCellPlayer(actor->second.claim,reporter)) {
                (void)LeaveAggro(map,record.id,{slot->host,slot->target,slot->target_type});continue;
            }
            if(!slot->monster)throw std::runtime_error("Native MONMOVE battle event removed its source monster template");
            if(m_catalog.monsters.at(*slot->monster).chase_range<Distance(*slot))
                (void)LeaveAggro(map,record.id,{slot->host,slot->target,slot->target_type});
        } else if(slot->mode==2) {
            if(Distance(*slot)<=3.0f)Event(map,record.id,11,0,{slot->host,slot->target,slot->target_type}); // AT_ATHOME
        } else if(Distance(*slot)>64.0f) {
            throw NativeMonsterInitializationRequired(slot->map,slot->channel,slot->id,
                "MONMOVE out-of-roam OnDie: transformation, loot, rewards, quest and dead AI lifecycle");
        }
        slot=current();if(slot&&slot->status!=3)accepted.try_emplace(record.id,generation);
    }
    auto projected=std::make_shared<std::vector<NativeMonsterMoveRecord>>();projected->reserve(accepted.size());
    for(const auto& [id,generation]:accepted) {
        const auto found=map.slots.find(id);
        if(found==map.slots.end()||found->second.generation!=generation)continue; // never dereference removed/reused source pointer
        const auto& slot=found->second;
        projected->push_back({id,generation,{slot.x,slot.y,slot.z,slot.pitch,slot.direction,slot.mouse,slot.key,slot.action}});
    }
    if(projected->empty())return; // actual source sender suppresses an empty batch
    const auto actor=map.players.find(reporter.char_id);
    if(actor==map.players.end()||!SameNativeCellPlayer(actor->second.claim,reporter))return;
    const auto position=NativeCellAt(actor->second.x,actor->second.z);
    if(!position)throw std::runtime_error("Native MONMOVE reporter has undefined source cell");
    NativeCellNotices notices;
    const auto cells=NativeNeighborCells(map.cells,*position);
    for(auto cell=cells.rbegin();cell!=cells.rend();++cell) {
        const auto& neighbors=map.cells.at(*cell).players;
        for(auto neighbor=neighbors.rbegin();neighbor!=neighbors.rend();++neighbor)if(neighbor->first!=reporter.char_id) {
            NativeCellNotice notice{NativeCellNoticeKind::MonsterMoveBatch,neighbor->second.claim,{}};
            notice.monster_moves=projected;notices.push_back(std::move(notice));
        }
    }
    m_publish(std::move(notices));
}
} // namespace tmapsvr
