#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
#include <map>

namespace tmapsvr {
// TITEMUSEDTABLE / RELEASEMAIN: WORD group, DWORD remaining milliseconds.
// The local observation and LOADCHAR exclusions are never protocol fields.
struct ItemCooldown {
    std::uint16_t group{};
    std::uint32_t remaining{};
    bool operator==(const ItemCooldown&) const = default;
};
struct ItemCooldownState {
    std::vector<ItemCooldown> timers;
    std::vector<ItemCooldown> ignored_on_load;
    std::uint64_t sampled_ms{};
    // Local absolute observations preserve ACTITEMSUSE's strict deadline < now
    // after relative remaining reaches zero. Never encoded or persisted.
    std::map<std::uint16_t,std::uint64_t> deadlines;
};
inline void OrderItemCooldowns(std::vector<ItemCooldown>& rows) {
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.group<b.group;});
    for(std::size_t i=1;i<rows.size();++i)
        if(rows[i-1].group==rows[i].group)throw std::runtime_error("Duplicate item delay group");
}
inline void SampleItemCooldowns(ItemCooldownState& state,std::uint64_t now) {
    // now=0 deliberately encodes an already sampled checkpoint, without taking
    // another observation while hashing it. Backward clocks never bypass reuse.
    if(!now||now<state.sampled_ms)return;
    const auto elapsed=now-state.sampled_ms;
    for(auto& row:state.timers) {
        state.deadlines.try_emplace(row.group,state.sampled_ms+row.remaining);
        row.remaining=elapsed>=row.remaining?0:row.remaining-static_cast<std::uint32_t>(elapsed);
    }
    state.sampled_ms=now;
}
inline std::vector<ItemCooldown> ActiveItemCooldowns(const ItemCooldownState& state) {
    auto rows=state.timers;OrderItemCooldowns(rows);
    std::erase_if(rows,[](const auto& row){return !row.remaining;});
    return rows;
}
inline std::string ItemCooldownJson(const std::vector<ItemCooldown>& rows) {
    auto sorted=rows;OrderItemCooldowns(sorted);std::string out="[";
    for(const auto& row:sorted) {
        if(out.size()>1)out+=',';
        out+='['+std::to_string(row.group)+','+std::to_string(row.remaining)+']';
    }
    return out+']';
}
inline void CheckItemCooldownProjection(const std::vector<ItemCooldown>& stored,const ItemCooldownState& live) {
    auto rows=stored;OrderItemCooldowns(rows);
    auto timers=live.timers;OrderItemCooldowns(timers);
    if(timers.size()>255)throw std::runtime_error("Item timers exceed CHARINFO count");
    for(const auto& row:rows) {
        const auto candidate=std::find_if(timers.begin(),timers.end(),[&](const auto& t){return t.group==row.group;});
        if(candidate==timers.end()) {
            if(std::find(live.ignored_on_load.begin(),live.ignored_on_load.end(),row)==live.ignored_on_load.end())
                throw std::runtime_error("Item cooldown disappeared outside source load/expiry");
        } else if(candidate->remaining>row.remaining)
            throw std::runtime_error("Core snapshot cannot extend an item cooldown");
    }
    for(const auto& row:timers)if(row.remaining&&std::none_of(rows.begin(),rows.end(),[&](const auto& t){return t.group==row.group;}))
        throw std::runtime_error("Core snapshot cannot arm an item cooldown");
}
}
