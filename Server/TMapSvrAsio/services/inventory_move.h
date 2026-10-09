#pragma once
#include "domain/character.h"
#include "domain/main_transfer.h"
#include <algorithm>
#include <stdexcept>

namespace tmapsvr {
inline bool IsCarriedBag(std::uint8_t id){return id<250||id==255;}

// CSHandler.cpp:1782-2183. Complete stacks retain identity (CTItem::Copy with
// bGenID=0); different templates always swap, even when count requests one unit.
// Splitting, merging, dropping and equipment have separate, unported contracts.
inline InventoryMovePlan PlanInventoryMove(const CharSnapshot& s,const InventoryMoveRequest& r) {
    if(!s.payload)throw std::runtime_error("Inventory move requires native state");
    if(s.payload->transfer_state&&!s.payload->transfer_state->new_security)
        throw std::runtime_error("Native secured inventory mutation is unsupported");
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
    if(other==dst->items.end()&&r.count<item->bCount)
        throw std::runtime_error("Native stack split is unsupported");
    if(other!=dst->items.end()&&item->wItemID==other->wItemID)
        throw std::runtime_error("Native same-template merge or swap is unsupported");
    InventoryMovePlan plan;
    plan.items.push_back({*item,r.destination_bag,r.destination_slot});
    if(other!=dst->items.end())plan.items.push_back({*other,r.source_bag,r.source_slot});
    for(const auto& move:plan.items) {
        const auto& i=move.before;
        if(!i.dlID||!i.source||i.durable_hash.size()!=64||!i.bCount||i.source->id!=i.dlID||
           i.source->storage||i.source->owner_type||i.source->owner_id!=s.dwCharID||
           i.source->storage_id!=i.bInvenID||i.source->slot!=i.bItemID||i.source->count!=i.bCount||i.source->item!=i.wItemID)
            throw std::runtime_error("Unverified inventory move source");
    }
    return plan;
}

inline void ApplyInventoryMove(CharSnapshot& s,const InventoryMovePlan& plan) {
    if(!s.payload||plan.result!=InventoryMoveResult::Success||plan.items.empty()||plan.items.size()>2)
        throw std::runtime_error("Invalid inventory move projection");
    auto p=std::make_shared<CharacterPayload>(*s.payload);
    for(const auto& move:plan.items) {
        unsigned found=0;
        for(auto& bag:p->bags)for(auto it=bag.items.begin();it!=bag.items.end();++it)if(it->dlID==move.before.dlID) {
            if(bag.bag.bInvenID!=move.before.bInvenID||it->bItemID!=move.before.bItemID||it->durable_hash!=move.before.durable_hash)
                throw std::runtime_error("Inventory move projection changed");
            ++found;bag.items.erase(it);break;
        }
        if(found!=1)throw std::runtime_error("Inventory move identity is missing or duplicated");
    }
    for(const auto& move:plan.items) {
        auto bag=std::find_if(p->bags.begin(),p->bags.end(),[&](const auto& b){return b.bag.bInvenID==move.bag;});
        if(bag==p->bags.end()||std::any_of(bag->items.begin(),bag->items.end(),[&](const auto& i){return i.bItemID==move.slot;}))
            throw std::runtime_error("Inventory move destination is occupied");
        auto item=move.before;item.bInvenID=move.bag;item.bItemID=move.slot;
        auto raw=std::make_shared<transfer::Item>(*item.source);raw->storage_id=move.bag;raw->slot=move.slot;item.source=std::move(raw);
        bag->items.push_back(std::move(item));
        std::sort(bag->items.begin(),bag->items.end(),[](const auto& a,const auto& b){return a.bItemID<b.bItemID;});
    }
    s.payload=std::move(p);
}
}
