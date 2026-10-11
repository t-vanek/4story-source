#include "services/skill_learning.h"
#include <iostream>
using namespace tmapsvr;
int main(){
    unsigned checks=0;
    const auto check=[&](bool v){if(!v)throw std::runtime_error("learning assertion "+std::to_string(checks+1));++checks;};
    try {
        CharSnapshot s;s.bClass=0;s.bLevel=60;s.dwCooper=200;s.wSkillPoint=4;
        auto payload=std::make_shared<CharacterPayload>();s.payload=payload;
        LearningCatalog c;c.npc_exists=c.can_talk=c.offered=true;
        c.skill=LearningSkill{101,0,8,12,3,1,1,1.0275000333786000f};c.money={{8,100},{20,110},{32,120}};
        c.points[std::make_pair(101,1)]={1,0,1};c.points[std::make_pair(101,2)]={2,1,1};
        const SkillPurchaseRequest req{22047,101};
        auto p=PlanSkillPurchase(req,s,c);check(p.result==0&&p.rank==1&&p.price==102&&p.after.dwCooper==98&&p.after.wSkillPoint==3);
        check(p.after.payload->skills.size()==1&&s.payload->skills.empty());
        check(EncodeSkillPurchaseAck(req,p).size()==30);
        c.offered=false;check(PlanSkillPurchase(req,s,c).result==1);c.offered=true;
        s.bLevel=1;s.wSkillPoint=0;check(PlanSkillPurchase(req,s,c).result==5);s.bLevel=60;
        c.skill->parent=100;check(PlanSkillPurchase(req,s,c).result==3);c.skill->parent=0;
        s.bClass=1;check(PlanSkillPurchase(req,s,c).result==11);s.bClass=0;
        check(PlanSkillPurchase(req,s,c).result==12);s.wSkillPoint=4;s.dwCooper=0;
        check(PlanSkillPurchase(req,s,c).result==4);s.dwCooper=200;
        payload->skills={{101,1,12345}};SkillTemplate def;def.wID=101;def.bKind=1;payload->skill_templates={def};
        c.offered=false;c.skill->classes=0;
        p=PlanSkillPurchase(req,s,c);check(p.result==0&&p.rank==2&&p.after.payload->skills[0].dwRemainTick==12345);
        s.dwCooper=0;s.wSkillPoint=0;c.skill->parent=100;p=PlanSkillPurchase(req,s,c);check(p.result==4&&p.rank==1);
        s.dwCooper=200;p=PlanSkillPurchase(req,s,c);check(p.result==12&&p.rank==0);
        s.wSkillPoint=4;p=PlanSkillPurchase(req,s,c);check(p.result==3&&p.rank==0);c.skill->parent=0;
        c.money.erase(20);p=PlanSkillPurchase(req,s,c);check(p.result==2&&p.rank==1);c.money[20]=110;
        c.skill->maximum=1;p=PlanSkillPurchase(req,s,c);check(p.result==2&&p.rank==1);c.skill->maximum=3;
        c.points.erase({101,2});p=PlanSkillPurchase(req,s,c);check(p.result==0&&p.after.wSkillPoint==s.wSkillPoint);
        c.skill->parent=100;check(PlanSkillPurchase(req,s,c).result==3);c.skill->parent=0;
        c.gold_titles={{84,0},{85,3},{86,5},{87,10}};s.dwGold=11;
        p=PlanSkillPurchase(req,s,c);check(p.titles.size()==4&&p.after.payload->titles.size()==4);
        check(p.titles[0]==std::vector<std::byte>({std::byte{1},std::byte{84},std::byte{0},std::byte{0},std::byte{84},std::byte{0},std::byte{1}}));
        payload->titles={{84,false}};p=PlanSkillPurchase(req,s,c);check(p.titles.empty()&&p.after.payload->titles.size()==1);
        // Upgrade checks money (and title side effects) before failing points.
        payload->titles.clear();c.points[{101,2}]={2,1,1};s.wSkillPoint=0;
        p=PlanSkillPurchase(req,s,c);check(p.result==12&&p.spent==0&&p.titles.size()==4&&p.after.dwGold==11);
        c.skill->price=0;p=PlanSkillPurchase(req,s,c);check(p.titles.empty());
        std::vector<std::byte> bytes{std::byte{0x1f},std::byte{0x56},std::byte{101},std::byte{0}};
        const auto decoded=DecodeSkillPurchase(bytes);check(decoded.npc==22047&&decoded.skill==101);
        bytes.push_back(std::byte{});bool rejected=false;try{DecodeSkillPurchase(bytes);}catch(...){rejected=true;}check(rejected);
        std::cout<<"PASS "<<checks<<" source learning arithmetic, gate ordering, titles and wire checks\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
