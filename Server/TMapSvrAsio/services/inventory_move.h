#pragma once
#include "domain/character.h"
#include "domain/main_transfer.h"
#include "equipment_move.h"
#include <algorithm>
#include <stdexcept>

namespace tmapsvr {
inline bool IsCarriedBag(std::uint8_t id){return id<250||id==255;}

// CSHandler.cpp:1782-2183. Complete stacks retain identity (CTItem::Copy with
// bGenID=0); different templates always swap, even when count requests one unit.
// CTItem::operator== compares raw values, excluding count, identity and grade
// effect. For reconstructed carried items, equal template/level/gem also select
// the same SetItemAttr pointer from the pinned catalog (TMapSvr.cpp:6833).
inline bool StackEquivalent(const ItemInstance& a,const ItemInstance& b) {
    if(!a.source||!b.source)throw std::runtime_error("Stack equality requires original item values");
    const auto& x=*a.source;const auto& y=*b.source;
    if(x.item!=y.item||x.level!=y.level||x.gem!=y.gem||x.appearance!=y.appearance||x.grade!=y.grade||
       x.durability_max!=y.durability_max||x.durability!=y.durability||x.refine!=y.refine||x.expires!=y.expires||
       x.companion!=y.companion||x.eld!=y.eld||x.wrap!=y.wrap||x.color!=y.color||x.guild!=y.guild||x.texture!=y.texture||
       x.magic.size()!=y.magic.size())return false;
    for(const auto& magic:x.magic)if(std::none_of(y.magic.begin(),y.magic.end(),[&](const auto& other){
        return magic.id==other.id&&magic.value==other.value;
    }))return false;
    return true;
}
inline InventoryMovePlan PlanInventoryMove(const CharSnapshot& s,const InventoryMoveRequest& r) {
    if(!s.payload)throw std::runtime_error("Inventory move requires native state");
    if(s.payload->transfer_state&&!s.payload->transfer_state->new_security)
        throw std::runtime_error("Native secured inventory mutation is unsupported");
    if(r.source_bag==254||r.destination_bag==254)return PlanEquipmentMove(s,r,StackEquivalent);
    const auto& bags=s.payload->bags;
    const auto src=std::find_if(bags.begin(),bags.end(),[&](const auto& b){return b.bag.bInvenID==r.source_bag;});
    if(src==bags.end())return {InventoryMoveResult::NoSourceBag,{}};
    const auto item=std::find_if(src->items.begin(),src->items.end(),[&](const auto& i){return i.bItemID==r.source_slot;});
    if(item==src->items.end()||!item->bCount||!r.count)return {InventoryMoveResult::NoSourceItem,{}};
    if(r.destination_bag==252)throw std::runtime_error("Native item drop is unsupported");
    const auto dst=std::find_if(bags.begin(),bags.end(),[&](const auto& b){return b.bag.bInvenID==r.destination_bag;});
    if(dst==bags.end())return {InventoryMoveResult::NoDestinationBag,{}};
    if(src==dst&&r.source_slot==r.destination_slot)return {InventoryMoveResult::SamePosition,{}};
    if(!IsCarriedBag(r.source_bag)||!IsCarriedBag(r.destination_bag)||src->bag.dEndTime||dst->bag.dEndTime)
        throw std::runtime_error("Native equipment or timed bag move is unsupported");
    // Original FindTItem accepts arbitrary BYTE positions. Refuse positions
    // outside the recovered bag capacity instead of creating invisible items.
    if(!src->slot_count||!dst->slot_count||r.source_slot>=src->slot_count||r.destination_slot>=dst->slot_count)
        throw std::runtime_error("Inventory position exceeds pinned bag capacity");
    const auto other=std::find_if(dst->items.begin(),dst->items.end(),[&](const auto& i){return i.bItemID==r.destination_slot;});
    InventoryMovePlan plan;
    const auto count=std::min(r.count,item->bCount);
    if(other==dst->items.end()&&count<item->bCount) {
        plan.kind=InventoryMoveKind::Split;
        plan.items={{*item,r.source_bag,r.source_slot,static_cast<std::uint8_t>(item->bCount-count)},
                    {*item,r.destination_bag,r.destination_slot,count,true}};
    }else if(other!=dst->items.end()&&StackEquivalent(*item,*other)) {
        if(!other->stack_limit||other->bCount>other->stack_limit)
            throw std::runtime_error("Merge destination exceeds pinned stack capacity");
        const auto amount=std::min<unsigned>(count,other->stack_limit-other->bCount);
        plan.kind=InventoryMoveKind::Merge;
        plan.items={{*item,r.source_bag,r.source_slot,static_cast<std::uint8_t>(item->bCount-amount)},
                    {*other,r.destination_bag,r.destination_slot,static_cast<std::uint8_t>(other->bCount+amount)}};
    }else {
        plan.items.push_back({*item,r.destination_bag,r.destination_slot,item->bCount});
        if(other!=dst->items.end()) {
            plan.kind=InventoryMoveKind::Swap;
            plan.items.push_back({*other,r.source_bag,r.source_slot,other->bCount});
        }
    }
    for(const auto& move:plan.items) {
        const auto& i=move.before;
        if(!i.dlID||!i.source||i.durable_hash.size()!=64||!i.bCount||i.source->id!=i.dlID||
           i.source->storage||i.source->owner_type||i.source->owner_id!=s.dwCharID||
           i.source->storage_id!=i.bInvenID||i.source->slot!=i.bItemID||i.source->count!=i.bCount||i.source->item!=i.wItemID)
            throw std::runtime_error("Unverified inventory move source");
    }
    return plan;
}

inline void ApplyInventoryMove(CharSnapshot& s,const InventoryMovePlan& plan,std::uint64_t created_id=0) {
    if(!s.payload||plan.result!=InventoryMoveResult::Success||
       (plan.kind!=InventoryMoveKind::Equipment&&(plan.items.empty()||plan.items.size()>2)))
        throw std::runtime_error("Invalid inventory move projection");
    auto p=std::make_shared<CharacterPayload>(*s.payload);
    if(created_id&&std::any_of(p->bags.begin(),p->bags.end(),[&](const auto& b){
        return std::any_of(b.items.begin(),b.items.end(),[&](const auto& i){return i.dlID==created_id;});
    }))throw std::runtime_error("Allocated split identity already exists");
    for(const auto& move:plan.items) {
        if(move.created)continue;
        unsigned found=0;
        for(auto& bag:p->bags)for(auto it=bag.items.begin();it!=bag.items.end();++it)if(it->dlID==move.before.dlID) {
            if(bag.bag.bInvenID!=move.before.bInvenID||it->bItemID!=move.before.bItemID||it->durable_hash!=move.before.durable_hash)
                throw std::runtime_error("Inventory move projection changed");
            ++found;bag.items.erase(it);break;
        }
        if(found!=1)throw std::runtime_error("Inventory move identity is missing or duplicated");
    }
    for(const auto& move:plan.items) {
        if(!move.count)continue;
        auto bag=std::find_if(p->bags.begin(),p->bags.end(),[&](const auto& b){return b.bag.bInvenID==move.bag;});
        if(bag==p->bags.end()||std::any_of(bag->items.begin(),bag->items.end(),[&](const auto& i){return i.bItemID==move.slot;}))
            throw std::runtime_error("Inventory move destination is occupied");
        auto item=move.before;item.bInvenID=move.bag;item.bItemID=move.slot;item.bCount=move.count;
        if(move.created){item.dlID=created_id;item.durable_hash.clear();}
        auto raw=std::make_shared<transfer::Item>(*item.source);
        raw->storage_id=move.bag;raw->slot=move.slot;raw->id=item.dlID;raw->count=item.bCount;item.source=std::move(raw);
        bag->items.push_back(std::move(item));
        std::sort(bag->items.begin(),bag->items.end(),[](const auto& a,const auto& b){return a.bItemID<b.bItemID;});
    }
    s.payload=std::move(p);
}
inline void PublishInventoryMove(CharSnapshot& s,const InventoryMovePlan& plan,const InventoryMoveCommit& commit) {
    if(commit.hashes.size()!=plan.items.size()||
       (std::any_of(plan.items.begin(),plan.items.end(),[](const auto& m){return m.created;})!=bool(commit.created_id)))
        throw std::runtime_error("Incomplete inventory commit receipt");
    ApplyInventoryMove(s,plan,commit.created_id);
    auto p=std::make_shared<CharacterPayload>(*s.payload);
    for(std::size_t n=0;n<plan.items.size();++n) {
        const auto& move=plan.items[n];
        if(!move.count){if(!commit.hashes[n].empty())throw std::runtime_error("Deleted item has live fingerprint");continue;}
        if(commit.hashes[n].size()!=64)throw std::runtime_error("Missing committed item fingerprint");
        const auto id=move.created?commit.created_id:move.before.dlID;
        for(auto& bag:p->bags)for(auto& item:bag.items)if(item.dlID==id)item.durable_hash=commit.hashes[n];
    }
    s.payload=std::move(p);
}
}
