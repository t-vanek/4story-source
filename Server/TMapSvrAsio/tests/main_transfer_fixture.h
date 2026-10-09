#pragma once
#include "services/main_transfer_runtime.h"
void VerifyMainTransfer(soci::session& admin,SessionPool& pool,tmapsvr::PostgreSQLMapService& source,
    const char* connection,const char* manifest,const char* routing,const char* actor,tmapsvr::MapSessionClaim primary) {
    using namespace tmapsvr;
    SyntheticReplicaPartition partition(admin);
    PostgreSQLMapOwner owner(connection,1,2);
    PostgreSQLMapService target(pool,{1,2,owner.Token(),manifest,routing,actor});
    primary.endpoint_ip=0x0100007f;primary.endpoint_port=5815;
    Check(source.ClaimSession(primary,*source.LookupSession(primary.user_id,primary.key)).has_value(),"transfer source consumes Login claim");
    auto snap=source.LoadAuthorized(primary);snap->wMapID=0;snap->fPosX=4080;snap->fPosY=80;snap->fPosZ=3584;
    Check(source.AuthorizeReplicas(primary,0,4080,3584,{{0x0100007f,5815,2}}),"transfer target receives source grant");
    auto secondary=primary;secondary.connection_id=111;
    Check(target.ClaimSession(secondary,*target.LookupSession(primary.user_id,primary.key)).has_value(),"transfer target claims its replica");
    secondary.role=MapSessionRole::Replica;
    Check(target.LoadReplica(secondary,0,4080,3584),"transfer target loads composite");
    target.MarkReady(secondary,*snap);source.MarkReady(primary,*snap);
    const auto original_exp=snap->dwEXP;
    SkillCooldownTracker timers;
    timers.Restore(primary.char_id,snap->payload->skills,1000);
    const auto skill=snap->payload->skills.front().wSkillID;
    Check(timers.TryUse(primary.char_id,skill,1000,300000),"transfer fixture arms live skill timer");
    snap->fPosX=4100;snap->dwEXP=81;snap->dwHP-=1;
    auto state=transfer::Capture(*snap,primary.key,timers,1030);
    // Typed buff retention is supported, effect/expiry simulation is not.
    // Hydration must not offer guessed speed-dependent casts for this graph.
    transfer::Buff buff;buff.skill=134;buff.level=1;buff.remaining=10000;state.buffs.push_back(buff);
    auto body=transfer::Encode(state);
    Check(source.PrepareTransfer(primary,*snap,body),"source freezes complete live graph into transfer journal");
    Check(source.PrepareTransfer(primary,*snap,body),"exact prepare retry confirms same transfer");
    Check(Throws([&]{source.CheckpointAuthorized(primary,*snap,1);}),"prepared transfer fences in-flight primary checkpoints");
    auto wrong=secondary;wrong.connection_id++;
    Check(!target.AcceptTransfer(wrong,body),"wrong target socket cannot consume transfer");
    auto altered=state;altered.character.dwEXP++;
    Check(!target.AcceptTransfer(secondary,transfer::Encode(altered)),"changed transfer body cannot consume journal");
    admin<<"CREATE TRIGGER synthetic_transfer_fault BEFORE UPDATE ON app_world.map_checkpoints FOR EACH ROW EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{target.AcceptTransfer(secondary,body);}),"late checkpoint failure aborts transfer transaction");
    Check(Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=711 AND server_id=1 AND phase='transferring' AND authority_epoch=0")==1&&
        Number(admin,"SELECT count(*) FROM app_world.map_transfers WHERE user_id=711 AND phase='prepared'")==1&&
        Number(admin,"SELECT \"dwEXP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwUserID\"=711")==original_exp,"failed transfer rolls back core ownership replicas and journal together");
    admin<<"DROP TRIGGER synthetic_transfer_fault ON app_world.map_checkpoints";
    auto received=target.AcceptTransfer(secondary,body);
    Check(received&&received->authority_epoch==1&&received->snapshot.dwEXP==81&&received->snapshot.dwHP==snap->dwHP,
        "target atomically promotes replica and restores unsaved live core");
    Check(received->snapshot.payload->skills.front().dwRemainTick==299970&&received->snapshot.payload->transfer_state&&
        transfer::Encode(*received->snapshot.payload->transfer_state)==body,"target retains exact full transfer and live remaining cooldown");
    Check(!received->snapshot.payload->statistics&&!received->snapshot.payload->skill_attack_timing&&received->snapshot.payload->transfer_state->buffs.size()==1,
          "buff-bearing transfer preserves effects but refuses guessed attack timing");
    Check(target.AcceptTransfer(secondary,body).has_value(),"exact target retry confirms previously committed load");
    Check(source.OutgoingTransferCommitted(primary),"source confirms exact outgoing authority receipt");
    auto promoted=secondary;promoted.role=MapSessionRole::Primary;promoted.authority_epoch=1;
    target.MarkReady(promoted,received->snapshot);
    Check(!target.AcceptTransfer(secondary,body),"consumed transfer cannot overwrite target after readiness");
    received->snapshot.dwEXP=82;target.CheckpointAuthorized(promoted,received->snapshot,1);
    source.SaveAuthorized(primary,*snap);
    Check(Number(admin,"SELECT \"dwEXP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwUserID\"=711")==82,
        "old source close confirmation cannot overwrite newer target core");
    received->snapshot.fPosX=4080;received->snapshot.dwEXP=83;
    SkillCooldownTracker imported;imported.Restore(primary.char_id,received->snapshot.payload->skills,2000);
    auto back=transfer::Encode(transfer::Capture(received->snapshot,primary.key,imported,2030));
    Check(target.PrepareTransfer(promoted,received->snapshot,back),"return transfer uses retained original source socket as replica");
    auto returning=primary;returning.role=MapSessionRole::Replica;
    auto returned=source.AcceptTransfer(returning,back);
    Check(returned&&returned->authority_epoch==2&&returned->snapshot.dwEXP==83,"same original socket regains primary with a new authority epoch");
    auto current=primary;current.authority_epoch=2;source.MarkReady(current,returned->snapshot);
    Check(Throws([&]{source.CheckpointAuthorized(primary,*snap,1);}),"old same-socket primary epoch cannot checkpoint after round trip");
    source.SaveAuthorized(primary,*snap);
    Check(Number(admin,"SELECT \"dwEXP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwUserID\"=711")==83,
        "old same-socket final save is read-only after round trip");
    returned->snapshot.dwEXP=84;source.CheckpointAuthorized(current,returned->snapshot,1);
    auto different=returned->snapshot;auto different_payload=std::make_shared<CharacterPayload>(*different.payload);
    different_payload->skills.front().dwRemainTick--;different.payload=different_payload;
    Check(Throws([&]{source.CheckpointAuthorized(current,different,1);}),"same checkpoint revision rejects a changed full graph even when core is equal");
    returned->snapshot.fPosX=4100;
    auto expire=transfer::Encode(transfer::Capture(returned->snapshot,primary.key,imported,2060));
    Check(source.PrepareTransfer(current,returned->snapshot,expire),"third transfer can prepare after a complete round trip");
    admin<<"UPDATE app_world.map_transfers SET expires_at=clock_timestamp()-interval '1 second' WHERE user_id=711 AND phase='prepared'";
    auto expired_target=secondary;expired_target.authority_epoch=2;
    Check(!target.AcceptTransfer(expired_target,expire),"expired prepared transfer cannot gain ownership");
    source.SaveAuthorized(current,returned->snapshot);
    Check(Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=711")==0&&
        Number(admin,"SELECT count(*) FROM app_world.map_transfers WHERE user_id=711 AND phase='consumed'")==2,
        "final primary logout releases session while retaining transfer receipts");
    Check(Number(admin,"SELECT count(*) FROM app_world.map_transfers WHERE user_id=711 AND phase='cancelled'")==1,
        "source final save cancels unconsumed transfer atomically");
}

