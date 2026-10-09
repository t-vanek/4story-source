#pragma once
#include "services/maintained_effects.h"

void VerifyEffectEnd(soci::session& admin,tmapsvr::PostgreSQLMapService& service,
    const tmapsvr::MapSessionClaim& active,tmapsvr::CharSnapshot& live,bool graph) {
    using namespace tmapsvr;const auto cid=std::to_string(active.char_id);
    // Own-PC host/map/channel are ignored by the original handler, even when
    // stale. The backup's Defence Stance is not marked bCanCancel.
    EffectEndRequest request;request.object=active.char_id;request.object_type=1;
    request.attacker=active.char_id;request.attack_type=1;request.skill=131;request.map=65535;request.channel=255;
    Check(Number(admin,"SELECT \"bCanCancel\" FROM character_compat.\"TSKILLCHART\" WHERE \"wID\"=131")==0,"original posture template does not grant UI cancellation permission");
    const auto durable=[&]{std::string result;admin<<"SELECT jsonb_build_array(to_jsonb(p),app_world.map_maintain_state(1::smallint,"+cid+"))::text FROM app_world.map_checkpoints p WHERE char_id="+cid,soci::into(result);return result;};
    auto original=live;const auto initial=durable();
    auto wrong=request;wrong.object++;
    Check(Throws([&]{service.EndMaintainedEffect(active,wrong,live);}),"foreign PC cancellation fails before persistence");
    wrong=request;wrong.object_type=2;
    Check(Throws([&]{service.EndMaintainedEffect(active,wrong,live);}),"unported non-PC cancellation fails closed");
    auto stale=active;stale.role=MapSessionRole::Replica;
    Check(Throws([&]{service.EndMaintainedEffect(stale,request,live);}),"replica service cannot mutate primary effects");
    stale=active;stale.connection_id++;
    Check(Throws([&]{service.EndMaintainedEffect(stale,request,live);}),"stale connection cannot cancel maintained effects");
    stale=active;stale.authority_epoch++;
    Check(Throws([&]{service.EndMaintainedEffect(stale,request,live);}),"stale authority epoch cannot cancel maintained effects");
    Check(durable()==initial,"rejected effect requests preserve exact recovery receipt and effects");
    for(int field:{0,1,2}) {
        wrong=request;if(field==0)wrong.attacker++;else if(field==1)wrong.attack_type++;else wrong.skill++;
        const auto result=service.EndMaintainedEffect(active,wrong,live);
        Check(!result.removed&&MaintainJson(*result.snapshot)==MaintainJson(live),"nonmatching attacker/type/skill commits ACK-only without effect removal");
        live=*result.snapshot;
    }
    const auto before_fault=durable();auto sample=live;sample.dwEXP++;
    admin<<"CREATE TRIGGER synthetic_effect_end_fault BEFORE INSERT ON app_world.maintained_effect_operations FOR EACH ROW EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{service.EndMaintainedEffect(active,request,sample);}),"late cancellation ledger failure rolls back effect and sampled core");
    admin<<"DROP TRIGGER synthetic_effect_end_fault ON app_world.maintained_effect_operations";
    Check(durable()==before_fault,"cancellation failure preserves exact native or graph receipt");
    if(!graph) {
        admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"bLevel\"=\"bLevel\"+1 WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=254 AND \"bItemID\"=0";
        Check(Throws([&]{service.EndMaintainedEffect(active,request,live);}),"changed equipment fences effect statistic recalculation");
        admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"bLevel\"=\"bLevel\"-1 WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=254 AND \"bItemID\"=0";
    }
    const auto race=[&]()->std::optional<EffectEndCommit>{try{return service.EndMaintainedEffect(active,request,live);}catch(...){return {};}};
    auto first=std::async(std::launch::async,race),second=std::async(std::launch::async,race);auto a=first.get(),b=second.get();
    Check(bool(a)!=bool(b),"concurrent cancellation of the same snapshot commits once");
    const auto result=a?*a:*b;live=*result.snapshot;
    Check(result.removed&&live.payload->effects->empty()&&original.payload->effects->size()==1,"cancellation removes exactly one effect and preserves original immutable snapshot");
    Check(live.payload->statistics->physical_defense<original.payload->statistics->physical_defense&&live.payload->statistics->magic_defense<original.payload->statistics->magic_defense,"cancellation rederives original defense statistics");
    Check(Throws([&]{service.EndMaintainedEffect(active,request,original);}),"old effect snapshot cannot replay after removal");
    const auto repeated=service.EndMaintainedEffect(active,request,live);
    Check(!repeated.removed&&repeated.snapshot->payload->effects->empty(),"repeated client cancellation commits legitimate absent-match ACK");
    live=*repeated.snapshot;
    Check(Number(admin,"SELECT count(*) FROM app_world.maintained_effect_operations WHERE char_id="+cid+" AND removed=1")==1,"one durable effect removal survives the cancellation race");
    Check(Number(admin,"SELECT recovery_contract FROM app_world.map_checkpoints WHERE char_id="+cid)==(graph?2:3),"last explicit cancellation returns to the correct empty recovery contract");
    Check(Number(admin,"SELECT count(*) FROM app_world.maintained_effect_operations WHERE char_id="+cid+" AND state_contract="+(graph?"2":"3")+" AND octet_length(request)=19")==5,"effect ledger records exact source request width and storage authority");
}

