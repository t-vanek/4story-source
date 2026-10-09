#pragma once
#include "services/skill_reagent.h"

void VerifyNativeReagent(soci::session& admin,tmapsvr::PostgreSQLMapService& map,
                        const tmapsvr::MapSessionClaim& c,bool recover) {
    using namespace tmapsvr;
    const auto cid=std::to_string(c.char_id);
    admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"wItemID\"=8412,\"bCount\"=2,\"dwDuraMax\"=0,\"dwDuraCur\"=0 WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=255 AND \"bItemID\"=1";
    admin<<"INSERT INTO app_world.\"TSKILLTABLE\" VALUES(1,"+cid+",36,1,0) ON CONFLICT DO NOTHING";
    Check(map.ClaimSession(c,*map.LookupSession(c.user_id,c.key)).has_value(),"reagent fixture claims native primary");
    auto s=*map.LoadAuthorized(c);map.MarkReady(c,s);
    const auto selected=FindSkillReagent(s,8412);
    Check(selected.has_value(),"reagent fixture changes an existing synthetic-owned starter slot");
    const auto before=*selected;
    Check(before.durable_hash.size()==64&&before.bInvenID==255&&before.bCount==2,"native reagent retains full-row fingerprint and source default bag");
    auto after=s;--after.dwMP;ConsumeReagentProjection(after,before);
    auto payload=std::make_shared<CharacterPayload>(*after.payload);
    for(auto& skill:payload->skills)if(skill.wSkillID==36)skill.dwRemainTick=28000;
    after.payload=payload;
    const auto item_where=" WHERE \"bWorldID\"=1 AND \"dlID\"="+std::to_string(before.dlID);
    if(!recover) {
        auto stale=c;stale.connection_id++;
        Check(Throws([&]{map.ConsumeSkillItem(stale,36,before,after);}),"stale generation cannot consume reagent");
        stale=c;stale.role=MapSessionRole::Replica;
        Check(Throws([&]{map.ConsumeSkillItem(stale,36,before,after);}),"replica cannot consume primary inventory");
        auto drift=before;drift.durable_hash=std::string(64,'0');
        Check(Throws([&]{map.ConsumeSkillItem(c,36,drift,after);}),"changed item fingerprint refuses consumption before mutation");
        Check(Throws([&]{map.ConsumeSkillItem(c,31,before,after);}),"reagent transaction validates immutable skill requirement");
        auto unlearned=after;auto unlearned_payload=std::make_shared<CharacterPayload>(*after.payload);
        std::erase_if(unlearned_payload->skills,[](const auto& row){return row.wSkillID==36;});unlearned.payload=unlearned_payload;
        Check(Throws([&]{map.ConsumeSkillItem(c,36,before,unlearned);}),"reagent transaction rejects unlearned skill");
        admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"dwDuraCur\"=1"+item_where;
        Check(Throws([&]{map.ConsumeSkillItem(c,36,before,after);}),"full-row drift rejects matching id template placement and count");
        admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"dwDuraCur\"=0"+item_where;
        admin<<"CREATE TRIGGER synthetic_consumption_fault BEFORE INSERT ON app_world.skill_item_consumptions FOR EACH ROW EXECUTE FUNCTION public.reject_checkpoint()";
        Check(Throws([&]{map.ConsumeSkillItem(c,36,before,after);}),"late consumption receipt failure rolls transaction back");
        admin<<"DROP TRIGGER synthetic_consumption_fault ON app_world.skill_item_consumptions";
        Check(Number(admin,"SELECT \"bCount\" FROM app_world.\"TITEMTABLE\""+item_where)==2&&
              Number(admin,"SELECT \"dwMP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwCharID\"="+cid)==s.dwMP&&
              Number(admin,"SELECT \"dwRemainTick\" FROM app_world.\"TSKILLTABLE\" WHERE \"dwCharID\"="+cid+" AND \"wSkillID\"=36")==0,
              "failed reagent write preserves count resources and timers atomically");
    }
    const auto hash=map.ConsumeSkillItem(c,36,before,after);
    auto p=std::make_shared<CharacterPayload>(*after.payload);
    for(auto& bag:p->bags)for(auto& item:bag.items)if(item.dlID==before.dlID)item.durable_hash=hash;
    after.payload=p;
    Check(hash.size()==64&&hash!=before.durable_hash&&Number(admin,"SELECT \"bCount\" FROM app_world.\"TITEMTABLE\""+item_where)==1,
          "confirmed reagent decrement returns new exact durable fingerprint");
    Check(Number(admin,"SELECT count(*) FROM app_world.skill_item_consumptions WHERE char_id="+cid+" AND before_count=2 AND after_count=1")==1&&
          Number(admin,"SELECT count(*) FROM app_world.map_checkpoints WHERE char_id="+cid+" AND revision=0 AND recovery_contract=3 AND app_world.map_checkpoint_matches(map_checkpoints)")==1,
          "item audit core and skill recovery receipt commit before periodic revision advances");
    if(recover)return;
    Check(Throws([&]{map.ConsumeSkillItem(c,36,before,after);}),"replaying consumed item snapshot cannot charge twice");
    map.CheckpointAuthorized(c,after,1);
    Check(Number(admin,"SELECT revision FROM app_world.map_checkpoints WHERE char_id="+cid)==1,"periodic revision continues after immediate consumption");
    const auto last=*FindSkillReagent(after,8412);ConsumeReagentProjection(after,last);
    map.ConsumeSkillItem(c,36,last,after);
    Check(Number(admin,"SELECT count(*) FROM app_world.\"TITEMTABLE\""+item_where)==0&&
          Number(admin,"SELECT count(*) FROM app_world.skill_item_consumptions WHERE char_id="+cid+" AND after_count=0 AND after_hash IS NULL")==1,
          "last reagent deletes exactly its owned row and appends atomic deletion receipt");
    auto uncertain=after;uncertain.persistence_uncertain=true;
    Check(Throws([&]{map.SaveAuthorized(c,uncertain);}),"unknown item outcome cannot overwrite committed state through logout");
    map.SaveAuthorized(c,after);
    Check(Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE char_id="+cid)==0,"confirmed reagent state survives normal final save");
}