void VerifyMainTransferRecovery(soci::session& admin,SessionPool& pool,tmapsvr::PostgreSQLMapService& source,
    const char* connection,const char* manifest,const char* routing,const char* actor,tmapsvr::MapSessionClaim primary,int stop_phase=0) {
    using namespace tmapsvr;
    const auto user=std::to_string(primary.user_id);
    SyntheticReplicaPartition partition(admin);
    auto owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
    PostgreSQLMapService target(pool,{1,2,owner->Token(),manifest,routing,actor});
    primary.endpoint_ip=0x0100007f;primary.endpoint_port=5815;
    Check(source.ClaimSession(primary,*source.LookupSession(primary.user_id,primary.key)).has_value(),"transfer crash fixture claims source");
    auto snap=source.LoadAuthorized(primary);snap->wMapID=0;snap->fPosX=4080;snap->fPosY=80;snap->fPosZ=3584;
    Check(source.AuthorizeReplicas(primary,0,4080,3584,{{0x0100007f,5815,2}}),"transfer crash fixture grants target");
    auto secondary=primary;secondary.connection_id=112;
    Check(target.ClaimSession(secondary,*target.LookupSession(primary.user_id,primary.key)).has_value(),"transfer crash target consumes replica");
    secondary.role=MapSessionRole::Replica;
    Check(target.LoadReplica(secondary,0,4080,3584),"transfer crash target loads replica");
    target.MarkReady(secondary,*snap);source.MarkReady(primary,*snap);
    SkillCooldownTracker timers;timers.TryUse(primary.char_id,snap->payload->skills.front().wSkillID,1000,300000);
    snap->fPosX=4100;snap->dwEXP=95;
    const auto body=transfer::Encode(transfer::Capture(*snap,primary.key,timers,1030));
    Check(source.PrepareTransfer(primary,*snap,body),"transfer crash source prepares graph");
    if(stop_phase==1)return; // leave durable prepared source for its owner-replacement test
    auto received=target.AcceptTransfer(secondary,body);
    Check(received&&received->snapshot.payload->skill_attack_timing&&received->snapshot.payload->skill_templates.size()==received->snapshot.payload->skills.size(),
          "ordinary transfer rebuilds pinned skill timing on its new authority");
    Check(received&&received->authority_epoch==1,"transfer crash target commits before client readiness");
    if(stop_phase==2) {
        auto promoted=secondary;promoted.role=MapSessionRole::Primary;promoted.authority_epoch=1;
        target.MarkReady(promoted,received->snapshot);
        received->snapshot.dwEXP=96;target.CheckpointAuthorized(promoted,received->snapshot,1);
    }
    owner.reset();owner=std::make_unique<PostgreSQLMapOwner>(connection,1,2);
    Check(Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id="+user)==0&&
        Number(admin,"SELECT count(*) FROM app_global.\"TCURRENTUSER\" WHERE \"dwUserID\"="+user)==0,
        "target process replacement recovers committed pre-ready transfer and releases account");
    Check(Number(admin,"SELECT count(*) FROM app_world.map_checkpoints WHERE user_id="+user+" AND outcome='recovered' AND transfer_body IS NOT NULL")==1&&
        Number(admin,"SELECT \"dwEXP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwUserID\"="+user)==(stop_phase==2?96:95),
        "transfer crash recovery retains exact graph and unsaved source progression");
    Check(Throws([&]{target.AcceptTransfer(secondary,body);}),"replaced target token cannot confirm old transfer");
}
