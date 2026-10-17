#include "native_player_host.h"
#include "native_map_cells.h"
#include "native_monster_spawn.h"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

namespace tmapsvr {
namespace {
float Distance(float x,float z,float other_x,float other_z) {
    // TMapType::GetDistance: FLOAT subtract/multiply/add, then sqrtf. Do not
    // replace with hypot, double precision, or a squared-distance predicate.
    const float dx=x-other_x,dz=z-other_z;
    return std::sqrt(dx*dx+dz*dz);
}
}
NativePlayerMovePlan PlanNativePlayerHostMove(const NativeCellPlayer& player,const CharSnapshot& snapshot,
                                            const NativePlayerMoveInput& input,std::uint32_t tick) {
    if(snapshot.dwCharID!=player.claim.char_id||snapshot.fPosX!=player.x||snapshot.fPosZ!=player.z)
        throw std::runtime_error("Native movement snapshot differs from actual source cell actor");
    if(!std::isfinite(input.x)||!std::isfinite(input.y)||!std::isfinite(input.z)||!std::isfinite(input.speed))
        throw std::runtime_error("Native movement requires finite source input");
    if(player.claim.role==MapSessionRole::Primary&&!snapshot.payload)
        throw std::runtime_error("Native primary movement requires actual HP authority");
    const auto origin=NativeCellAt(player.x,player.z);
    if(!origin)throw std::runtime_error("Native movement origin requires defined source WORD coordinates");
    NativePlayerMovePlan out;
    out.claim=player.claim;out.revision=player.revision;out.origin_cell=*origin;out.map=snapshot.wMapID;out.host=player.host;
    out.x=input.x;out.y=input.y;out.z=input.z;out.direction=input.direction;out.movement=snapshot.movement;
    if(snapshot.payload)out.host.hp_zero=snapshot.dwHP==0;
    out.movement.speed=input.speed; // source writes this even before ghost early return
    out.host.move_tick=tick; // observed0 is valid and distinct from never observed
    auto mouse=input.mouse,key=input.key,action=input.action;
    if(out.host.hp_zero)mouse=key=4; // source local m_dwHP, never unknown replica bars
    if(input.ghost) {
        out.host.ghost=input.ghost;out.host.ghost_x=input.x;out.host.ghost_z=input.z;
        const float distance=Distance(player.x,player.z,input.x,input.z);
        if(!out.host.can_host&&distance<32.0f)out.host.can_host=true;
        else if(out.host.can_host&&distance>=32.0f)out.host.can_host=false;
        // Actual source monster event loops require the separate populated-map
        // integration. The service enforces the proven source-empty map here.
        out.x=snapshot.fPosX;out.y=snapshot.fPosY;out.z=snapshot.fPosZ;
        if(snapshot.movement.action==7) {out.early_return=true;out.direction=snapshot.wDIR;return out;}
        action=7; // first ghost request makes TA_DEAD action without moving body
    } else if(out.host.ghost)out.host.ghost=0;
    // Deliberately after MoveGhost: first far ghost can set this TRUE again;
    // CanHost still uses its two independent ghost distance predicates.
    if(!out.host.can_host&&action!=0)out.host.can_host=true;
    // CS_MOVE assigns m_bAction before comparing it with bAction for the
    // SITDOWN transition. That condition cannot restart recovery clocks.
    out.movement.pitch=input.pitch;out.movement.mouse=mouse;out.movement.key=key;out.movement.action=action;
    if(!NativeCellAt(out.x,out.z))throw std::runtime_error("Native body movement requires defined source WORD coordinates");
    return out;
}
bool NativePlayerCanHost(const NativeCellPlayer& player,float x,float z) {
    if(player.host.ghost)return Distance(player.x,player.z,player.host.ghost_x,player.host.ghost_z)<32.0f&&
        Distance(player.x,player.z,x,z)<32.0f;
    return player.host.can_host;
}
namespace {
std::vector<const NativeCellPlayer*> HostNeighbors(const NativeMonsterMap& map,float x,float z) {
    const auto position=NativeCellAt(x,z);
    if(!position)throw std::runtime_error("Native host selection requires source WORD monster coordinates");
    std::vector<const NativeCellPlayer*> players; // borrowed only during this synchronous, locked turn
    for(auto cell:NativeNeighborCells(map.cells,*position))for(const auto& [id,index]:map.cells.at(cell).players) {
        const auto actor=map.players.find(id);
        if(actor==map.players.end()||!SameNativeCellPlayer(actor->second.claim,index.claim))
            throw std::runtime_error("Native host neighbor index has no matching actor lifetime");
        players.push_back(&actor->second); // no main/role/channel filter in source GetNeighbor
    }
    return players;
}
std::uint32_t HostDistance(const NativeCellPlayer& player,float x,float z) {
    const float distance=std::abs(x-player.x)+std::abs(z-player.z); // cast AFTER FLOAT Manhattan sum
    if(!std::isfinite(distance)||std::trunc(static_cast<double>(distance))>4294967295.0)
        throw std::runtime_error("Native host distance requires defined source DWORD conversion");
    return static_cast<std::uint32_t>(distance);
}
}
std::optional<MapSessionClaim> FindNativeMonsterHost(const NativeMonsterMap& map,float x,float z,std::uint32_t id) {
    const auto position=NativeCellAt(x,z);
    if(!position)throw std::runtime_error("Native FindHost requires source WORD coordinates");
    for(auto cell:NativeNeighborCells(map.cells,*position)) {
        const auto found=map.cells.at(cell).players.find(id);
        if(found==map.cells.at(cell).players.end())continue;
        const auto actor=map.players.find(id);
        if(actor==map.players.end()||!SameNativeCellPlayer(actor->second.claim,found->second.claim))
            throw std::runtime_error("Native FindHost neighbor has stale actor lifetime");
        return actor->second.claim; // source stops at the first matching cell
    }
    return {};
}
NativeMonsterHostChoice SelectNativeMonsterHost(const NativeMonsterMap& map,float x,float z,std::uint32_t tick) {
    const auto players=HostNeighbors(map,x,z);
    const NativeCellPlayer* selected=nullptr;
    std::uint32_t best=std::numeric_limits<std::uint32_t>::max();
    if(!players.empty()&&NativePlayerCanHost(*players.front(),x,z))selected=players.front();
    for(const auto* player:players) {
        if(!NativePlayerCanHost(*player,x,z))continue;
        const auto local=HostDistance(*player,x,z);
        if(local<best) {
            if(!player->host.move_tick)throw std::runtime_error("Source host recency needs an observed MOVE tick; initial value is unproven");
            const std::uint32_t elapsed=tick-*player->host.move_tick;
            if(elapsed<3000){selected=player;best=local;}
        }
    }
    if(!selected&&!players.empty()&&!players.front()->host.can_host&&!players.front()->host.ghost)
        return {players.front()->claim,true};
    return {selected?std::optional<MapSessionClaim>{selected->claim}:std::nullopt,false};
}
NativeMonsterHostChoice SelectNativeMonsterReplacementHost(const NativeMonsterMap& map,float x,float z,
                                                         const std::optional<MapSessionClaim>& previous) {
    if(previous) {
        const auto player=map.players.find(previous->char_id);
        if(player==map.players.end()||!SameNativeCellPlayer(player->second.claim,*previous))
            throw std::runtime_error("Native CHKHOST previous lifetime is stale");
        if(NativePlayerCanHost(player->second,x,z))return {previous,false,true};
    }
    const auto players=HostNeighbors(map,x,z);
    const NativeCellPlayer* selected=nullptr;
    std::uint32_t best=0; // source CHKHOST has no first-player preselection or recency gate
    for(const auto* player:players) {
        if(!NativePlayerCanHost(*player,x,z))continue;
        const auto local=HostDistance(*player,x,z);
        if(!selected||local<best){selected=player;best=local;}
    }
    if(!selected&&!players.empty()&&!players.front()->host.can_host&&!players.front()->host.ghost)
        return {players.front()->claim,true};
    return {selected?std::optional<MapSessionClaim>{selected->claim}:std::nullopt,false};
}
} // namespace tmapsvr
