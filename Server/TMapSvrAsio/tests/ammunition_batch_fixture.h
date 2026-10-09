#pragma once
#include "services/skill_reagent.h"

void VerifyAmmunitionBatch(soci::session& admin,SessionPool& pool,tmapsvr::PostgreSQLMapService& source,
    const char* connection,const char* manifest,const char* routing,const char* actor,tmapsvr::MapSessionClaim primary,
    bool graph,bool recover=false,bool multi=false) {
    using namespace tmapsvr;
    const std::uint16_t used_skill=multi?324:32;
    const auto cid=std::to_string(primary.char_id);
    admin<<"INSERT INTO app_world.\"TSKILLTABLE\" VALUES(1,"+cid+",32,1,0) ON CONFLICT DO NOTHING";
    if(multi)admin<<"INSERT INTO app_world.\"TSKILLTABLE\" VALUES(1,"+cid+",324,2,0),(1,"+cid+",412,5,0),(1,"+cid+",1407,1,0)";
    admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"wItemID\"=701,\"dwDuraMax\"=100,\"dwDuraCur\"=100 WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=254 AND \"bItemID\"=0";
    admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"wItemID\"=8401,\"bCount\"=1 WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=255 AND \"bItemID\"=1";
    // Reuse the typed PostgreSQL row to preserve every synthetic fixture field.
    admin<<"INSERT INTO app_world.\"TITEMTABLE\" SELECT (jsonb_populate_record(NULL::app_world.\"TITEMTABLE\",to_jsonb(i)||"
           "jsonb_build_object('dlID',9000000+\"dwOwnerID\",'bItemID',2,'wItemID',11054,'bCount',5))).* "
           "FROM app_world.\"TITEMTABLE\" i WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=255 AND \"bItemID\"=1";
    std::string original_items;admin<<"SELECT jsonb_agg(to_jsonb(i) ORDER BY \"dlID\")::text FROM app_world.\"TITEMTABLE\" i WHERE \"dwOwnerID\"="+cid,soci::into(original_items);
    std::unique_ptr<SyntheticReplicaPartition> partition;
    std::unique_ptr<PostgreSQLMapOwner> owner;
    std::unique_ptr<PostgreSQLMapService> target;
    if(graph) {
        partition=std::make_unique<SyntheticReplicaPartition>(admin);
        owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
        target=std::make_unique<PostgreSQLMapService>(pool,PostgreSQLMapConfig{1,2,owner->Token(),manifest,routing,actor});
    }
    primary.endpoint_ip=0x0100007f;primary.endpoint_port=5815;
    Check(source.ClaimSession(primary,*source.LookupSession(primary.user_id,primary.key)).has_value(),"batch source claims native handoff");
    auto live=*source.LoadAuthorized(primary);auto active=primary;auto* service=&source;
    live.wMapID=0;live.fPosX=4080;live.fPosY=80;live.fPosZ=3584;
    source.MarkReady(primary,live);
    if(graph) {
        Check(source.AuthorizeReplicas(primary,0,4080,3584,{{0x0100007f,5815,2}}),"batch graph replica grant");
        auto secondary=primary;secondary.connection_id+=1000;
        Check(target->ClaimSession(secondary,*target->LookupSession(primary.user_id,primary.key)).has_value(),"batch graph replica claimed");
        secondary.role=MapSessionRole::Replica;
        Check(target->LoadReplica(secondary,0,4080,3584),"batch graph replica loaded");target->MarkReady(secondary,live);
        live.fPosX=4100;SkillCooldownTracker timers;timers.Restore(primary.char_id,live.payload->skills,0);
        auto state=transfer::Capture(live,primary.key,timers,0);
        state.quests.push_back({12345,67890,1,1,1});
        for(auto& item:state.items)if(item.storage==0&&item.storage_id==255&&(item.slot==1||item.slot==2)) {
            item.id=0xfedcba9876543200ULL+item.slot;item.texture=0xfedcba98;item.magic={{3,100}};
        }
        const auto body=transfer::Encode(state);
        Check(source.PrepareTransfer(primary,live,body),"batch graph transfer prepared");
        auto received=target->AcceptTransfer(secondary,body);Check(received.has_value(),"batch graph promoted");
        active=secondary;active.role=MapSessionRole::Primary;active.authority_epoch=received->authority_epoch;
        live=received->snapshot;service=target.get();service->MarkReady(active,live);
    }
    if(multi) {
        for(const auto& [id,count]:std::vector<std::pair<int,int>>{{324,4},{412,7},{1407,6}}) {
            const auto t=std::find_if(live.payload->skill_templates.begin(),live.payload->skill_templates.end(),[&](const auto& row){return row.wID==id;});
            Check(t!=live.payload->skill_templates.end()&&t->multi_attack&&t->multi_attack->count==count,"native hydration derives pinned multi-attack budget at learned rank");
        }
        if(graph)admin<<"UPDATE app_world.\"TSKILLTABLE\" SET \"bLevel\"=1 WHERE \"dwCharID\"="+cid+" AND \"wSkillID\"=324";
    }
    const auto debits=FindSkillAmmunition(live,24,4);
    Check(debits.size()==2&&debits[0].count==1&&debits[1].count==3&&debits[1].before.wItemID==11054,
          "native four-target selection spans ordered mixed-template arrow stacks");
    auto after=live;ConsumeSkillItemProjection(after,debits);--after.dwMP;
    auto p=std::make_shared<CharacterPayload>(*after.payload);
    for(auto& skill:p->skills)if(skill.wSkillID==used_skill)skill.dwRemainTick=multi?12300:1500;
    after.payload=p;
    std::string old_checkpoint;admin<<"SELECT to_jsonb(p)::text FROM app_world.map_checkpoints p WHERE char_id="+cid,soci::into(old_checkpoint);
    if(multi) {
        auto drift=after;auto altered=std::make_shared<CharacterPayload>(*after.payload);
        for(auto& skill:altered->skills)if(skill.wSkillID==324)skill.bLevel=1;
        drift.payload=altered;
        Check(Throws([&]{service->ConsumeSkillItems(active,used_skill,4,debits,drift);}),"multi-attack refuses a rank projection different from canonical learned state");
    }
    auto wrong=debits;wrong[1].before.durable_hash=std::string(64,'0');
    Check(Throws([&]{service->ConsumeSkillItems(active,used_skill,4,wrong,after);}),"stale second stack rolls back every earlier debit");
    wrong=debits;std::reverse(wrong.begin(),wrong.end());
    Check(Throws([&]{service->ConsumeSkillItems(active,used_skill,4,wrong,after);}),"reordered batch cannot bypass original selection");
    Check(Throws([&]{service->ConsumeSkillItems(active,used_skill,3,debits,after);}),"debit quantity must match verified target count");
    wrong=debits;wrong.push_back(wrong.front());
    Check(Throws([&]{service->ConsumeSkillItems(active,used_skill,4,wrong,after);}),"duplicate item cannot be debited twice in one cast");
    admin<<"CREATE TRIGGER synthetic_batch_fault BEFORE INSERT ON app_world.skill_item_consumptions FOR EACH ROW WHEN (NEW.after_count>0) EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{service->ConsumeSkillItems(active,used_skill,4,debits,after);}),"second receipt failure rolls back both stack writes and first receipt");
    admin<<"DROP TRIGGER synthetic_batch_fault ON app_world.skill_item_consumptions";
    std::string checkpoint,items;
    admin<<"SELECT to_jsonb(p)::text FROM app_world.map_checkpoints p WHERE char_id="+cid,soci::into(checkpoint);
    admin<<"SELECT jsonb_agg(to_jsonb(i) ORDER BY \"dlID\")::text FROM app_world.\"TITEMTABLE\" i WHERE \"dwOwnerID\"="+cid,soci::into(items);
    Check(checkpoint==old_checkpoint&&items==original_items&&
          Number(admin,"SELECT count(*) FROM app_world.skill_item_consumptions WHERE char_id="+cid)==0&&
          Number(admin,"SELECT \"dwMP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwCharID\"="+cid)==live.dwMP,
          "failed batch leaves exact inventory checkpoint core and audit unchanged");
    const auto hashes=service->ConsumeSkillItems(active,used_skill,4,debits,after);
    Check(hashes.size()==2&&hashes[0].empty()&&hashes[1].size()==64,"batch returns one exact hash or deletion per ordered debit");
    for(std::size_t i=0;i<debits.size();++i)PublishReagentHash(after,debits[i].before.dlID,hashes[i]);
    Check(Number(admin,"SELECT count(*) FROM app_world.skill_item_consumptions WHERE char_id="+cid)==2&&
          Number(admin,"SELECT count(DISTINCT cast_id) FROM app_world.skill_item_consumptions WHERE char_id="+cid)==1&&
          Number(admin,"SELECT sum(before_count-after_count) FROM app_world.skill_item_consumptions WHERE char_id="+cid)==4&&
          Number(admin,"SELECT count(*) FROM app_world.skill_item_consumptions WHERE char_id="+cid+" AND hit_count=4 AND consumption_kind='ammunition' AND state_contract="+(graph?"2":"3"))==2,
          "both durable stack receipts identify one four-hit cast and correct storage contract");
    if(multi)Check(Number(admin,"SELECT count(*) FROM app_world.skill_item_consumptions WHERE char_id="+cid+" AND hit_mode='expanded'")==2,"expanded ammo receipts distinguish source-generated hits");
    Check(Throws([&]{service->ConsumeSkillItems(active,used_skill,4,debits,after);}),"batch replay cannot charge again");
    if(graph) {
        SkillCooldownTracker timers;timers.Restore(primary.char_id,after.payload->skills,0);
        Check(transfer::Encode(StoredReagentGraph(admin,primary.char_id))==transfer::Encode(transfer::Capture(after,primary.key,timers,0)),
              "graph batch preserves complete raw item extensions and unrelated quest");
        admin<<"SELECT jsonb_agg(to_jsonb(i) ORDER BY \"dlID\")::text FROM app_world.\"TITEMTABLE\" i WHERE \"dwOwnerID\"="+cid,soci::into(items);
        Check(items==original_items,"batch graph never touches stale normalized inventory");
        if(recover) {
            owner.reset();owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
            Check(owner->RecoveredSessions()==1&&transfer::Encode(StoredReagentGraph(admin,primary.char_id))==transfer::Encode(transfer::Capture(after,primary.key,timers,0)),
                  "replacement recovers both stack debits before periodic checkpoint");
            return;
        }
        after.fPosX=4080;const auto back=transfer::Encode(transfer::Capture(after,primary.key,timers,0));
        Check(target->PrepareTransfer(active,after,back),"batch graph prepares return transfer");
        auto returning=primary;returning.role=MapSessionRole::Replica;
        auto received=source.AcceptTransfer(returning,back);Check(received&&received->authority_epoch==2,"batch return retains authority epoch");
        active=primary;active.authority_epoch=2;after=received->snapshot;service=&source;source.MarkReady(active,after);
    }
    const auto final=FindSkillAmmunition(after,24,2);
    Check(final.size()==1&&final[0].before.bCount==2&&final[0].count==2,"remaining arrows survive batch and return hydration");
    ConsumeSkillItemProjection(after,final);service->ConsumeSkillItems(active,32,2,final,after);
    Check(FindSkillAmmunition(after,24,1).empty(),"last two-target cast depletes remaining stack");
    service->SaveAuthorized(active,after);
}
