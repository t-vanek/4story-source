#pragma once
#include "domain/character.h"
#include "domain/main_transfer.h"
#include <algorithm>
#include <optional>
#include <stdexcept>
#include <tuple>

namespace tmapsvr {
// DeleteSkillItem scans ordered inventory/slot maps and consumes exactly one.
// Selection is independent of the DTO vector order. Equipped items require
// their separately ported inventory mutation and derived-stat contract.
inline std::optional<ItemInstance> FindSkillConsumable(const CharSnapshot& s,std::uint16_t item,std::uint8_t kind=0) {
    if((!item&&!kind)||!s.payload)throw std::runtime_error("Unsupported reagent character graph");
    std::optional<ItemInstance> out;
    for(const auto& bag:s.payload->bags)for(const auto& row:bag.items)if(kind?row.bKind==kind:row.wItemID==item) {
        if(row.bInvenID!=bag.bag.bInvenID)throw std::runtime_error("Reagent inventory placement disagrees");
        if(!out||std::tie(row.bInvenID,row.bItemID)<std::tie(out->bInvenID,out->bItemID))out=row;
    }
    if(out&&(out->bInvenID==254||!out->bCount||!out->source||out->durable_hash.size()!=64))
        throw std::runtime_error("Unsupported or unverified reagent item");
    return out;
}
inline std::optional<ItemInstance> FindSkillReagent(const CharSnapshot& s,std::uint16_t item) {
    return FindSkillConsumable(s,item);
}
inline std::optional<ItemInstance> FindSkillAmmunition(const CharSnapshot& s,std::uint8_t kind) {
    return FindSkillConsumable(s,0,kind);
}
inline void ConsumeReagentProjection(CharSnapshot& s,const ItemInstance& before) {
    auto p=std::make_shared<CharacterPayload>(*s.payload);
    bool found=false;
    for(auto& bag:p->bags)for(auto it=bag.items.begin();it!=bag.items.end();++it)if(it->dlID==before.dlID) {
        if(found||it->bCount!=before.bCount)throw std::runtime_error("Reagent projection changed");
        found=true;
        if(--it->bCount==0)bag.items.erase(it);
        else {
            auto raw=std::make_shared<transfer::Item>(*it->source);
            raw->count=it->bCount;it->source=std::move(raw);
        }
        break;
    }
    if(!found)throw std::runtime_error("Missing reagent projection");
    s.payload=std::move(p);
}
}
