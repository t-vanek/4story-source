#include "native_map_cells.h"
#include "native_monster_spawn.h"
#include <stdexcept>
#include <limits>

namespace tmapsvr {
bool SameNativeCellPlayer(const MapSessionClaim& a,const MapSessionClaim& b) {
    return a.char_id==b.char_id&&a.user_id==b.user_id&&a.key==b.key&&a.group==b.group&&
        a.channel==b.channel&&a.connection_id==b.connection_id&&a.role==b.role&&a.authority_epoch==b.authority_epoch;
}
namespace {
bool Main(const NativeCellPlayer& player) {return player.claim.role==MapSessionRole::Primary;}
std::uint32_t Position(float x,float z) {
    const auto cell=NativeCellAt(x,z);
    if(!cell)throw std::runtime_error("Native player cell requires defined source WORD coordinates");
    return *cell;
}
NativeMapCells::iterator Enabled(NativeMonsterMap& map,std::uint32_t key,std::uint8_t channel) {
    auto cell=map.cells.find(key);
    return cell!=map.cells.end()&&cell->second.IsEnabled(channel)?cell:map.cells.end();
}
// GetNeighbor(MAPTCELL) and GetNeighbor(VPLAYER) both walk z,x ascending;
// packed DWORD cell keys have that same order. No distance/channel filter.
std::vector<std::uint32_t> Neighbors(const NativeMonsterMap& map,std::uint32_t position) {
    return NativeNeighborCells(map.cells,position);
}
void Notice(NativeCellNotices& out,NativeCellNoticeKind kind,const NativeCellPlayer& recipient,
            const NativeCellPlayer& actor,bool flag=false) {
    out.push_back({kind,recipient.claim,actor.claim,flag});
}
void Enter(NativeCellNotices& out,const NativeCellPlayer& player,const NativeMapCell& neighbor,
           const NativeMapCell& origin,bool fresh) {
    // TCell::EnterPlayer: the two main-cell tests deliberately use DIFFERENT cells.
    for(const auto& [id,other]:neighbor.players)if(id!=player.claim.char_id) {
        if(Main(other)&&(neighbor.IsMain(player.claim.channel)||other.claim.channel==player.claim.channel))
            Notice(out,NativeCellNoticeKind::Enter,player,other);
        if(Main(player)&&origin.IsMain(other.claim.channel))
            Notice(out,NativeCellNoticeKind::Enter,other,player,fresh);
    }
}
void Leave(NativeCellNotices& out,const NativeCellPlayer& player,const NativeMapCell& neighbor,bool exit) {
    // TCell::LeavePlayer has no channel/main-cell filter.
    for(const auto& [id,other]:neighbor.players) {
        if(Main(other))Notice(out,NativeCellNoticeKind::Leave,player,other);
        if(Main(player))Notice(out,NativeCellNoticeKind::Leave,other,player,exit);
    }
}
void LeaveChannel(NativeCellNotices& out,const NativeCellPlayer& player,const NativeMapCell& neighbor,
                  const std::set<std::uint8_t>& lost,bool own_lost) {
    if(!own_lost) {
        // TCell::LeaveChannel(player,channel,FALSE), in source MAPBYTE order.
        for(auto channel:lost)for(const auto& [id,other]:neighbor.players)
            if(other.claim.channel!=player.claim.channel&&other.claim.channel==channel) {
                Notice(out,NativeCellNoticeKind::Leave,other,player);
                Notice(out,NativeCellNoticeKind::Leave,player,other);
            }
        // LeaveChannel(player) affects monsters/objects only, excluded here.
    } else {
        // TCell::LeaveChannel(player,FALSE): this branch does NOT check m_bMain.
        for(const auto& [id,other]:neighbor.players)if(other.claim.channel!=player.claim.channel) {
            Notice(out,NativeCellNoticeKind::Leave,other,player);
            if(!neighbor.IsMain(player.claim.channel))Notice(out,NativeCellNoticeKind::Leave,player,other);
        }
    }
}
void RequirePcMap(const NativeMonsterMap& map) {
    if(!map.slots.empty())throw std::runtime_error("Native PC cells require completed monster host/visibility lifecycle");
}
}
NativeCellNotices EnterNativeCellPlayer(NativeMonsterMap& map,const NativeCellPlayer& player,bool fresh) {
    RequirePcMap(map);
    if(player.claim.channel>=NativeCellChannels||!player.claim.char_id||!player.claim.connection_id)
        throw std::runtime_error("Invalid native player cell identity");
    const auto position=Position(player.x,player.z);
    if(const auto old=map.players.find(player.claim.char_id);old!=map.players.end()) {
        if(SameNativeCellPlayer(old->second.claim,player.claim)&&old->second.x==player.x&&old->second.z==player.z) {
            // CONREADY after promotion refreshes EnterMAP(FALSE) on the same
            // CTPlayer object. It never creates a second cell member.
            NativeCellNotices out;
            if(const auto origin=Enabled(map,position,player.claim.channel);origin!=map.cells.end())
                for(auto key:Neighbors(map,position))Enter(out,player,map.cells.at(key),origin->second,false);
            return out;
        }
        throw std::runtime_error("Native player cell identity still owned by another connection");
    }
    if(map.players.size()>=65536)throw std::runtime_error("Native player cell resource limit");
    NativeCellNotices out;
    map.players.emplace(player.claim.char_id,player); // EnterMAP establishes m_pMAP even without an enabled cell
    const auto origin=Enabled(map,position,player.claim.channel);
    if(origin==map.cells.end())return out;
    origin->second.players.emplace(player.claim.char_id,player);
    for(auto key:Neighbors(map,position))Enter(out,player,map.cells.at(key),origin->second,fresh);
    return out;
}
NativeCellNotices MoveNativeCellPlayer(NativeMonsterMap& map,const MapSessionClaim& claim,float x,float z,
                                     const std::vector<std::uint8_t>& channels,bool broadcast_movement) {
    RequirePcMap(map);
    const auto found=map.players.find(claim.char_id);
    if(found==map.players.end()||!SameNativeCellPlayer(found->second.claim,claim))
        throw std::runtime_error("Native move has no matching EnterMAP lifetime");
    auto& player=found->second;
    const auto before=Position(player.x,player.z),after=Position(x,z);
    if(player.revision==std::numeric_limits<std::uint64_t>::max())throw std::runtime_error("Native player cell revision exhausted");
    ++player.revision;
    NativeCellNotices out;
    if(before!=after) {
        auto old=Enabled(map,before,claim.channel),next=Enabled(map,after,claim.channel);
        std::set<std::uint8_t> lost;
        // Source OnMove narrows vector size to BYTE; duplicates do not change MAPBYTE.
        const auto count=static_cast<std::uint8_t>(channels.size());
        for(unsigned i=0;i<count;++i)if(old!=map.cells.end()&&old->second.IsMain(channels[i]))lost.insert(channels[i]);
        if(old!=map.cells.end())old->second.players.erase(claim.char_id);
        const auto old_cells=Neighbors(map,before),new_cells=Neighbors(map,after);
        const std::set<std::uint32_t> old_set(old_cells.begin(),old_cells.end()),new_set(new_cells.begin(),new_cells.end());
        for(auto key:old_cells)if(!new_set.contains(key))Leave(out,player,map.cells.at(key),false);
        player.x=x;player.z=z;
        if(next!=map.cells.end()) {
            for(unsigned i=0;i<count;++i)if(next->second.IsMain(channels[i]))lost.erase(channels[i]);
            next->second.players.emplace(claim.char_id,player);
        }
        const bool own_lost=lost.contains(claim.channel);
        for(auto key:new_cells) {
            const auto& cell=map.cells.at(key);
            if(!old_set.contains(key)&&next!=map.cells.end())Enter(out,player,cell,next->second,false);
            LeaveChannel(out,player,cell,lost,own_lost);
        }
    } else {
        player.x=x;player.z=z;
        if(auto cell=Enabled(map,after,claim.channel);cell!=map.cells.end()) {
            if(auto member=cell->second.players.find(claim.char_id);member!=cell->second.players.end())member->second=player;
        }
    }
    if(broadcast_movement&&Main(player)) {
        // CSHandler::OnCS_MOVE_REQ consumes GetNeighbor(VPLAYER) with pop_back.
        const auto cells=Neighbors(map,after);
        for(auto cell=cells.rbegin();cell!=cells.rend();++cell) {
            const auto& players=map.cells.at(*cell).players;
            for(auto other=players.rbegin();other!=players.rend();++other)if(other->first!=claim.char_id)
                Notice(out,NativeCellNoticeKind::Move,other->second,player);
        }
    }
    return out;
}
NativeCellNotices ApplyNativePlayerHostMove(NativeMonsterMap& map,const NativePlayerMovePlan& plan,
                                          const std::vector<std::uint8_t>& channels) {
    RequirePcMap(map); // real monster event consumers are still a separate missing dependency
    const auto found=map.players.find(plan.claim.char_id);
    if(found==map.players.end()||!SameNativeCellPlayer(found->second.claim,plan.claim)||
       found->second.revision!=plan.revision||map.map_id!=plan.map)
        throw std::runtime_error("Native move preparation has stale cell lifetime/revision");
    auto& player=found->second;
    NativeCellNotices out;
    if(plan.early_return) {
        if(player.revision==std::numeric_limits<std::uint64_t>::max())throw std::runtime_error("Native player cell revision exhausted");
        ++player.revision; // no body/cell/MOVE_ACK change on the source ghost return
    } else out=MoveNativeCellPlayer(map,plan.claim,plan.x,plan.z,channels,true);
    player.host=plan.host;
    const auto position=Position(player.x,player.z);
    if(auto cell=map.cells.find(position);cell!=map.cells.end())
        if(auto member=cell->second.players.find(plan.claim.char_id);member!=cell->second.players.end())member->second=player;
    return out;
}
void RebindNativeCellPlayer(NativeMonsterMap& map,const MapSessionClaim& previous,const MapSessionClaim& current) {
    auto found=map.players.find(previous.char_id);
    if(found==map.players.end()||!SameNativeCellPlayer(found->second.claim,previous))
        throw std::runtime_error("Native role change has no matching cell lifetime");
    auto expected=previous;expected.role=current.role;expected.authority_epoch=current.authority_epoch;
    if(!SameNativeCellPlayer(expected,current)||previous.role==current.role||current.authority_epoch<=previous.authority_epoch)
        throw std::runtime_error("Native cell role change differs from confirmed handoff");
    const auto position=Position(found->second.x,found->second.z);
    found->second.claim=current;
    if(auto cell=map.cells.find(position);cell!=map.cells.end())
        if(auto member=cell->second.players.find(current.char_id);member!=cell->second.players.end())member->second.claim=current;
}
void ActivateNativePlayerHost(NativeMonsterMap& map,const MapSessionClaim& claim) {
    const auto found=map.players.find(claim.char_id);
    if(found==map.players.end()||!SameNativeCellPlayer(found->second.claim,claim))
        throw std::runtime_error("Native host activation has stale player lifetime");
    auto& player=found->second;
    const auto position=Position(player.x,player.z);
    if(player.revision==std::numeric_limits<std::uint64_t>::max())throw std::runtime_error("Native player cell revision exhausted");
    ++player.revision;player.host.can_host=true;
    if(auto cell=map.cells.find(position);cell!=map.cells.end())
        if(auto member=cell->second.players.find(claim.char_id);member!=cell->second.players.end())member->second=player;
}
NativeCellNotices LeaveNativeCellPlayer(NativeMonsterMap& map,const MapSessionClaim& claim) {
    const auto found=map.players.find(claim.char_id);
    if(found==map.players.end()||!SameNativeCellPlayer(found->second.claim,claim))return {};
    const auto player=found->second;
    const auto position=Position(player.x,player.z);
    if(auto cell=map.cells.find(position);cell!=map.cells.end())cell->second.players.erase(claim.char_id);
    NativeCellNotices out;
    for(auto key:Neighbors(map,position))Leave(out,player,map.cells.at(key),true);
    map.players.erase(found);
    return out;
}
} // namespace tmapsvr
