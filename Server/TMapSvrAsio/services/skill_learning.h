#pragma once
#include "domain/character.h"
#include "wire_codec.h"
#include "character_titles.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>

namespace tmapsvr {
struct SkillPurchaseRequest {std::uint16_t npc{},skill{};};
inline SkillPurchaseRequest DecodeSkillPurchase(std::span<const std::byte> bytes) {
    SkillPurchaseRequest v;wire::Reader r(bytes.data(),bytes.size());
    if(!r.Read(v.npc)||!r.Read(v.skill)||!r.Eof())throw std::runtime_error("Invalid SKILLBUY request length");
    return v;
}
struct SkillPurchaseCommit {
    std::shared_ptr<const CharSnapshot> snapshot;
    std::vector<std::pair<std::uint16_t,std::vector<std::byte>>> packets;
};
struct LearningPoint {std::uint8_t cost{},group{},parent{};};
struct LearningSkill {
    std::uint16_t id{},parent{};
    std::uint8_t start{},next{},maximum{},kind{};
    std::uint32_t classes{};
    float price{};
};
struct LearningCatalog {
    bool npc_exists{},can_talk{},offered{};
    std::uint8_t discount{};
    std::optional<LearningSkill> skill;
    std::map<std::pair<std::uint16_t,std::uint8_t>,LearningPoint> points;
    std::map<std::uint8_t,std::uint32_t> money;
    std::map<std::uint16_t,std::uint32_t> gold_titles;
};
struct LearningPlan {
    CharSnapshot after;
    std::uint8_t result{1},rank{};
    std::uint32_t price{},spent{};
    std::vector<std::vector<std::byte>> titles;
};
inline std::vector<std::byte> EncodeSkillPurchaseAck(const SkillPurchaseRequest& req,const LearningPlan& plan) {
    std::vector<std::byte> out;const auto add=[&](auto v){wire::WritePOD(out,v);};
    add(plan.result);add(req.skill);add(plan.rank);add(std::uint32_t{0});
    add(plan.after.dwGold);add(plan.after.dwSilver);add(plan.after.dwCooper);add(plan.after.wSkillPoint);
    for(auto p:plan.after.payload->skill_points)add(p);
    return out;
}
// CTPlayer::GetTitle(GOLD_TITLE, gold, TRUE), including its early return if
// ANY qualifying title is already owned. Do not replace it with threshold max.
inline void LearningGoldTitles(LearningPlan& plan,const LearningCatalog& catalog) {
    auto p=std::make_shared<CharacterPayload>(*plan.after.payload);plan.after.payload=p;
    std::vector<std::uint16_t> eligible;
    for(const auto& [id,threshold]:catalog.gold_titles)if(threshold<plan.after.dwGold)eligible.push_back(id);
    // GetTitle promotes the live ID before the owned-title early return. The
    // per-title flags remain separate; original reload/transfer re-derives it.
    if(eligible.size()>1&&eligible[eligible.size()-2]==p->selected_title)p->selected_title=eligible.back();
    for(auto id:eligible)if(std::any_of(p->titles.begin(),p->titles.end(),[&](const auto& t){return t.id==id;}))return;
    const bool selected=catalog.gold_titles.contains(p->selected_title);
    std::erase_if(p->titles,[&](const auto& t){return catalog.gold_titles.contains(t.id);});
    for(auto id:eligible) {
        p->titles.push_back({id,selected});
        std::sort(p->titles.begin(),p->titles.end(),[](const auto& a,const auto& b){return a.id<b.id;});
        if(p->titles.size()>255)throw std::runtime_error("Title count exceeds original wire width");
        std::vector<std::byte> packet;wire::WritePOD(packet,static_cast<std::uint8_t>(p->titles.size()));
        for(const auto& t:p->titles){wire::WritePOD(packet,t.id);wire::WritePOD(packet,static_cast<std::uint8_t>(t.selected));}
        wire::WritePOD(packet,id);wire::WritePOD(packet,std::uint8_t{1});plan.titles.push_back(std::move(packet));
    }
}
inline LearningPlan PlanSkillPurchase(const SkillPurchaseRequest& req,const CharSnapshot& before,const LearningCatalog& catalog) {
    if(!before.payload)throw std::runtime_error("Learning requires hydrated character");
    LearningPlan p;p.after=before;
    if(!catalog.npc_exists||!catalog.can_talk)return p;
    const auto learned=std::find_if(before.payload->skills.begin(),before.payload->skills.end(),[&](const auto& s){return s.wSkillID==req.skill;});
    const bool upgrade=learned!=before.payload->skills.end();
    if(!catalog.skill||(!upgrade&&!catalog.offered))return p;
    const auto& def=*catalog.skill;
    const std::uint8_t old=upgrade?learned->bLevel:0,rank=static_cast<std::uint8_t>(old+1);
    const std::uint8_t level=static_cast<std::uint8_t>(def.start+old*def.next);
    const auto money=catalog.money.find(level);
    if(upgrade&&(old==def.maximum||money==catalog.money.end())){p.result=2;p.rank=old;return p;}
    if(money==catalog.money.end())throw std::runtime_error("New skill has no original level price");
    const float price=static_cast<float>(money->second)*def.price;
    if(!std::isfinite(price)||price<0||static_cast<double>(price)>=4294967296.0)
        throw std::runtime_error("Source skill price is not representable as DWORD");
    p.price=static_cast<std::uint32_t>(price);
    p.price-=static_cast<std::uint32_t>(p.price*catalog.discount)/100; // original DWORD multiplication
    const auto point=catalog.points.find({req.skill,rank});
    const LearningPoint need=point==catalog.points.end()?LearningPoint{}:point->second;
    const auto parent_ok=[&]{return !def.parent||(point!=catalog.points.end()&&std::any_of(before.payload->skills.begin(),before.payload->skills.end(),
        [&](const auto& s){return s.wSkillID==def.parent&&s.bLevel>=need.parent;}));};
    const auto points_ok=[&]{
        std::uint16_t used=0;
        for(const auto& s:before.payload->skills) {
            const auto t=std::find_if(before.payload->skill_templates.begin(),before.payload->skill_templates.end(),[&](const auto& t){return t.wID==s.wSkillID;});
            if(t==before.payload->skill_templates.end())throw std::runtime_error("Learned skill lacks source kind");
            if(t->bKind!=def.kind)continue;
            for(unsigned n=1;n<=s.bLevel;++n){const auto v=catalog.points.find({s.wSkillID,static_cast<std::uint8_t>(n)});if(v!=catalog.points.end())used=static_cast<std::uint16_t>(used+v->second.cost);}
        }
        return before.wSkillPoint>=need.cost&&used>=need.group;
    };
    const auto use_money=[&](bool spend){
        if(!p.price)return true; // original UseMoney(0) never invokes GetTitle
        constexpr std::uint32_t base=1000;
        const std::uint64_t total=static_cast<std::uint32_t>(p.after.dwCooper+p.after.dwSilver*base)+static_cast<std::uint64_t>(p.after.dwGold)*base*base;
        if(total<p.price)return false;
        if(spend){const auto left=total-p.price;p.after.dwCooper=left%base;p.after.dwSilver=left/base%base;p.after.dwGold=left/base/base;p.spent=p.price;}
        LearningGoldTitles(p,catalog);return true;
    };
    if(before.bLevel<level){p.result=5;p.rank=old;return p;}
    if(upgrade) {
        if(!use_money(false)){p.result=4;p.rank=old;return p;}
        if(!points_ok()){p.result=12;return p;}
        if(!parent_ok()){p.result=3;return p;}
    }else {
        if(!parent_ok()){p.result=3;return p;}
        if(before.bClass>=32||!(def.classes&(std::uint32_t{1}<<before.bClass))){p.result=11;return p;}
        if(!points_ok()){p.result=12;return p;}
        if(!use_money(false)){p.result=4;return p;}
    }
    use_money(true);p.after.wSkillPoint=static_cast<std::uint16_t>(p.after.wSkillPoint-need.cost);
    auto payload=std::make_shared<CharacterPayload>(*p.after.payload);p.after.payload=payload;
    if(upgrade)for(auto& s:payload->skills){if(s.wSkillID==req.skill)s.bLevel=rank;}
    else payload->skills.push_back({req.skill,1,0});
    std::sort(payload->skills.begin(),payload->skills.end(),[](const auto& a,const auto& b){return a.wSkillID<b.wSkillID;});
    p.result=0;p.rank=rank;return p;
}
}
