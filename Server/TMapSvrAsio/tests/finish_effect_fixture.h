#pragma once
// Real native transactions for the source ACTION/cure/eternal-posture flow.
void VerifyFinishEffects(soci::session& admin,SessionPool& pool,tmapsvr::PostgreSQLMapService& source,
    const char* connection,const char* manifest,const char* routing,const char* actor,tmapsvr::MapSessionClaim primary,bool graph) {
    using namespace tmapsvr;const auto cid=std::to_string(primary.char_id);
    admin<<"UPDATE app_world.\"TCHARTABLE\" SET \"bLevel\"=60,\"dwHP\"=100,\"dwMP\"=100000 WHERE \"dwCharID\"="+cid;
    admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"dwStorageID\"=255,\"bItemID\"=13 WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=254 AND \"bItemID\"=1";
    admin<<"INSERT INTO app_world.\"TITEMTABLE\" SELECT (jsonb_populate_record(NULL::app_world.\"TITEMTABLE\",to_jsonb(i)||jsonb_build_object('dlID',9700000+\"dwOwnerID\"*100,'dwStorageID',255,'bItemID',10,'wItemID',1001))).* FROM app_world.\"TITEMTABLE\" i WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=254 AND \"bItemID\"=0";
    for(int skill:{8,240,509})admin<<"INSERT INTO app_world.\"TSKILLTABLE\" VALUES(1,"+cid+","+std::to_string(skill)+",2,0) ON CONFLICT (\"bWorldID\",\"dwCharID\",\"wSkillID\") DO UPDATE SET \"bLevel\"=2,\"dwRemainTick\"=0";
    primary.endpoint_ip=0x0100007f;primary.endpoint_port=5815;
    Check(source.ClaimSession(primary,*source.LookupSession(primary.user_id,primary.key)).has_value(),"finish-effects claims primary");
    auto live=*source.LoadAuthorized(primary);live.wMapID=0;live.fPosX=4080;live.fPosY=80;live.fPosZ=3584;source.MarkReady(primary,live);
    auto active=primary;auto* service=&source;
    const auto equip=[&](InventoryMoveRequest request){auto after=live;ApplyInventoryMove(after,PlanInventoryMove(live,request));
        live=*service->MoveInventoryItems(active,request,live,after).equipment_snapshot;};
    equip({255,10,254,1,1});const auto posture=MaintainJson(live);
    Check(live.payload->effects->size()==1&&live.payload->effects->front().skill==131,"real shield creates Defence Stance for skill flow");
    std::vector<std::byte> action;const auto add_action=[&](auto v){wire::WritePOD(action,v);};
    add_action(primary.char_id);add_action(std::uint8_t{1});add_action(std::uint8_t{2});add_action(std::uint32_t{3});add_action(std::uint32_t{4});
    add_action(primary.channel);add_action(std::uint16_t{0});add_action(std::uint16_t{240});
    auto charged=source.CommitSkillAction(primary,action,live);live=*charged.snapshot;
    Check(charged.acknowledgement[0]==std::byte{0}&&MaintainJson(live)==posture,"ACTION succeeds with source-preserved Defence Stance");
    std::unique_ptr<SyntheticReplicaPartition> partition;std::unique_ptr<PostgreSQLMapOwner> owner;std::unique_ptr<PostgreSQLMapService> target;
    if(graph) {
        partition=std::make_unique<SyntheticReplicaPartition>(admin);owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
        target=std::make_unique<PostgreSQLMapService>(pool,PostgreSQLMapConfig{1,2,owner->Token(),manifest,routing,actor});
        Check(source.AuthorizeReplicas(primary,0,4080,3584,{{0x0100007f,5815,2}}),"posture and charge authorize replica");
        auto secondary=primary;secondary.connection_id+=1000;
        Check(target->ClaimSession(secondary,*target->LookupSession(primary.user_id,primary.key)).has_value(),"posture and charge claim replica");
        secondary.role=MapSessionRole::Replica;Check(target->LoadReplica(secondary,0,4080,3584),"posture and charge load replica");target->MarkReady(secondary,live);
        live.fPosX=4100;SkillCooldownTracker timers;timers.Restore(primary.char_id,live.payload->skills,0);
        auto body=transfer::Encode(transfer::Capture(live,primary.key,timers,0));Check(source.PrepareTransfer(primary,live,body),"posture and charge prepare handoff");
        const auto received=target->AcceptTransfer(secondary,body);Check(received.has_value(),"posture and charge promote target");
        active=secondary;active.role=MapSessionRole::Primary;active.authority_epoch=received->authority_epoch;
        live=received->snapshot;service=target.get();service->MarkReady(active,live);
    }
    const auto finish=[&](std::uint16_t skill,bool empty=false){std::vector<std::byte> b;const auto add=[&](auto v){wire::WritePOD(b,v);};
        add(primary.char_id);add(primary.char_id);add(std::uint8_t{1});add(1.f);add(2.f);add(3.f);add(skill);
        add(std::uint32_t{0});add(std::uint32_t{0});add(std::uint16_t{0});add(static_cast<std::uint8_t>(!empty));
        if(!empty){add(primary.char_id);add(std::uint8_t{1});}return b;};
    auto cast=PlannedCast(active,live,240,false,true);live=*service->CommitSkillCast(active,cast.first,live,cast.second).snapshot;
    admin<<"UPDATE app_world.skill_action_operations SET started_at=clock_timestamp()-interval '1 second' WHERE char_id="+cid;
    const auto before_heal=live;auto healed=service->FinishSkill(active,finish(240),live);live=*healed.snapshot;
    Check(live.dwHP>before_heal.dwHP&&MaintainJson(live)==posture&&healed.audiences==std::vector<FinishAudience>{FinishAudience::Nearby,FinishAudience::Nearby},
        "charged cure preserves posture and original nearby DEFEND then HPMP");
    Check(Number(admin,"SELECT count(*) FROM app_world.skill_finish_operations WHERE char_id="+cid+" AND before_effects=after_effects AND jsonb_array_length(before_effects)=1 AND response_audiences='[2,2]'::jsonb")==1,
        "cure receipt binds preserved posture and exact audiences");
    // Consume all MP at cast, then equip the shield before its delayed finish.
    // This exercises Defend's own CheckEternalBuff, distinct from cast cleanup.
    {const auto& t=*std::find_if(live.payload->skill_templates.begin(),live.payload->skill_templates.end(),[](const auto& t){return t.wID==509;});
     live.dwMP=skill_engine::RequiredMP(t,live.payload->pure_max_mp,2);live.dwHP=100;}
    cast=PlannedCast(active,live,509,false,true);live=*service->CommitSkillCast(active,cast.first,live,cast.second).snapshot;
    Check(!live.dwMP&&live.payload->effects->empty(),"ordinary cast retains existing zero-MP posture removal");
    equip({254,1,255,10,1});equip({255,10,254,1,1});
    Check(!live.dwMP&&live.payload->effects->size()==1,"equipment can restore stance before a pending zero-MP finish");
    const auto prior=live;
    const auto receipt=[&]{std::string out;admin<<"SELECT to_jsonb(p)::text FROM app_world.map_checkpoints p WHERE char_id="+cid,soci::into(out);return out;};
    const auto old_receipt=receipt();
    admin<<"CREATE TRIGGER synthetic_finish_effect_fault BEFORE INSERT ON app_world.skill_finish_operations FOR EACH ROW EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{service->FinishSkill(active,finish(509),live);}),"late finish failure rolls back healing and erased posture");
    admin<<"DROP TRIGGER synthetic_finish_effect_fault ON app_world.skill_finish_operations";
    Check(receipt()==old_receipt,"late finish rollback preserves exact fresh or graph recovery receipt");
    const auto race=[&]()->std::optional<FinishSkillCommit>{try{return service->FinishSkill(active,finish(509),live);}catch(...){return {};}};
    auto one=std::async(std::launch::async,race),two=std::async(std::launch::async,race);auto a=one.get(),b=two.get();
    Check(bool(a)!=bool(b),"concurrent zero-MP finishes heal and erase posture once");
    healed=a?*a:*b;live=*healed.snapshot;
    Check(live.dwHP>prior.dwHP&&!live.dwMP&&live.payload->effects->empty(),"finished cure and eternal posture removal commit together");
    Check(healed.packets.size()==4&&healed.packets[0].first==0x52b7&&healed.packets[1].first==0x5324&&
          healed.packets[2].first==0x52a1&&healed.packets[3].first==0x52a2&&
          healed.audiences==std::vector<FinishAudience>{FinishAudience::Neighbors,FinishAudience::Owner,FinishAudience::Nearby,FinishAudience::Nearby},
          "finish preserves END neighbor, private STAT, nearby DEFEND and HPMP order");
    Check(Number(admin,"SELECT count(*) FROM app_world.skill_finish_operations WHERE char_id="+cid+" AND jsonb_array_length(before_effects)=1 AND after_effects='[]'::jsonb AND response_audiences='[1,0,2,2]'::jsonb")==1,
          "durable finish receipt records one posture removal and every reply audience");
    Check(Throws([&]{service->SaveAuthorized(active,prior);}),"stale pre-finish logout cannot resurrect erased posture");
    Check(Number(admin,"SELECT recovery_contract FROM app_world.map_checkpoints WHERE char_id="+cid)==(graph?2:3),"effect finish retains correct graph or empty fresh contract");
    Check(Number(admin,"SELECT count(*) FROM app_world.map_checkpoints WHERE char_id="+cid+" AND app_world.map_checkpoint_matches(map_checkpoints)")==1,"finished effects remain recoverable");
    service->SaveAuthorized(active,live);
}
