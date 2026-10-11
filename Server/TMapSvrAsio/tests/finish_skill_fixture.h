#pragma once
// Actual PostgreSQL transaction tests; independent packet/formula oracle lives
// in verify_finish_skill_wire.py, not this service-boundary builder.
void VerifySkillFinishes(soci::session& admin,SessionPool& pool,tmapsvr::PostgreSQLMapService& source,
    const char* connection,const char* manifest,const char* routing,const char* actor,tmapsvr::MapSessionClaim primary,bool graph) {
    using namespace tmapsvr;const auto cid=std::to_string(primary.char_id);
    admin<<"UPDATE app_world.\"TCHARTABLE\" SET \"bLevel\"=60,\"dwHP\"=100,\"dwMP\"=100000 WHERE \"dwCharID\"="+cid;
    admin<<"INSERT INTO app_world.\"TSKILLTABLE\" VALUES(1,"+cid+",509,2,0) ON CONFLICT (\"bWorldID\",\"dwCharID\",\"wSkillID\") DO UPDATE SET \"bLevel\"=2,\"dwRemainTick\"=0";
    primary.endpoint_ip=0x0100007f;primary.endpoint_port=5815;
    Check(source.ClaimSession(primary,*source.LookupSession(primary.user_id,primary.key)).has_value(),"finish claims native primary");
    auto live=*source.LoadAuthorized(primary);live.wMapID=0;live.fPosX=4080;live.fPosY=80;live.fPosZ=3584;source.MarkReady(primary,live);
    auto [cast,projection]=PlannedCast(primary,live,509,false,true);
    live=*source.CommitSkillCast(primary,cast,live,projection).snapshot;
    auto active=primary;auto* service=&source;
    std::unique_ptr<SyntheticReplicaPartition> partition;std::unique_ptr<PostgreSQLMapOwner> owner;std::unique_ptr<PostgreSQLMapService> target;
    if(graph) {
        partition=std::make_unique<SyntheticReplicaPartition>(admin);owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
        target=std::make_unique<PostgreSQLMapService>(pool,PostgreSQLMapConfig{1,2,owner->Token(),manifest,routing,actor});
        Check(source.AuthorizeReplicas(primary,0,4080,3584,{{0x0100007f,5815,2}}),"pending finish authorizes replica");
        auto secondary=primary;secondary.connection_id+=1000;
        Check(target->ClaimSession(secondary,*target->LookupSession(primary.user_id,primary.key)).has_value(),"pending finish claims replica");
        secondary.role=MapSessionRole::Replica;Check(target->LoadReplica(secondary,0,4080,3584),"pending finish loads replica");target->MarkReady(secondary,live);
        live.fPosX=4100;SkillCooldownTracker timers;timers.Restore(primary.char_id,live.payload->skills,0);
        const auto body=transfer::Encode(transfer::Capture(live,primary.key,timers,0));
        Check(source.PrepareTransfer(primary,live,body),"pending finish prepares source graph");const auto received=target->AcceptTransfer(secondary,body);
        Check(received.has_value(),"pending finish promotes target");active=secondary;active.role=MapSessionRole::Primary;active.authority_epoch=received->authority_epoch;
        live=received->snapshot;service=target.get();service->MarkReady(active,live);
    }
    std::vector<std::byte> body;const auto add=[&](auto v){wire::WritePOD(body,v);};
    add(primary.char_id);add(primary.char_id);add(std::uint8_t{1});add(1.f);add(2.f);add(3.f);
    add(std::uint16_t{509});add(std::uint32_t{0});add(std::uint32_t{0});add(std::uint16_t{0});add(std::uint8_t{1});add(primary.char_id);add(std::uint8_t{1});
    const auto receipt=[&]{std::string s;admin<<"SELECT to_jsonb(p)::text FROM app_world.map_checkpoints p WHERE char_id="+cid,soci::into(s);return s;};
    const auto saved=receipt();const auto stale=live;
    auto forged=body;forged[0]^=std::byte{1};Check(Throws([&]{service->FinishSkill(active,forged,live);}),"finish rejects spoofed host");
    forged=body;forged[34]^=std::byte{1};Check(Throws([&]{service->FinishSkill(active,forged,live);}),"finish rejects another target");
    forged=body;forged.push_back(std::byte{0});Check(Throws([&]{service->FinishSkill(active,forged,live);}),"finish rejects trailing bytes");
    forged=body;forged[33]=std::byte{2};forged.insert(forged.end(),body.begin()+34,body.end());
    Check(Throws([&]{service->FinishSkill(active,forged,live);}),"finish cannot multiply an accepted target occurrence");
    for(int field:{0,1,2}){auto wrong=active;if(field==0)++wrong.connection_id;else if(field==1)++wrong.authority_epoch;else wrong.role=MapSessionRole::Replica;
        Check(Throws([&]{service->FinishSkill(wrong,body,live);}),"finish rejects stale connection epoch and replica");}
    admin<<"CREATE TRIGGER synthetic_finish_fault BEFORE INSERT ON app_world.map_checkpoints FOR EACH ROW EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{service->FinishSkill(active,body,live);}),"late finish failure rolls back consumed occurrence and healing");
    admin<<"DROP TRIGGER synthetic_finish_fault ON app_world.map_checkpoints";
    Check(receipt()==saved&&Number(admin,"SELECT count(*) FROM app_world.skill_finish_operations WHERE char_id="+cid)==0,"rejected finishes preserve checkpoint and absence of receipts");
    const auto race=[&]()->std::optional<FinishSkillCommit>{try{return service->FinishSkill(active,body,live);}catch(...){return {};}};
    auto one=std::async(std::launch::async,race),two=std::async(std::launch::async,race);auto a=one.get(),b=two.get();
    Check(bool(a)!=bool(b),"concurrent finishes consume the accepted cast once");
    const auto& result=a?*a:*b;live=*result.snapshot;
    Check(live.dwHP>stale.dwHP&&live.dwMP==stale.dwMP&&result.packets.size()==2,"source self-cure heals without charging resources again");
    Check(live.payload->last_finish_id>0&&Number(admin,"SELECT count(*) FROM app_world.skill_finish_operations WHERE char_id="+cid)==1,"finish advances durable one-use ledger");
    Check(Throws([&]{service->FinishSkill(active,body,live);}),"replayed finish without another accepted cast cannot heal twice");
    Check(Throws([&]{service->SaveAuthorized(active,stale);}),"stale logout cannot overwrite finished cure");
    Check(Throws([&]{service->CheckpointAuthorized(active,stale,1);}),"stale periodic checkpoint cannot overwrite finished cure");
    Check(Number(admin,"SELECT count(*) FROM app_world.map_checkpoints WHERE char_id="+cid+" AND app_world.map_checkpoint_matches(map_checkpoints)")==1,"finished cure has complete durable recovery receipt");
    service->SaveAuthorized(active,live);
}