void VerifyPostures(soci::session& admin,SessionPool& pool,tmapsvr::PostgreSQLMapService& source,
    const char* connection,const char* manifest,const char* routing,const char* actor,tmapsvr::MapSessionClaim primary,bool graph) {
    using namespace tmapsvr;
    const auto cid=std::to_string(primary.char_id);
    admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"dwStorageID\"=255,\"bItemID\"=13 WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=254 AND \"bItemID\"=1";
    for(const auto [slot,item]:{std::pair{10,1001},std::pair{11,401}})
        admin<<"INSERT INTO app_world.\"TITEMTABLE\" SELECT (jsonb_populate_record(NULL::app_world.\"TITEMTABLE\",to_jsonb(i)||jsonb_build_object('dlID',9500000+\"dwOwnerID\"*100+"+std::to_string(slot)+",'dwStorageID',255,'bItemID',"+std::to_string(slot)+",'wItemID',"+std::to_string(item)+"))).* FROM app_world.\"TITEMTABLE\" i WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=254 AND \"bItemID\"=0";
    for(int skill:{8,14})admin<<"INSERT INTO app_world.\"TSKILLTABLE\"(\"bWorldID\",\"dwCharID\",\"wSkillID\",\"bLevel\",\"dwRemainTick\") VALUES(1,"+cid+","+std::to_string(skill)+",1,0) ON CONFLICT DO NOTHING";
    primary.endpoint_ip=0x0100007f;primary.endpoint_port=5815;
    Check(source.ClaimSession(primary,*source.LookupSession(primary.user_id,primary.key)).has_value(),"posture fixture claims native handoff");
    auto live=*source.LoadAuthorized(primary);live.wMapID=0;live.fPosX=4080;live.fPosY=80;live.fPosZ=3584;
    source.MarkReady(primary,live);auto* service=&source;auto active=primary;
    const auto rows=[&]{std::string out;admin<<"SELECT COALESCE(jsonb_agg(to_jsonb(i) ORDER BY \"dlID\"),'[]')::text FROM app_world.\"TITEMTABLE\" i WHERE \"dwOwnerID\"="+cid,soci::into(out);return out;};
    const auto effects=[&]{std::string out;admin<<"SELECT app_world.map_maintain_state(1::smallint,"+cid+")::text",soci::into(out);return out;};
    std::unique_ptr<SyntheticReplicaPartition> partition;
    std::unique_ptr<PostgreSQLMapOwner> owner;
    std::unique_ptr<PostgreSQLMapService> target;
    if(graph) {
        partition=std::make_unique<SyntheticReplicaPartition>(admin);owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
        target=std::make_unique<PostgreSQLMapService>(pool,PostgreSQLMapConfig{1,2,owner->Token(),manifest,routing,actor});
        Check(source.AuthorizeReplicas(primary,0,4080,3584,{{0x0100007f,5815,2}}),"posture replica grant");
        auto secondary=primary;secondary.connection_id+=1000;
        Check(target->ClaimSession(secondary,*target->LookupSession(primary.user_id,primary.key)).has_value(),"posture replica claimed");
        secondary.role=MapSessionRole::Replica;Check(target->LoadReplica(secondary,0,4080,3584),"posture replica loaded");target->MarkReady(secondary,live);
        live.fPosX=4100;SkillCooldownTracker timers;timers.Restore(primary.char_id,live.payload->skills,0);
        const auto body=transfer::Encode(transfer::Capture(live,primary.key,timers,0));
        Check(source.PrepareTransfer(primary,live,body),"posture graph prepared");auto received=target->AcceptTransfer(secondary,body);
        Check(received.has_value(),"posture graph promoted");active=secondary;active.role=MapSessionRole::Primary;active.authority_epoch=received->authority_epoch;
        live=received->snapshot;service=target.get();service->MarkReady(active,live);
    }
    const auto normalized=rows();
    const auto preview=[&](InventoryMoveRequest r){auto s=live;ApplyInventoryMove(s,PlanInventoryMove(s,r));return s;};
    const auto apply=[&](InventoryMoveRequest r){auto next=preview(r);auto result=service->MoveInventoryItems(active,r,live,next);
        Check(bool(result.equipment_snapshot),"posture equipment commits a complete snapshot");live=*result.equipment_snapshot;return result;};
    const InventoryMoveRequest shield{255,10,254,1,1};const auto initial=live;
    auto expected=preview(shield);
    admin<<"CREATE TRIGGER synthetic_posture_fault BEFORE INSERT ON app_world.equipment_operations FOR EACH ROW EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{service->MoveInventoryItems(active,shield,live,expected);}),"late equipment receipt failure rolls back newly created posture");
    admin<<"DROP TRIGGER synthetic_posture_fault ON app_world.equipment_operations";
    Check(rows()==normalized&&effects()=="[]","posture rollback preserves items and native effects");
    const auto race=[&]()->std::optional<InventoryMoveCommit>{try{return service->MoveInventoryItems(active,shield,live,expected);}catch(...){return {};}};
    auto one=std::async(std::launch::async,race),two=std::async(std::launch::async,race);auto a=one.get(),b=two.get();
    Check(bool(a)!=bool(b),"concurrent shield equips have exactly one posture commit");
    auto result=a?*a:*b;live=*result.equipment_snapshot;
    Check(live.payload->effects->size()==1&&live.payload->effects->front().skill==131&&result.effects_before.size()==1&&result.effects_before[0].added&&result.effects_after.empty(),"shield creates one permanent Defence Stance before EQUIP");
    const auto& e=live.payload->effects->front();
    Check(e.level==1&&!e.remaining&&e.attack_id==primary.char_id&&e.host_id==primary.char_id&&e.attack_type==1&&e.host_type==1&&e.hit==0&&e.attack_level==0&&e.position[0]==live.fPosX,"ForceMaintain runtime fields match original constructor overrides");
    Check(live.payload->statistics->physical_defense>initial.payload->statistics->physical_defense&&live.payload->statistics->magic_defense>initial.payload->statistics->magic_defense,"shield posture changes both source-derived defenses");
    Check(Number(admin,"SELECT recovery_contract FROM app_world.map_checkpoints WHERE char_id="+cid)==(graph?2:4),"postures use native v4 or authoritative graph v2 recovery");
    if(!graph) {
        auto forged=live;auto p=std::make_shared<CharacterPayload>(*live.payload);p->effects->clear();forged.payload=p;
        Check(Throws([&]{service->CheckpointAuthorized(active,forged,1);}),"core checkpoint cannot silently remove a maintained effect");
        Check(Throws([&]{service->SaveAuthorized(active,forged);}),"logout cannot silently remove a maintained effect");
        admin<<"UPDATE app_world.map_maintained_effects SET attack_id=attack_id+1 WHERE char_id="+cid;
        Check(Number(admin,"SELECT CASE WHEN app_world.map_checkpoint_matches(p) THEN 1 ELSE 0 END FROM app_world.map_checkpoints p WHERE char_id="+cid)==0,"native recovery detects maintained-effect drift");
        Check(Throws([&]{service->MoveInventoryItems(active,{254,1,255,10,1},live,preview({254,1,255,10,1}));}),"effect drift fences equipment mutation");
        admin<<"UPDATE app_world.map_maintained_effects SET attack_id=attack_id-1 WHERE char_id="+cid;
    }
    VerifyEffectEnd(admin,*service,active,live,graph);
    apply({254,1,255,10,1});apply(shield);
    service->CheckpointAuthorized(active,live,1);
    const auto old=live;result=apply({254,1,255,10,1});
    Check(live.payload->effects->empty()&&result.equipment_display->payload->effects->size()==1&&result.effects_after.size()==1&&!result.effects_after[0].added,"unequip ends posture after intermediate equipment sheet and HPMP");
    Check(old.payload->effects->size()==1&&result.equipment_display->payload->statistics->physical_defense>live.payload->statistics->physical_defense,"immutable intermediate sheet retains the removed posture");
    Check(Throws([&]{auto stale=old;const InventoryMoveRequest r{254,1,255,10,1};ApplyInventoryMove(stale,PlanInventoryMove(old,r));service->MoveInventoryItems(active,r,old,stale);}),"stale posture transaction cannot replay after cancellation");
    apply(shield);
    result=apply({255,11,254,0,1});
    Check(live.payload->effects->size()==1&&live.payload->effects->front().skill==132&&result.effects_before.size()==2&&!result.effects_before[0].added&&result.effects_before[0].skill==131&&result.effects_before[1].added,"two-hand swap replaces defense with attack posture in original order");
    Check(result.effects_before[0].state->payload->effects->empty()&&result.effects_before[1].state->payload->effects->size()==1,"posture replacement preserves each distinct intermediate state");
    Check(Number(admin,"SELECT count(*) FROM app_world.equipment_operations WHERE char_id="+cid+" AND before_effects IS NOT NULL AND after_effects IS NOT NULL")>=4,"equipment receipts bind before and after maintained state");
    if(graph) {
        Check(rows()==normalized&&effects()=="[]","graph posture transactions leave stale normalized children untouched");
        SkillCooldownTracker timers;timers.Restore(primary.char_id,live.payload->skills,0);
        const auto graph_before=transfer::Encode(transfer::Capture(live,primary.key,timers,0));
        owner.reset();owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
        Check(owner->RecoveredSessions()==1&&transfer::Encode(StoredReagentGraph(admin,primary.char_id))==graph_before,"replacement owner recovers exact committed posture graph before next sweep");
    }else service->SaveAuthorized(active,live);
}
