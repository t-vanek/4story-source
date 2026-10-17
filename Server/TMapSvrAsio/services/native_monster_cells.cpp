#include "native_map_cells.h"
#include "native_monster_spawn.h"
#include <stdexcept>
#include <utility>

namespace tmapsvr {
namespace {
std::uint32_t Position(float x,float z) {
    const auto p=NativeCellAt(x,z);
    if(!p)throw std::runtime_error("Native monster cells require source WORD coordinates");
    return *p;
}
void Delete(NativeCellNotices& out,const NativeMonsterSlot& slot,const NativeMapCell& cell,bool exit_map,
            std::optional<std::uint8_t> channel={}) {
    for(const auto& [id,player]:cell.players) {
        if(channel&&(player.claim.channel==slot.channel||player.claim.channel!=*channel))continue;
        NativeCellNotice notice{NativeCellNoticeKind::DeleteMonster,player.claim,{},exit_map};
        notice.monster_id=slot.id;notice.monster_generation=slot.generation;out.push_back(std::move(notice));
    }
}
void Enter(NativeCellNotices& out,const NativeMonsterSlot& slot,const NativeMapCell& neighbor,const NativeMapCell& origin,
    const NativeMonsterCatalog& catalog,const NativeMonsterAiCatalog& ai,std::uint32_t tick,bool fresh,
    std::shared_ptr<const NativeMonsterAppearance>& appearance) {
    for(const auto& [id,player]:neighbor.players)if(origin.IsMain(player.claim.channel)) {
        if(!appearance)appearance=std::make_shared<const NativeMonsterAppearance>(ProjectNativeMonsterAppearance(slot,catalog,ai,tick));
        NativeCellNotice notice{NativeCellNoticeKind::AddMonster,player.claim,{},fresh};
        notice.monster_id=slot.id;notice.monster_generation=slot.generation;notice.monster=appearance;out.push_back(std::move(notice));
    }
}
}
NativeCellNotices EnterNativeMonster(NativeMonsterMap& map,std::uint32_t id,const NativeMonsterCatalog& catalog,
                                   const NativeMonsterAiCatalog& ai,std::uint32_t tick) {
    auto& slot=map.slots.at(id);
    if(slot.entered_map)throw NativeMonsterInitializationRequired(slot.map,slot.channel,id,"repeated EnterMAP lifecycle");
    const auto key=Position(slot.x,slot.z);
    NativeCellNotices out;
    if(auto origin=map.cells.find(key);origin!=map.cells.end()) {
        // Normal OT_MON EnterMAP ignores channel enable, unlike OnMove.
        origin->second.monsters.insert(id);slot.cell_memberships.insert(key);
        std::shared_ptr<const NativeMonsterAppearance> appearance;
        for(auto neighbor:NativeNeighborCells(map.cells,key))
            Enter(out,slot,map.cells.at(neighbor),origin->second,catalog,ai,tick,true,appearance);
    }
    slot.entered_map=true;return out;
}
NativeCellNotices LeaveNativeMonster(NativeMonsterMap& map,std::uint32_t id,bool exit_map) {
    auto& slot=map.slots.at(id);
    const auto key=Position(slot.x,slot.z);
    if(auto cell=map.cells.find(key);cell!=map.cells.end())cell->second.monsters.erase(id);
    // Source disabled-old-cell movement can retain indexes. Preserve those
    // while the object exists, then retire EVERY owned reference before erase.
    // The source's dangling pointer after destruction is not compatibility.
    for(auto old:slot.cell_memberships)if(auto cell=map.cells.find(old);cell!=map.cells.end())cell->second.monsters.erase(id);
    slot.cell_memberships.clear();
    NativeCellNotices out;
    for(auto cell:NativeNeighborCells(map.cells,key))Delete(out,slot,map.cells.at(cell),exit_map);
    slot.entered_map=false;return out; // owning map affiliation is retained
}
NativeMonsterCellMove MoveNativeMonster(NativeMonsterMap& map,std::uint32_t id,float x,float z,
    const NativeMonsterCatalog& catalog,const NativeMonsterAiCatalog& ai,const std::vector<std::uint8_t>& channels,std::uint32_t tick) {
    auto& slot=map.slots.at(id);
    const auto before=Position(slot.x,slot.z),after=Position(x,z);
    NativeMonsterCellMove out;out.changed=before!=after;
    if(!out.changed){slot.x=x;slot.z=z;return out;}
    std::set<std::uint8_t> lost;
    const auto count=static_cast<std::uint8_t>(channels.size()); // source BYTE prefix, duplicates retained in input
    const auto old=map.cells.find(before),next=map.cells.find(after);
    if(old!=map.cells.end()&&old->second.IsEnabled(slot.channel)) {
        for(unsigned i=0;i<count;++i)if(old->second.IsMain(channels[i]))lost.insert(channels[i]);
        old->second.monsters.erase(id);slot.cell_memberships.erase(before);
    }
    const auto old_cells=NativeNeighborCells(map.cells,before),new_cells=NativeNeighborCells(map.cells,after);
    const std::set<std::uint32_t> old_set(old_cells.begin(),old_cells.end()),new_set(new_cells.begin(),new_cells.end());
    for(auto cell:old_cells)if(!new_set.contains(cell))Delete(out.notices,slot,map.cells.at(cell),false);
    slot.x=x;slot.z=z;
    if(next!=map.cells.end()&&next->second.IsEnabled(slot.channel)) {
        bool enter_channel=false;
        for(unsigned i=0;i<count;++i)if(next->second.IsMain(channels[i])&&!lost.contains(channels[i])){enter_channel=true;break;}
        for(unsigned i=0;i<count;++i)if(next->second.IsMain(channels[i]))lost.erase(channels[i]);
        next->second.monsters.insert(id);slot.cell_memberships.insert(after);
        std::shared_ptr<const NativeMonsterAppearance> appearance;
        for(auto cell:new_cells) {
            // Normal OT_MON never sets the recall/companion bLeaveCH flag.
            // LeaveChannel still excludes the monster's own physical channel.
            for(auto channel:lost)Delete(out.notices,slot,map.cells.at(cell),false,channel);
            if(enter_channel||!old_set.contains(cell))
                Enter(out.notices,slot,map.cells.at(cell),next->second,catalog,ai,tick,false,appearance);
        }
    }
    return out; // caller emits one AT_LEAVE even with missing/disabled destination
}
} // namespace tmapsvr
