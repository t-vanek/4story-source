#pragma once
// Transaction-level fixtures. The original client sender/receiver oracle is
// independently implemented in verify_skill_actions_wire.py.
void VerifySkillActions(soci::session& admin,SessionPool& pool,tmapsvr::PostgreSQLMapService& source,
    const char* connection,const char* manifest,const char* routing,const char* actor,tmapsvr::MapSessionClaim primary,bool graph) {
    using namespace tmapsvr;const auto cid=std::to_string(primary.char_id);
    admin<<"UPDATE app_world.\"TCHARTABLE\" SET \"bLevel\"=60,\"dwHP\"=100,\"dwMP\"=100000 WHERE \"dwCharID\"="+cid;
    for(int skill:{34,240,629})admin<<"INSERT INTO app_world.\"TSKILLTABLE\" VALUES(1,"+cid+","+std::to_string(skill)+",2,0) ON CONFLICT (\"bWorldID\",\"dwCharID\",\"wSkillID\") DO UPDATE SET \"bLevel\"=2,\"dwRemainTick\"=0";
    primary.endpoint_ip=0x0100007f;primary.endpoint_port=5815;
    Check(source.ClaimSession(primary,*source.LookupSession(primary.user_id,primary.key)).has_value(),"action claims native primary");
    auto live=*source.LoadAuthorized(primary);live.wMapID=0;live.fPosX=4080;live.fPosY=80;live.fPosZ=3584;source.MarkReady(primary,live);
    const auto action=[&](std::uint16_t skill) {
        std::vector<std::byte> b;const auto add=[&](auto v){wire::WritePOD(b,v);};
        add(primary.char_id);add(std::uint8_t{1});add(std::uint8_t{2});add(std::uint32_t{3});add(std::uint32_t{4});
        add(primary.channel);add(std::uint16_t{0});add(skill);return b;
    };
    const auto finish=[&](std::uint16_t skill) {
        std::vector<std::byte> b;const auto add=[&](auto v){wire::WritePOD(b,v);};
        add(primary.char_id);add(primary.char_id);add(std::uint8_t{1});add(1.f);add(2.f);add(3.f);add(skill);
        add(std::uint32_t{0});add(std::uint32_t{0});add(std::uint16_t{0});add(std::uint8_t{1});add(primary.char_id);add(std::uint8_t{1});return b;
    };
    auto result=source.CommitSkillAction(primary,action(65000),live);live=*result.snapshot;
    Check(result.acknowledgement.size()==17&&result.acknowledgement[0]==std::byte{1},"unlearned ACTION returns original NOTFOUND instead of placeholder success");
    const auto mp=live.dwMP;live.dwMP=0;result=source.CommitSkillAction(primary,action(240),live);live=*result.snapshot;
    Check(result.acknowledgement[0]==std::byte{7},"ACTION checks source MP before starting charge");live.dwMP=mp;
    {auto p=std::make_shared<CharacterPayload>(*live.payload);for(auto& s:p->skills)if(s.wSkillID==240)s.dwRemainTick=500;live.payload=p;}
    result=source.CommitSkillAction(primary,action(240),live);live=*result.snapshot;
    Check(result.acknowledgement[0]==std::byte{6},"ACTION returns source cooldown code");
    {auto p=std::make_shared<CharacterPayload>(*live.payload);for(auto& s:p->skills)s.dwRemainTick=0;live.payload=p;}
    const auto pure_mp=live.payload->pure_max_mp,pure_hp=live.payload->pure_max_hp;
    const auto old_aftermath=live.bAftermath;live.bAftermath=100;live.dwMP=pure_mp*5/100-1;
    result=source.CommitSkillAction(primary,action(34),live);live=*result.snapshot;
    Check(live.payload->pure_max_mp==pure_mp&&live.payload->pure_max_hp==pure_hp&&live.dwMaxMP<pure_mp,
        "pure HP and MP exclude aftermath while visible maximum changes");
    Check(result.acknowledgement[0]==std::byte{7},"percentage ACTION rejects insufficient pure-base MP despite affordable modified maximum");
    live.dwMP=live.dwMaxMP;
    for(bool loop:{false,true}) {
        {auto p=std::make_shared<CharacterPayload>(*live.payload);for(auto& s:p->skills)s.dwRemainTick=0;live.payload=p;}
        const auto before=live.dwMP;auto [request,after]=PlannedCast(primary,live,34,loop);
        live=*source.CommitSkillCast(primary,request,live,after).snapshot;
        Check(before-live.dwMP==pure_mp*5/100,"native ordinary and loop casts commit source pure-base percentage cost");
    }
    live.bAftermath=old_aftermath;
    result=source.CommitSkillAction(primary,action(65000),live);live=*result.snapshot;
    const auto saved=live;
    auto forged=action(240);forged[0]^=std::byte{1};Check(Throws([&]{source.CommitSkillAction(primary,forged,live);}),"ACTION rejects spoofed PC");
    forged=action(240);forged[14]^=std::byte{1};Check(Throws([&]{source.CommitSkillAction(primary,forged,live);}),"ACTION rejects a different channel");
    forged=action(240);forged.push_back(std::byte{0});Check(Throws([&]{source.CommitSkillAction(primary,forged,live);}),"ACTION rejects trailing payload");
    for(int field:{0,1,2}){auto wrong=primary;if(field==0)++wrong.connection_id;else if(field==1)++wrong.authority_epoch;else wrong.role=MapSessionRole::Replica;
        Check(Throws([&]{source.CommitSkillAction(wrong,action(240),live);}),"ACTION fences stale connection epoch and replica");}
    const auto count=Number(admin,"SELECT count(*) FROM app_world.skill_action_operations WHERE char_id="+cid);
    admin<<"CREATE TRIGGER synthetic_action_fault BEFORE INSERT ON app_world.map_checkpoints FOR EACH ROW EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{source.CommitSkillAction(primary,action(240),live);}),"late ACTION failure rolls back charge and ledger");
    admin<<"DROP TRIGGER synthetic_action_fault ON app_world.map_checkpoints";
    Check(Number(admin,"SELECT count(*) FROM app_world.skill_action_operations WHERE char_id="+cid)==count,"failed ACTION publishes no charge");
    const auto race=[&]()->std::optional<SkillActionCommit>{try{return source.CommitSkillAction(primary,action(240),live);}catch(...){return {};}};
    auto one=std::async(std::launch::async,race),two=std::async(std::launch::async,race);auto a=one.get(),b=two.get();
    Check(bool(a)!=bool(b),"concurrent ACTION commits one charge from the same snapshot");live=*(a?a->snapshot:b->snapshot);
    Check(live.dwMP==saved.dwMP&&live.dwHP==saved.dwHP&&live.payload->last_action_id>saved.payload->last_action_id,"ACTION charges no resources and advances its durable head");
    Check(Throws([&]{source.SaveAuthorized(primary,saved);}),"pre-action logout cannot overwrite action state");
    auto active=primary;auto* service=&source;
    std::unique_ptr<SyntheticReplicaPartition> partition;std::unique_ptr<PostgreSQLMapOwner> owner;std::unique_ptr<PostgreSQLMapService> target;
    if(graph) {
        partition=std::make_unique<SyntheticReplicaPartition>(admin);owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
        target=std::make_unique<PostgreSQLMapService>(pool,PostgreSQLMapConfig{1,2,owner->Token(),manifest,routing,actor});
        Check(source.AuthorizeReplicas(primary,0,4080,3584,{{0x0100007f,5815,2}}),"charge authorizes replica");
        auto secondary=primary;secondary.connection_id+=1000;
        Check(target->ClaimSession(secondary,*target->LookupSession(primary.user_id,primary.key)).has_value(),"charge claims replica");
        secondary.role=MapSessionRole::Replica;Check(target->LoadReplica(secondary,0,4080,3584),"charge loads replica");target->MarkReady(secondary,live);
        live.fPosX=4100;SkillCooldownTracker timers;timers.Restore(primary.char_id,live.payload->skills,0);
        const auto body=transfer::Encode(transfer::Capture(live,primary.key,timers,0));
        Check(source.PrepareTransfer(primary,live,body),"charge prepares primary transfer");const auto received=target->AcceptTransfer(secondary,body);
        Check(received.has_value(),"charge promotes target");active=secondary;active.role=MapSessionRole::Primary;active.authority_epoch=received->authority_epoch;
        live=received->snapshot;service=target.get();service->MarkReady(active,live);
    }
    auto [cast,after]=PlannedCast(active,live,240,false,true);live=*service->CommitSkillCast(active,cast,live,after).snapshot;
    Check(Number(admin,"SELECT count(*) FROM app_world.accepted_skill_casts WHERE char_id="+cid+" AND charge_action_id IS NOT NULL")==1,"accepted First Aid binds pending charge across primary owners");
    // Deterministic shared-clock boundary; actual 300ms waiting is tested over TCP.
    admin<<"UPDATE app_world.skill_action_operations SET started_at=clock_timestamp()+interval '1 minute' WHERE char_id="+cid+" AND charge_ms>0";
    Check(Throws([&]{service->FinishSkill(active,finish(240),live);}),"First Aid cannot finish before authoritative charge time");
    admin<<"UPDATE app_world.skill_action_operations SET started_at=clock_timestamp()-interval '1 second' WHERE char_id="+cid+" AND charge_ms>0";
    const auto spent=live;auto healed=service->FinishSkill(active,finish(240),live);live=*healed.snapshot;
    Check(live.dwHP>spent.dwHP&&live.dwMP==spent.dwMP&&healed.packets.size()==2,"charged First Aid finishes without charging MP twice");
    Check(Throws([&]{service->FinishSkill(active,finish(240),live);}),"finished charge cannot be replayed");
    result=service->CommitSkillAction(active,action(629),live);live=*result.snapshot;
    Check(result.acknowledgement[0]==std::byte{0},"recall-target Aid can begin ACTION without granting an own-PC finish");
    // Advance the sampled reuse timer, then explicitly cancel another valid
    // First Aid charge. Aid's recall target is intentionally not self-healed.
    {auto p=std::make_shared<CharacterPayload>(*live.payload);for(auto& s:p->skills)s.dwRemainTick=0;live.payload=p;}
    result=service->CommitSkillAction(active,action(240),live);live=*result.snapshot;
    result=service->CommitSkillAction(active,action(0),live);live=*result.snapshot;
    Check(result.acknowledgement[0]==std::byte{0},"zero-skill ACTION retains source acknowledgement and cancels pending charge");
    auto cancelled=PlannedCast(active,live,240,false,true);live=*service->CommitSkillCast(active,cancelled.first,live,cancelled.second).snapshot;
    Check(Number(admin,"SELECT count(*) FROM app_world.accepted_skill_casts WHERE char_id="+cid+" AND skill_id=240 AND charge_action_id IS NULL")==1,"cancelled charge cannot authorize a later cast");
    Check(Throws([&]{service->FinishSkill(active,finish(240),live);}),"cancelled First Aid cannot produce healing");
    service->SaveAuthorized(active,live);
}
