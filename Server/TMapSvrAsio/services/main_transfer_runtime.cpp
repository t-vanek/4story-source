#include "main_transfer_runtime.h"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
namespace tmapsvr::transfer {
namespace {
auto SampleSkills(const CharSnapshot& s,const SkillCooldownTracker& timers,std::uint64_t now) {
    auto skills=s.payload->skills;const auto character=s.dwCharID;
    for(auto& skill:skills)skill.dwRemainTick=0;
    for(const auto& [id,remaining]:timers.Snapshot(character,now)){
        auto skill=std::find_if(skills.begin(),skills.end(),[id](const auto& row){return row.wSkillID==id;});
        if(skill==skills.end())throw std::runtime_error("Active cooldown has no learned skill");
        skill->dwRemainTick=remaining;
    }
    return skills;
}
}

CharSnapshot PersistenceSnapshot(const CharSnapshot& s,std::uint32_t key,const SkillCooldownTracker& timers,std::uint64_t now) {
    if(!s.payload)return s;
    auto out=s;auto p=std::make_shared<CharacterPayload>(*s.payload);
    if(p->transfer_state) {
        auto graph=std::make_shared<State>(Capture(s,key,timers,now));
        p->skills=graph->skills;p->transfer_state=std::move(graph);p->transfer_received_ms=now;
    }else p->skills=SampleSkills(s,timers,now);
    out.payload=std::move(p);return out;
}
State Capture(const CharSnapshot& s,std::uint32_t key,const SkillCooldownTracker& timers,std::uint64_t now) {
    if(!s.payload||!s.dwCharID||!key)throw std::invalid_argument("Transfer requires a hydrated native character");
    const auto& p=*s.payload;
    State out=p.transfer_state?*p.transfer_state:State{};
    if(!p.transfer_state)out.new_security=1; // source has no secure-code object
    out.character=s;out.character.payload.reset();out.key=key;out.db_load=0;out.result=0;out.login=0;
    out.aid_country=p.aid_country;out.lucky=p.lucky_number;out.rank_points=p.rank_point;
    if(!p.transfer_state) {
        if(p.posts.size()>std::numeric_limits<std::uint16_t>::max())throw std::length_error("Transfer mail count overflow");
        out.post_total=static_cast<std::uint16_t>(p.posts.size());
        out.post_read=static_cast<std::uint16_t>(std::count_if(p.posts.begin(),p.posts.end(),[](const auto& post){return post.read!=0;}));
    }
    // Extra storage kinds remain in the complete typed graph. Inventory entries
    // come from live payload values, with raw magic/full extensions retained.
    out.bags.clear();std::erase_if(out.items,[](const auto& item){return item.storage==0;});
    for(const auto& bag:p.bags){
        out.bags.push_back(bag.bag);
        for(const auto& item:bag.items){
            if(!item.source)throw std::runtime_error("Cannot reconstruct raw item from client projection");
            auto raw=*item.source;
            raw.id=item.dlID;raw.storage=0;raw.storage_id=bag.bag.bInvenID;raw.owner_type=0;raw.owner_id=s.dwCharID;
            raw.slot=item.bItemID;raw.item=item.wItemID;raw.level=item.bLevel;raw.gem=item.bGem;
            raw.appearance=item.wMoggItemID;raw.count=item.bCount;raw.grade=item.bGLevel;
            raw.durability_max=item.dwDuraMax;raw.durability=item.dwDuraCur;raw.refine=item.bRefineCur;
            raw.expires=item.dEndTime;raw.grade_effect=item.bGradeEffect;
            // These attributes have a narrowed client representation. Preserve
            // upper bits while incorporating a supported lower-bit mutation.
            raw.eld=(raw.eld&0xffffff00U)|item.bELD;raw.wrap=(raw.wrap&0xffffff00U)|item.bWrap;
            raw.color=(raw.color&0xffff0000U)|item.wColor;raw.companion=(raw.companion&0xffff0000U)|item.wCompanion;
            raw.texture=(raw.texture&0xffff0000U)|item.wCustomTex;raw.guild=item.dwGuildBound;
            out.items.push_back(std::move(raw));
        }
    }
    out.cabinets=p.cabinets;out.skills=p.skills;out.titles=p.titles;out.recalls=p.recalls;out.pets=p.pets;
    if(p.effects)out.buffs=*p.effects;
    // Preserve dirty-save flags absent from the client hotkey projection.
    auto old_hotkeys=std::move(out.hotkeys);out.hotkeys.clear();
    for(const auto& h:p.hotkeys){
        auto old=std::find_if(old_hotkeys.begin(),old_hotkeys.end(),[&](const auto& row){return row.row.inventory==h.inventory;});
        out.hotkeys.push_back({h,old==old_hotkeys.end()?std::uint8_t(0):old->save});
    }
    out.skills=SampleSkills(s,timers,now);
    // Typed extra state stays intact. Runtime integration of effect/quest/recall
    // timers must update it at their authoritative simulation tick; these fields
    // are not arbitrarily interpreted as absolute clock values here.
    return out;
}
}
