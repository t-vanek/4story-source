#pragma once
void VerifySkillLearning(soci::session& admin,SessionPool& pool,tmapsvr::PostgreSQLMapService& source,
    const char* connection,const char* manifest,const char* routing,const char* actor,tmapsvr::MapSessionClaim primary,bool graph) {
    using namespace tmapsvr;const auto cid=std::to_string(primary.char_id);
    admin<<"UPDATE app_world.\"TCHARTABLE\" SET \"bLevel\"=60,\"dwGold\"=11,\"dwSilver\"=0,\"dwCooper\"=0,\"wSkillPoint\"=200,\"bStartAct\"=0 WHERE \"dwCharID\"="+cid;
    primary.endpoint_ip=0x0100007f;primary.endpoint_port=5815;
    Check(source.ClaimSession(primary,*source.LookupSession(primary.user_id,primary.key)).has_value(),"learning claims primary");
    auto live=*source.LoadAuthorized(primary);live.wMapID=0;live.fPosX=4080;live.fPosY=80;live.fPosZ=3584;source.MarkReady(primary,live);
    const auto request=[](std::uint16_t npc,std::uint16_t skill){std::vector<std::byte> b;wire::WritePOD(b,npc);wire::WritePOD(b,skill);return b;};
    const auto result=[&](PostgreSQLMapService& service,const MapSessionClaim& claim,std::uint16_t npc,std::uint16_t skill,int code,int rank){
        const auto r=service.PurchaseSkill(claim,request(npc,skill),live);live=*r.snapshot;
        const auto& ack=r.packets.back().second;Check(ack.size()==30&&std::to_integer<int>(ack[0])==code&&std::to_integer<int>(ack[3])==rank,"learning original result and rank with 30-byte ACK");return r;};
    result(source,primary,65535,240,1,0);
    result(source,primary,22047,509,11,0); // this synthetic character is class warrior
    const auto saved=live;
    auto r=result(source,primary,22047,131,0,1);
    Check(r.packets.size()==5&&live.payload->titles.size()==saved.payload->titles.size()+4,"positive purchase checks money and emits four ordered source gold-title gains before skill ACK");
    Check(live.dwGold<saved.dwGold&&live.wSkillPoint<saved.wSkillPoint,"learning atomically charges source price and skill point");
    Check(Number(admin,"SELECT \"bLevel\" FROM app_world.\"TSKILLTABLE\" WHERE \"dwCharID\"="+cid+" AND \"wSkillID\"=131")==1,"fresh learned rank persists");
    Check(Throws([&]{source.SaveAuthorized(primary,saved);}),"pre-purchase save cannot restore currency or skills");
    result(source,primary,22047,131,2,1);
    auto malformed=request(22047,131);malformed.push_back(std::byte{});
    Check(Throws([&]{source.PurchaseSkill(primary,malformed,live);}),"learning rejects trailing request data");
    for(int field:{0,1,2}){auto wrong=primary;if(field==0)++wrong.connection_id;else if(field==1)++wrong.authority_epoch;else wrong.role=MapSessionRole::Replica;
        Check(Throws([&]{source.PurchaseSkill(wrong,request(22047,101),live);}),"learning fences stale generation epoch and replica");}
    const auto normalized_rank=Number(admin,"SELECT COALESCE(max(\"bLevel\"),-1) FROM app_world.\"TSKILLTABLE\" WHERE \"dwCharID\"="+cid+" AND \"wSkillID\"=101");
    auto active=primary;auto* service=&source;
    std::unique_ptr<SyntheticReplicaPartition> partition;std::unique_ptr<PostgreSQLMapOwner> owner;std::unique_ptr<PostgreSQLMapService> target;
    if(graph) {
        partition=std::make_unique<SyntheticReplicaPartition>(admin);owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
        target=std::make_unique<PostgreSQLMapService>(pool,PostgreSQLMapConfig{1,2,owner->Token(),manifest,routing,actor});
        Check(source.AuthorizeReplicas(primary,0,4080,3584,{{0x0100007f,5815,2}}),"learning authorizes transfer replica");
        auto secondary=primary;secondary.connection_id+=1000;
        Check(target->ClaimSession(secondary,*target->LookupSession(primary.user_id,primary.key)).has_value(),"learning claims replica");
        secondary.role=MapSessionRole::Replica;Check(target->LoadReplica(secondary,0,4080,3584),"learning loads replica");target->MarkReady(secondary,live);
        live.fPosX=4100;SkillCooldownTracker timers;timers.Restore(primary.char_id,live.payload->skills,0);
        const auto body=transfer::Encode(transfer::Capture(live,primary.key,timers,0));
        Check(source.PrepareTransfer(primary,live,body),"learning prepares original full graph");const auto received=target->AcceptTransfer(secondary,body);
        Check(received.has_value(),"learning promotes target");active=secondary;active.role=MapSessionRole::Primary;active.authority_epoch=received->authority_epoch;
        live=received->snapshot;service=target.get();service->MarkReady(active,live);
    }
    const auto count=Number(admin,"SELECT count(*) FROM app_world.skill_purchase_operations WHERE char_id="+cid);
    admin<<"CREATE TRIGGER synthetic_learning_fault BEFORE INSERT ON app_world.map_checkpoints FOR EACH ROW EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{service->PurchaseSkill(active,request(22047,101),live);}),"late learning failure rolls back money rank and receipt");
    admin<<"DROP TRIGGER synthetic_learning_fault ON app_world.map_checkpoints";
    Check(Number(admin,"SELECT count(*) FROM app_world.skill_purchase_operations WHERE char_id="+cid)==count,"failed learning publishes no operation");
    const auto race=[&]()->std::optional<SkillPurchaseCommit>{try{return service->PurchaseSkill(active,request(22047,101),live);}catch(...){return {};}};
    auto one=std::async(std::launch::async,race),two=std::async(std::launch::async,race);auto a=one.get(),b=two.get();
    Check(bool(a)!=bool(b),"concurrent purchases accept exactly one snapshot");live=*(a?a->snapshot:b->snapshot);
    Check((a?a->packets:b->packets).back().second[0]==std::byte{0},"concurrent winner learns offered warrior skill");
    result(*service,active,22047,104,0,1);
    result(*service,active,22047,102,0,1);
    r=result(*service,active,22047,101,0,2);
    Check(std::any_of(live.payload->skills.begin(),live.payload->skills.end(),[](const auto& s){return s.wSkillID==101&&s.bLevel==2;}),"rank upgrade replaces learned rank in authoritative payload");
    if(graph)Check(Number(admin,"SELECT COALESCE(max(\"bLevel\"),-1) FROM app_world.\"TSKILLTABLE\" WHERE \"dwCharID\"="+cid+" AND \"wSkillID\"=101")==normalized_rank,"transferred learning leaves stale normalized skill rows untouched");
    service->SaveAuthorized(active,live);
    Check(Number(admin,"SELECT count(*) FROM app_world.map_checkpoints WHERE char_id="+cid+" AND outcome='logout'")==1,"learning saves recoverable logout checkpoint");
}
