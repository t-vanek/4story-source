#pragma once
#include "services/skill_reagent.h"

tmapsvr::transfer::State StoredReagentGraph(soci::session& sql,std::uint32_t cid) {
    std::string hex;sql<<"SELECT encode(transfer_body,'hex') FROM app_world.map_checkpoints WHERE char_id=:c",soci::use(cid),soci::into(hex);
    std::vector<std::byte> body;
    for(std::size_t i=0;i<hex.size();i+=2)body.push_back(std::byte(std::stoul(hex.substr(i,2),nullptr,16)));
    auto graph=tmapsvr::transfer::Decode(body);if(!graph)throw std::runtime_error("Invalid test graph");return *graph;
}
void PublishReagentHash(tmapsvr::CharSnapshot& s,std::uint64_t id,const std::string& hash) {
    auto p=std::make_shared<tmapsvr::CharacterPayload>(*s.payload);
    for(auto& bag:p->bags)for(auto& item:bag.items)if(item.dlID==id)item.durable_hash=hash;
    s.payload=std::move(p);
}
void VerifyGraphReagent(soci::session& admin,SessionPool& pool,tmapsvr::PostgreSQLMapService& source,
    const char* connection,const char* manifest,const char* routing,const char* actor,tmapsvr::MapSessionClaim primary,bool recover) {
    using namespace tmapsvr;
    const auto cid=std::to_string(primary.char_id),user=std::to_string(primary.user_id);
    admin<<"INSERT INTO app_world.\"TSKILLTABLE\" VALUES(1,"+cid+",36,1,0) ON CONFLICT DO NOTHING";
    std::string original_items;admin<<"SELECT jsonb_agg(to_jsonb(i) ORDER BY \"dlID\")::text FROM app_world.\"TITEMTABLE\" i WHERE \"dwOwnerID\"="+cid,soci::into(original_items);
    SyntheticReplicaPartition partition(admin);
    auto owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
    PostgreSQLMapService target(pool,{1,2,owner->Token(),manifest,routing,actor});
    primary.endpoint_ip=0x0100007f;primary.endpoint_port=5815;
    Check(source.ClaimSession(primary,*source.LookupSession(primary.user_id,primary.key)).has_value(),"graph reagent source claims Login handoff");
    auto snap=*source.LoadAuthorized(primary);snap.wMapID=0;snap.fPosX=4080;snap.fPosY=80;snap.fPosZ=3584;
    Check(source.AuthorizeReplicas(primary,0,4080,3584,{{0x0100007f,5815,2}}),"graph reagent target receives replica grant");
    auto secondary=primary;secondary.connection_id=221;
    Check(target.ClaimSession(secondary,*target.LookupSession(primary.user_id,primary.key)).has_value(),"graph reagent target claims replica");
    secondary.role=MapSessionRole::Replica;
    Check(target.LoadReplica(secondary,0,4080,3584),"graph reagent target loads summary");
    target.MarkReady(secondary,snap);source.MarkReady(primary,snap);
    SkillCooldownTracker timers;timers.Restore(primary.char_id,snap.payload->skills,0);snap.fPosX=4100;
    auto graph=transfer::Capture(snap,primary.key,timers,0);
    auto item=std::find_if(graph.items.begin(),graph.items.end(),[](const auto& row){return row.storage_id==255&&row.slot==1;});
    Check(item!=graph.items.end(),"graph reagent fixture starts with actual source-owned starter slot");
    // Explicit synthetic unsaved graph changes: no corresponding item row is
    // needed, and full unsigned identity/raw extensions must survive consumption.
    item->id=0xfedcba9876543210ULL;item->item=8412;item->count=3;item->texture=0xfedcba98;item->eld=0x12345600;
    item->magic={{3,100}};
    graph.quests.push_back({12345,67890,1,1,1});graph.buffs.push_back({1,1,1,4,134,50000,primary.char_id,primary.char_id});
    auto cabinet=*item;cabinet.id++;cabinet.storage=1;cabinet.storage_id=0;graph.items.push_back(cabinet);
    const auto body=transfer::Encode(graph);
    Check(source.PrepareTransfer(primary,snap,body),"graph reagent source prepares complete transient inventory");
    auto received=target.AcceptTransfer(secondary,body);Check(received.has_value(),"graph reagent target promotes complete state");
    auto promoted=secondary;promoted.role=MapSessionRole::Primary;promoted.authority_epoch=received->authority_epoch;
    auto live=received->snapshot;target.MarkReady(promoted,live);
    const auto before=*FindSkillReagent(live,8412);
    Check(before.dlID==0xfedcba9876543210ULL&&before.durable_hash.size()==64&&before.bCount==3,
          "transferred item hashes original server fields and keeps unsigned 64-bit identity");
    auto after=live;ConsumeReagentProjection(after,before);--after.dwMP;
    auto skills=std::make_shared<CharacterPayload>(*after.payload);
    for(auto& skill:skills->skills)if(skill.wSkillID==36)skill.dwRemainTick=28000;
    after.payload=skills;
    const auto old_graph=transfer::Encode(StoredReagentGraph(admin,primary.char_id));
    if(!recover) {
        auto stale=promoted;--stale.authority_epoch;
        Check(Throws([&]{target.ConsumeSkillItem(stale,36,before,after);}),"graph item consumption fences stale authority epoch");
        stale=promoted;stale.role=MapSessionRole::Replica;
        Check(Throws([&]{target.ConsumeSkillItem(stale,36,before,after);}),"replica cannot charge graph inventory");
        auto drift=before;drift.durable_hash=std::string(64,'0');
        Check(Throws([&]{target.ConsumeSkillItem(promoted,36,drift,after);}),"graph item full-field hash rejects stale input");
        auto changed=after;auto p=std::make_shared<CharacterPayload>(*after.payload);
        auto g=std::make_shared<transfer::State>(*p->transfer_state);g->quests[0].remaining++;p->transfer_state=g;changed.payload=p;
        Check(Throws([&]{target.ConsumeSkillItem(promoted,36,before,changed);}),"reagent transaction cannot alter unrelated quest graph");
        p=std::make_shared<CharacterPayload>(*after.payload);p->skills.front().bLevel++;changed.payload=p;
        Check(Throws([&]{target.ConsumeSkillItem(promoted,36,before,changed);}),"graph consumption cannot silently change learned rank");
        p=std::make_shared<CharacterPayload>(*after.payload);p->transfer_state.reset();changed.payload=p;
        Check(Throws([&]{target.ConsumeSkillItem(promoted,36,before,changed);}),"transferred authority cannot downgrade to fresh row storage");
        admin<<"CREATE TRIGGER synthetic_graph_reagent_fault BEFORE INSERT ON app_world.skill_item_consumptions FOR EACH ROW EXECUTE FUNCTION public.reject_checkpoint()";
        Check(Throws([&]{target.ConsumeSkillItem(promoted,36,before,after);}),"late graph reagent receipt failure aborts transaction");
        admin<<"DROP TRIGGER synthetic_graph_reagent_fault ON app_world.skill_item_consumptions";
        Check(transfer::Encode(StoredReagentGraph(admin,primary.char_id))==old_graph&&
              Number(admin,"SELECT \"dwMP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwCharID\"="+cid)==live.dwMP&&
              Number(admin,"SELECT count(*) FROM app_world.skill_item_consumptions WHERE char_id="+cid)==0,
              "failed graph charge preserves complete item graph core timers and audit atomically");
    }
    const auto hash=target.ConsumeSkillItem(promoted,36,before,after);PublishReagentHash(after,before.dlID,hash);
    SkillCooldownTracker expected_timers;expected_timers.Restore(primary.char_id,after.payload->skills,0);
    Check(transfer::Encode(StoredReagentGraph(admin,primary.char_id))==transfer::Encode(transfer::Capture(after,primary.key,expected_timers,0)),
          "confirmed graph decrement preserves every other source field with current core and timers");
    Check(Number(admin,"SELECT count(*) FROM app_world.skill_item_consumptions s JOIN app_world.map_checkpoints p ON p.char_id=s.char_id WHERE s.char_id="+cid+
          " AND state_contract=2 AND before_count=3 AND after_count=2 AND item_id=-81985529216486896 AND before_graph_hash<>after_graph_hash AND after_graph_hash=p.transfer_hash AND p.recovery_contract=2 AND p.revision=0")==1,
          "graph receipt records unsigned item bits exact graph transition and unchanged periodic revision");
    std::string items;admin<<"SELECT jsonb_agg(to_jsonb(i) ORDER BY \"dlID\")::text FROM app_world.\"TITEMTABLE\" i WHERE \"dwOwnerID\"="+cid,soci::into(items);
    Check(items==original_items,"graph consumption never reloads or rewrites stale normalized item rows");
    if(recover) {
        owner.reset();owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
        Check(owner->RecoveredSessions()==1&&Number(admin,"SELECT count(*) FROM app_world.map_checkpoints WHERE user_id="+user+" AND outcome='recovered'")==1&&
              transfer::Encode(StoredReagentGraph(admin,primary.char_id))==transfer::Encode(transfer::Capture(after,primary.key,expected_timers,0)),
              "target replacement recovers exact committed graph charge before any periodic sweep");
        Check(Throws([&]{target.ConsumeSkillItem(promoted,36,before,after);}),"replaced target process cannot consume graph state again");
        return;
    }
    Check(Throws([&]{target.ConsumeSkillItem(promoted,36,before,after);}),"replayed graph snapshot cannot consume twice");
    target.CheckpointAuthorized(promoted,after,1);after.fPosX=4080;
    auto back=transfer::Encode(transfer::Capture(after,primary.key,expected_timers,0));
    Check(target.PrepareTransfer(promoted,after,back),"consumed graph can transfer back through original source socket");
    auto returning=primary;returning.role=MapSessionRole::Replica;
    auto returned=source.AcceptTransfer(returning,back);Check(returned&&returned->authority_epoch==2,"returned graph preserves newer authority");
    auto current=primary;current.authority_epoch=2;live=returned->snapshot;source.MarkReady(current,live);
    Check(FindSkillReagent(live,8412)->bCount==2,"return hydration retains consumed stack and rebuilds its graph item hash");
    for(int count:{2,1}) {
        const auto next=*FindSkillReagent(live,8412);auto plan=live;ConsumeReagentProjection(plan,next);
        if(count==2)Check(Throws([&]{source.ConsumeSkillItem(primary,36,next,plan);}),"original same-socket epoch cannot charge returned inventory");
        const auto next_hash=source.ConsumeSkillItem(current,36,next,plan);PublishReagentHash(plan,next.dlID,next_hash);live=plan;
    }
    Check(!FindSkillReagent(live,8412)&&Number(admin,"SELECT count(*) FROM app_world.skill_item_consumptions WHERE char_id="+cid+" AND state_contract=2")==3,
          "last graph reagent disappears without consuming same-template cabinet item");
    source.SaveAuthorized(current,live);
    const auto saved=StoredReagentGraph(admin,primary.char_id);
    Check(std::count_if(saved.items.begin(),saved.items.end(),[](const auto& row){return row.item==8412;})==1&&saved.quests[0].remaining==67890,
          "final graph save retains unrelated cabinet item and quest state after deletion");
}
