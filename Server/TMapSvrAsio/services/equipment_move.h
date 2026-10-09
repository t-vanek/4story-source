#pragma once
#include "domain/character.h"
#include "domain/main_transfer.h"
#include <algorithm>
#include <map>
#include <stdexcept>

namespace tmapsvr {
inline InventoryMoveResult CanEquipItem(const CharSnapshot& s,const ItemInstance& item,std::uint8_t slot) {
    if(!item.source||!item.equipment)throw std::runtime_error("Equipment lacks original rules");
    const auto& rule=*item.equipment;
    if(item.source->wrap)return InventoryMoveResult::Wrapped;
    if(rule.skill_required&&!s.payload->equipment_kinds[item.bKind])return InventoryMoveResult::NoSkill;
    if(slot>=32||!(rule.slots&(std::uint32_t{1}<<slot)))return InventoryMoveResult::CannotEquip;
    if(s.bClass>=32||!(rule.classes&(std::uint32_t{1}<<s.bClass)))return InventoryMoveResult::WrongClass;
    // GetEquipLevel compares the full DWORD before subtracting a BYTE. ELD
    // greater than or equal to the requirement deliberately leaves it unchanged.
    const auto level=rule.level>item.source->eld?rule.level-static_cast<std::uint8_t>(item.source->eld):rule.level;
    return s.bLevel<level?InventoryMoveResult::LowLevel:InventoryMoveResult::Success;
}

// Pure simulation of CSHandler::OnCS_MOVEITEM_REQ and Player::PushTItem. Keep
// intermediate packets separately from the final identity diff: displacement
// may merge into the incoming stack before the incoming unit is equipped.
template<class Equal>
InventoryMovePlan PlanEquipmentMove(const CharSnapshot& s,InventoryMoveRequest request,Equal equal) {
    using Result=InventoryMoveResult;
    InventoryMovePlan plan;plan.kind=InventoryMoveKind::Equipment;
    const auto reject=[&](Result result){InventoryMovePlan p;p.kind=InventoryMoveKind::Equipment;p.result=result;return p;};
    std::map<std::uint8_t,std::map<std::uint8_t,ItemInstance>> bags;
    std::map<std::uint8_t,std::uint8_t> capacities;
    std::map<std::uint64_t,ItemInstance> originals;
    for(const auto& bag:s.payload->bags) {
        if(bag.bag.dEndTime)throw std::runtime_error("Timed equipment displacement bags are unsupported");
        capacities.emplace(bag.bag.bInvenID,bag.slot_count);
        for(const auto& item:bag.items) {
            if(!item.source||!item.equipment||!item.dlID||!originals.emplace(item.dlID,item).second)
                throw std::runtime_error("Equipment inventory identity or template invalid");
            const auto& raw=*item.source;
            if(item.durable_hash.size()!=64||!item.bCount||raw.id!=item.dlID||raw.storage||raw.owner_type||
               raw.owner_id!=s.dwCharID||raw.storage_id!=bag.bag.bInvenID||raw.slot!=item.bItemID||raw.count!=item.bCount||raw.item!=item.wItemID)
                throw std::runtime_error("Equipment inventory source is unverified");
            if(bag.bag.bInvenID==254&&item.bCount!=1)
                throw std::runtime_error("Equipped item quantity is not one");
            bags[bag.bag.bInvenID].emplace(item.bItemID,item);
        }
        bags.try_emplace(bag.bag.bInvenID);
    }
    auto sb=request.source_bag,ss=request.source_slot,db=request.destination_bag,ds=request.destination_slot;
    if(!bags.contains(sb))return reject(Result::NoSourceBag);
    if(!bags[sb].contains(ss)||!bags[sb][ss].bCount||!request.count)return reject(Result::NoSourceItem);
    if(!bags.contains(db))return reject(Result::NoDestinationBag);
    if(sb==db&&ss==ds)return reject(Result::SamePosition);
    const auto supported=[](auto bag){return bag<250||bag==254||bag==255;};
    if(!supported(sb)||!supported(db))throw std::runtime_error("Unsupported equipment storage");
    if((sb==254&&ss==18)||(db==254&&ds==18))throw std::runtime_error("Race costume change dependencies are not implemented");
    if(db==254&&ds==bags[sb][ss].equipment->secondary)ds=bags[sb][ss].equipment->primary;
    const auto valid_slot=[&](auto bag,auto slot){return bag==254?slot<19:capacities[bag]&&slot<capacities[bag];};
    if(!valid_slot(sb,ss)||!valid_slot(db,ds))throw std::runtime_error("Equipment position exceeds original slot range");
    if(db==254&&ds==1&&!bags[db].contains(ds)&&bags[db].contains(0)&&bags[db][0].equipment->secondary==1)
        return reject(Result::BothHands);
    if(bags[db].contains(ds)&&sb==254&&db!=254){std::swap(sb,db);std::swap(ss,ds);}
    if(db==254)if(const auto r=CanEquipItem(s,bags[sb][ss],ds);r!=Result::Success)return reject(r);
    if(sb==254&&bags[db].contains(ds))if(const auto r=CanEquipItem(s,bags[db][ds],ss);r!=Result::Success)return reject(r);
    // The source remaps a two-hand secondary slot before looking up the target.
    // Malformed requests which alias the source through this remap are rejected
    // instead of duplicating/freeing the same original CTItem pointer twice.
    if(sb==db&&ss==ds)return reject(Result::SamePosition);
    std::vector<std::uint8_t> displaced;
    const auto displace=[&](std::uint8_t slot){
        if(!bags[254].contains(slot))return;
        if(std::find(displaced.begin(),displaced.end(),slot)!=displaced.end()||
           (sb==254&&slot==ss))throw std::runtime_error("Overlapping equipment displacement");
        displaced.push_back(slot);
    };
    if(db==254) {
        if(bags[db].contains(ds)&&bags[sb][ss].bCount>1&&!equal(bags[sb][ss],bags[db][ds]))displace(ds);
        const auto sub=bags[sb][ss].equipment->secondary;
        if(sub!=255)displace(sub);
    }
    if(sb==254&&bags[db].contains(ds)) {
        const auto sub=bags[db][ds].equipment->secondary;
        if(sub!=255)displace(sub);
    }
    const auto emit=[&](InventoryWireKind kind,const ItemInstance& item){plan.wire.push_back({kind,item});};
    // All displaced equipment is removed before any carried UPDATE/ADD. CanPush
    // runs before the incoming source vacates its carried slot; the simulation
    // likewise cannot use that future blank when inventory is full.
    for(auto slot:displaced)emit(InventoryWireKind::Delete,bags[254].at(slot));
    std::vector<ItemInstance> removed;
    for(auto slot:displaced){removed.push_back(bags[254].at(slot));bags[254].erase(slot);}
    std::vector<std::uint8_t> carried;
    if(bags.contains(255))carried.push_back(255);
    for(const auto& [bag,items]:bags)if(bag!=254&&bag!=255){
        if(bag>=250)throw std::runtime_error("Unsupported displacement bag kind");
        carried.push_back(bag);
    }
    for(auto item:removed) {
        bool placed=false;
        for(auto bag:carried) {
            for(auto& [slot,target]:bags[bag])if(equal(item,target)&&target.stack_limit&&target.bCount<target.stack_limit) {
                ++target.bCount;emit(InventoryWireKind::Update,target);placed=true;break;
            }
            if(placed)break;
        }
        if(!placed)for(auto bag:carried) {
            for(unsigned slot=0;slot<capacities[bag];++slot)if(!bags[bag].contains(slot)) {
                item.bInvenID=bag;item.bItemID=slot;bags[bag].emplace(slot,item);
                emit(InventoryWireKind::Add,item);placed=true;break;
            }
            if(placed)break;
        }
        if(!placed)return reject(Result::InventoryFull);
    }
    auto source=bags[sb].at(ss);
    bool source_pointer_alive=true;
    auto destination_kind=source.bKind;
    std::uint64_t split_parent=0;
    if(!bags[db].contains(ds)) {
        auto destination=source;destination.bInvenID=db;destination.bItemID=ds;destination.bCount=1;
        if(source.bCount>1) {
            --bags[sb][ss].bCount;emit(InventoryWireKind::Update,bags[sb][ss]);
            destination.dlID=0;split_parent=source.dlID;
        }else {emit(InventoryWireKind::Delete,source);bags[sb].erase(ss);source_pointer_alive=false;}
        bags[db][ds]=destination;emit(InventoryWireKind::Add,destination);
    }else if(!equal(source,bags[db][ds])) {
        auto destination=bags[db][ds];source.bInvenID=db;source.bItemID=ds;destination.bInvenID=sb;destination.bItemID=ss;
        destination_kind=destination.bKind;
        bags[sb][ss]=destination;bags[db][ds]=source;
        emit(InventoryWireKind::Update,source);emit(InventoryWireKind::Update,destination);
    }else if(db!=254) {
        auto& destination=bags[db][ds];
        destination_kind=destination.bKind;
        if(!destination.stack_limit||destination.bCount>destination.stack_limit)throw std::runtime_error("Invalid equipment merge capacity");
        const auto amount=std::min<unsigned>(1,destination.stack_limit-destination.bCount);
        destination.bCount+=amount;bags[sb][ss].bCount-=amount;
        if(!bags[sb][ss].bCount){emit(InventoryWireKind::Delete,bags[sb][ss]);bags[sb].erase(ss);source_pointer_alive=false;}
        else emit(InventoryWireKind::Update,bags[sb][ss]);
        emit(InventoryWireKind::Update,destination);
    }else destination_kind=bags[db][ds].bKind;
    // Automatic warrior posture creation and active-effect cancellation must be
    // persisted together with the equipment; do not silently omit these effects.
    if(s.bClass==0&&db==254) {
        if((source_pointer_alive&&(source.bKind==3||source.bKind==5))||destination_kind==12)
            throw std::runtime_error("Equipment-triggered warrior postures require durable active effects");
    }
    std::map<std::uint64_t,ItemInstance> final;
    for(const auto& [bag,items]:bags)for(const auto& [slot,item]:items)final.emplace(item.dlID,item);
    for(const auto& [id,old]:originals) {
        const auto it=final.find(id);
        if(it==final.end())plan.items.push_back({old,old.bInvenID,old.bItemID,0});
        else if(old.bInvenID!=it->second.bInvenID||old.bItemID!=it->second.bItemID||old.bCount!=it->second.bCount)
            plan.items.push_back({old,it->second.bInvenID,it->second.bItemID,it->second.bCount});
    }
    if(split_parent) {
        const auto& created=final.at(0);
        plan.items.push_back({originals.at(split_parent),created.bInvenID,created.bItemID,created.bCount,true});
    }
    return plan;
}
}
