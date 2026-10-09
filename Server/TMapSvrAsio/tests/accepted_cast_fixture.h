#pragma once
#include "services/skill_engine.h"
#include "services/skill_timing.h"
#include "wire_codec.h"

// This builder is a service-boundary fixture, not the independent wire oracle.
// Arithmetic/packet compatibility is covered separately by native TCP fixtures.
std::pair<tmapsvr::SkillCastRequest,tmapsvr::CharSnapshot> PlannedCast(
    const tmapsvr::MapSessionClaim& c,const tmapsvr::CharSnapshot& before,std::uint16_t id,bool loop=false) {
    using namespace tmapsvr;
    const auto& p=*before.payload;
    const auto& t=*std::find_if(p.skill_templates.begin(),p.skill_templates.end(),[&](const auto& t){return t.wID==id;});
    const auto& row=*std::find_if(p.skills.begin(),p.skills.end(),[&](const auto& s){return s.wSkillID==id;});
    SkillCastRequest request;request.skill=id;request.loop=loop;request.elapsed_ms=0;
    const auto add=[&](auto v){wire::WritePOD(request.request,v);};
    add(c.char_id);add(std::uint8_t{1});add(c.channel);add(before.wMapID);add(id);
    if(!loop){add(std::uint8_t{2});add(std::uint32_t{3});add(std::uint32_t{4});}
    add(1.f);add(2.f);add(3.f);add(std::uint8_t{0});
    SkillUseAckFields ack;ack.attack_id=c.char_id;ack.attack_type=1;ack.skill_id=id;
    ack.action_id=2;ack.act_id=3;ack.ani_id=4;ack.skill_level=row.bLevel;ack.attacker_level=before.bLevel;
    ack.country=before.bCountry;ack.aid_country=p.aid_country;ack.gnd_x=1;ack.gnd_y=2;ack.gnd_z=3;
    const auto& power=*t.attack_profile;
    ack.attack_level=loop||power.attack_type==1?power.physical_level:power.magic_level;
    ack.pys_min_power=power.physical_min;ack.pys_max_power=power.physical_max;
    ack.mg_min_power=power.magic_min;ack.mg_max_power=power.magic_max;ack.cp=power.critical;
    request.acknowledgement=loop?EncodeLoopSkillAck(ack,{}):EncodeSkillUseAck(ack,{});
    auto after=before;
    if(t.items==SkillItemGate::Reagent){const auto item=FindSkillReagent(before,t.wUseItem);if(!item)throw std::runtime_error("fixture reagent missing");request.debits.push_back({*item,1});ConsumeSkillItemProjection(after,request.debits);}
    after.dwMP-=skill_engine::RequiredMP(t,before.dwMaxMP,row.bLevel);after.dwHP-=skill_engine::RequiredHP(t,before.dwMaxHP,row.bLevel);
    SkillAttackTiming timing;if(t.bSpeedApply)timing=(*p.skill_attack_timing)[t.bSpeedApply-1];
    std::vector<std::uint16_t> kind;if(!loop&&t.dwKindDelay)for(const auto& k:p.skill_templates)if(k.bKind==t.bKind)kind.push_back(k.wID);
    SkillCooldownTracker timers;timers.Restore(c.char_id,p.skills,0);
    timers.TryUse(c.char_id,id,0,loop?skill_timing::LoopDelay(t,timing):skill_timing::ReuseDelay(t,row.bLevel,timing),kind,loop?0:t.dwKindDelay);
    return {request,transfer::PersistenceSnapshot(after,c.key,timers,0)};
}

