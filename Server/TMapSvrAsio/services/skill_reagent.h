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
// UseSkillItem accumulates BYTE counts separately in each ordered inventory.
// A later stack may use another template of the same ammunition kind.
inline std::vector<SkillItemDebit> SelectSkillAmmunition(std::vector<ItemInstance> items,
    std::uint8_t kind,std::uint8_t need) {
    if(!kind||!need||need>16)throw std::runtime_error("Unsupported ammunition hit count");
    std::erase_if(items,[&](const auto& i){return i.bKind!=kind;});
    std::sort(items.begin(),items.end(),[](const auto& a,const auto& b){
        return std::tie(a.bInvenID,a.bItemID)<std::tie(b.bInvenID,b.bItemID);
    });
    for(std::size_t first=0;first<items.size();) {
        auto end=first;std::uint8_t total=0;
        while(end<items.size()&&items[end].bInvenID==items[first].bInvenID) {
            if(!items[end].bCount)throw std::runtime_error("Unsupported zero ammunition stack");
            total=static_cast<std::uint8_t>(total+items[end++].bCount);
            if(total>=need) {
                if(items[first].bInvenID==254)throw std::runtime_error("Equipped ammunition mutation unsupported");
                std::vector<SkillItemDebit> result;
                for(auto i=first;i<end&&need;++i) {
                    const auto count=std::min(need,items[i].bCount);
                    result.push_back({items[i],count});need-=count;
                }
                return result;
            }
        }
        first=end;
    }
    return {};
}
inline std::vector<SkillItemDebit> FindSkillAmmunition(const CharSnapshot& s,std::uint8_t kind,std::uint8_t hits) {
    if(!s.payload)throw std::runtime_error("Missing ammunition character graph");
    std::vector<ItemInstance> items;
    for(const auto& bag:s.payload->bags)for(const auto& item:bag.items) {
        if(item.bInvenID!=bag.bag.bInvenID)throw std::runtime_error("Ammunition placement disagrees");
        items.push_back(item);
    }
    auto out=SelectSkillAmmunition(std::move(items),kind,hits);
    for(const auto& debit:out)if(!debit.before.source||debit.before.durable_hash.size()!=64)
        throw std::runtime_error("Unverified ammunition stack");
    return out;
}
inline void ConsumeSkillItemProjection(CharSnapshot& s,const std::vector<SkillItemDebit>& debits) {
    if(!s.payload)throw std::runtime_error("Missing consumption projection");
    auto p=std::make_shared<CharacterPayload>(*s.payload);
    std::vector<std::uint64_t> seen;
    for(const auto& debit:debits) {
        const auto& before=debit.before;
        if(!debit.count||debit.count>before.bCount||std::find(seen.begin(),seen.end(),before.dlID)!=seen.end())
            throw std::runtime_error("Invalid or duplicate stack debit");
        seen.push_back(before.dlID);bool found=false;
        for(auto& bag:p->bags)for(auto it=bag.items.begin();it!=bag.items.end();++it)if(it->dlID==before.dlID) {
            if(found||it->bCount!=before.bCount||it->bInvenID!=before.bInvenID||it->bItemID!=before.bItemID||
               it->wItemID!=before.wItemID||!it->source)throw std::runtime_error("Consumption projection changed");
            found=true;it->bCount-=debit.count;
            if(!it->bCount)bag.items.erase(it);
            else {
                auto raw=std::make_shared<transfer::Item>(*it->source);
                raw->count=it->bCount;it->source=std::move(raw);
            }
            break;
        }
        if(!found)throw std::runtime_error("Missing consumption projection");
    }
    s.payload=std::move(p);
}
inline void ConsumeReagentProjection(CharSnapshot& s,const ItemInstance& before) {
    ConsumeSkillItemProjection(s,{{before,1}});
}
}