void VerifyAcceptedCasts(soci::session& admin,SessionPool& pool,tmapsvr::PostgreSQLMapService& source,
    const char* connection,const char* manifest,const char* routing,const char* actor,tmapsvr::MapSessionClaim primary,bool graph) {
    using namespace tmapsvr;const auto cid=std::to_string(primary.char_id);
    admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"wItemID\"=8412,\"bCount\"=3,\"dwDuraMax\"=0,\"dwDuraCur\"=0 WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=255 AND \"bItemID\"=1";
    admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"dwStorageID\"=255,\"bItemID\"=13 WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=254 AND \"bItemID\"=1";
    admin<<"INSERT INTO app_world.\"TITEMTABLE\" SELECT (jsonb_populate_record(NULL::app_world.\"TITEMTABLE\",to_jsonb(i)||jsonb_build_object('dlID',9600000+\"dwOwnerID\"*100,'dwStorageID',255,'bItemID',10,'wItemID',1001))).* FROM app_world.\"TITEMTABLE\" i WHERE \"dwOwnerID\"="+cid+" AND \"dwStorageID\"=254 AND \"bItemID\"=0";
    for(int skill:{1,8,36,134})admin<<"INSERT INTO app_world.\"TSKILLTABLE\" VALUES(1,"+cid+","+std::to_string(skill)+",1,0) ON CONFLICT (\"bWorldID\",\"dwCharID\",\"wSkillID\") DO UPDATE SET \"bLevel\"=1,\"dwRemainTick\"=0";
    primary.endpoint_ip=0x0100007f;primary.endpoint_port=5815;
    Check(source.ClaimSession(primary,*source.LookupSession(primary.user_id,primary.key)).has_value(),"accepted cast claims native primary");
    auto live=*source.LoadAuthorized(primary);live.wMapID=0;live.fPosX=4080;live.fPosY=80;live.fPosZ=3584;source.MarkReady(primary,live);
    // A free cast before transfer proves ledger identity is recovered separately
    // from the unchanged original server transfer bytes.
    const auto before_first_cast=live;
    auto [first_req,first_after]=PlannedCast(primary,live,1);live=*source.CommitSkillCast(primary,first_req,live,first_after).snapshot;
    const auto first_head=live.payload->last_cast_id;
    auto active=primary;auto* service=&source;
    std::unique_ptr<SyntheticReplicaPartition> partition;std::unique_ptr<PostgreSQLMapOwner> owner;std::unique_ptr<PostgreSQLMapService> target;
    if(graph) {
        partition=std::make_unique<SyntheticReplicaPartition>(admin);owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
        target=std::make_unique<PostgreSQLMapService>(pool,PostgreSQLMapConfig{1,2,owner->Token(),manifest,routing,actor});
        Check(source.AuthorizeReplicas(primary,0,4080,3584,{{0x0100007f,5815,2}}),"cast fixture authorizes replica");
        auto secondary=primary;secondary.connection_id+=1000;
        Check(target->ClaimSession(secondary,*target->LookupSession(primary.user_id,primary.key)).has_value(),"cast fixture claims replica");
        secondary.role=MapSessionRole::Replica;Check(target->LoadReplica(secondary,0,4080,3584),"cast fixture loads replica");target->MarkReady(secondary,live);
        live.fPosX=4100;SkillCooldownTracker timers;timers.Restore(primary.char_id,live.payload->skills,0);
        auto stale=before_first_cast;stale.fPosX=4100;
        const auto stale_body=transfer::Encode(transfer::Capture(stale,primary.key,timers,0));
        Check(Throws([&]{source.PrepareTransfer(primary,stale,stale_body);}),"pre-cast snapshot cannot prepare a transfer that bypasses the cast ledger");
        const auto body=transfer::Encode(transfer::Capture(live,primary.key,timers,0));
        Check(source.PrepareTransfer(primary,live,body),"accepted cast graph prepares");const auto received=target->AcceptTransfer(secondary,body);
        Check(received.has_value(),"accepted cast graph promotes");active=secondary;active.role=MapSessionRole::Primary;active.authority_epoch=received->authority_epoch;
        live=received->snapshot;service=target.get();service->MarkReady(active,live);
    }
    Check(live.payload->last_cast_id==first_head&&first_head>0,"cast ledger head survives native hydration and primary handoff");
    const auto ledger=[&]{return Number(admin,"SELECT count(*) FROM app_world.accepted_skill_casts WHERE char_id="+cid);};
    const auto receipt=[&]{std::string out;admin<<"SELECT to_jsonb(p)::text FROM app_world.map_checkpoints p WHERE char_id="+cid,soci::into(out);return out;};
    auto [request,after]=PlannedCast(active,live,1);const auto unchanged=receipt();
    auto forged=request;forged.acknowledgement[22]^=std::byte{1};
    Check(Throws([&]{service->CommitSkillCast(active,forged,live,after);}),"forged outgoing cast powers fail before persistence");
    forged=request;forged.request.push_back(std::byte{0});
    Check(Throws([&]{service->CommitSkillCast(active,forged,live,after);}),"trailing raw cast request fails before persistence");
    auto stolen=after;stolen.dwGold++;
    Check(Throws([&]{service->CommitSkillCast(active,request,live,stolen);}),"cast cannot smuggle unrelated currency changes");
    stolen=after;auto p=std::make_shared<CharacterPayload>(*after.payload);p->skills.front().dwRemainTick=9000;stolen.payload=p;
    Check(Throws([&]{service->CommitSkillCast(active,request,live,stolen);}),"cast cannot forge sampled cooldowns");
    for(int field:{0,1,2}) {auto stale=active;if(field==0)stale.connection_id++;else if(field==1)stale.authority_epoch++;else stale.role=MapSessionRole::Replica;
        Check(Throws([&]{service->CommitSkillCast(stale,request,live,after);}),"cast fences stale connection epoch and replica authority");}
    Check(receipt()==unchanged&&ledger()==1,"rejected casts preserve exact receipt and ledger");
    const auto stale_snapshot=live;
    const auto race=[&]()->std::optional<SkillCastCommit>{try{return service->CommitSkillCast(active,request,live,after);}catch(...){return {};}};
    auto one=std::async(std::launch::async,race),two=std::async(std::launch::async,race);auto a=one.get(),b=two.get();
    Check(bool(a)!=bool(b),"concurrent zero-cost zero-cooldown cast snapshot commits once");live=*(a?a->snapshot:b->snapshot);
    Check(ledger()==2&&live.payload->last_cast_id>first_head,"free accepted cast advances persistent ledger chain");
    Check(Throws([&]{service->SaveAuthorized(active,stale_snapshot);}),"stale pre-cast snapshot cannot overwrite a free accepted cast");
    Check(Throws([&]{service->CheckpointAuthorized(active,stale_snapshot,1);}),"stale periodic checkpoint cannot overwrite accepted cast");
    EffectEndRequest end;end.object=active.char_id;end.object_type=1;end.attack_type=1;end.attacker=active.char_id;end.skill=131;
    Check(Throws([&]{service->EndMaintainedEffect(active,end,stale_snapshot);}),"stale effect transaction cannot overwrite accepted cast");
    auto repeat=PlannedCast(active,live,1);live=*service->CommitSkillCast(active,repeat.first,live,repeat.second).snapshot;
    Check(ledger()==3,"identical legal next free request is a distinct cast, not body deduplication");
    auto reagent=PlannedCast(active,live,36);const auto before_receipt=receipt();
    admin<<"CREATE TRIGGER synthetic_cast_fault BEFORE INSERT ON app_world.skill_item_consumptions FOR EACH ROW EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{service->CommitSkillCast(active,reagent.first,live,reagent.second);}),"late linked item receipt failure rolls back accepted cast");
    admin<<"DROP TRIGGER synthetic_cast_fault ON app_world.skill_item_consumptions";
    Check(receipt()==before_receipt&&ledger()==3,"cast failure rolls back core timers items and cast ledger together");
    live=*service->CommitSkillCast(active,reagent.first,live,reagent.second).snapshot;
    Check(FindSkillReagent(live,8412)->bCount==2&&Number(admin,"SELECT count(*) FROM app_world.skill_item_consumptions i JOIN app_world.accepted_skill_casts c ON c.cast_id=i.accepted_cast_id WHERE c.char_id="+cid)==1,"item consumption and accepted cast share one durable commit");
    auto cost=PlannedCast(active,live,134);const auto mp=live.dwMP;live=*service->CommitSkillCast(active,cost.first,live,cost.second).snapshot;
    Check(live.dwMP<mp&&Number(admin,"SELECT \"dwMP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwCharID\"="+cid)==live.dwMP,"cost-only cast persists MP immediately without periodic checkpoint");
    const InventoryMoveRequest shield{255,10,254,1,1};auto equipped=live;ApplyInventoryMove(equipped,PlanInventoryMove(live,shield));
    live=*service->MoveInventoryItems(active,shield,live,equipped).equipment_snapshot;
    Check(live.payload->effects->size()==1,"cast fixture has real equipment-created eternal posture");
    live.dwMP=0;auto loop=PlannedCast(active,live,1,true);
    auto result=service->CommitSkillCast(active,loop.first,live,loop.second);live=*result.snapshot;
    Check(result.ended.empty()&&live.payload->effects->size()==1,"original LOOPSKILL at zero MP retains eternal posture");
    auto ordinary=PlannedCast(active,live,1);const auto zero_receipt=receipt();
    admin<<"CREATE TRIGGER synthetic_cast_effect_fault BEFORE INSERT ON app_world.accepted_skill_casts FOR EACH ROW EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{service->CommitSkillCast(active,ordinary.first,live,ordinary.second);}),"cast ledger failure rolls back zero-MP posture removal");
    admin<<"DROP TRIGGER synthetic_cast_effect_fault ON app_world.accepted_skill_casts";
    Check(receipt()==zero_receipt,"failed eternal removal preserves exact recovery state");
    result=service->CommitSkillCast(active,ordinary.first,live,ordinary.second);live=*result.snapshot;
    Check(result.ended.size()==1&&result.ended[0].skill==131&&live.payload->effects->empty(),"ordinary cast at zero MP atomically removes source eternal posture");
    Check(Number(admin,"SELECT count(*) FROM app_world.accepted_skill_casts WHERE char_id="+cid+" AND before_effects<>'[]'::jsonb AND after_effects='[]'::jsonb")==1,"accepted ledger retains both effect states for zero-MP removal");
    Check(Number(admin,"SELECT count(*) FROM app_world.map_checkpoints WHERE char_id="+cid+" AND app_world.map_checkpoint_matches(map_checkpoints)")==1,"accepted cast produces a valid complete recovery checkpoint");
    service->SaveAuthorized(active,live);
}
