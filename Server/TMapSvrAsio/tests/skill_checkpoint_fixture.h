#pragma once
#include "services/main_transfer_runtime.h"
namespace {
void VerifyFreshSkillCheckpoint(soci::session& admin,tmapsvr::PostgreSQLMapService& map,const tmapsvr::MapSessionClaim& c) {
    using namespace tmapsvr;
    auto candidate=map.LookupSession(c.user_id,c.key);
    Check(candidate&&map.ClaimSession(c,*candidate).has_value(),"fresh skill fixture claims native primary");
    auto loaded=map.LoadAuthorized(c);
    SkillCooldownTracker timers;
    const auto ordinary=loaded->payload->skills.front().wSkillID;
    timers.TryUse(c.char_id,ordinary,100,500);
    auto ready=transfer::PersistenceSnapshot(*loaded,c.key,timers,225);
    map.MarkReady(c,ready);
    Check(Number(admin,"SELECT \"dwRemainTick\" FROM app_world.\"TSKILLTABLE\" WHERE \"dwCharID\"="+std::to_string(c.char_id)+
                       " AND \"wSkillID\"="+std::to_string(ordinary))==375&&
          Number(admin,"SELECT count(*) FROM app_world.map_checkpoints WHERE char_id="+std::to_string(c.char_id)+
                       " AND revision=0 AND recovery_contract=3 AND app_world.map_checkpoint_matches(map_checkpoints)")==1,
          "initial checkpoint commits current skill duration with an exact revision-zero receipt");
    timers.Restore(c.char_id,loaded->payload->skills,100);
    Check(timers.TryUse(c.char_id,ordinary,100,0xffffffffU),"new runtime skill cooldown arms without transfer");
    auto sampled=transfer::PersistenceSnapshot(*loaded,c.key,timers,125);
    Check(!sampled.payload->transfer_state&&sampled.payload!=loaded->payload&&loaded->payload->skills.front().dwRemainTick==0&&
          sampled.payload->skills.front().dwRemainTick==0xffffffe6,
          "fresh snapshot samples live timers without mutating loaded payload or fabricating transfer graph");
    map.CheckpointAuthorized(c,sampled,1);
    const auto where=" WHERE \"dwCharID\"="+std::to_string(c.char_id);
    Check(Number(admin,"SELECT \"dwRemainTick\" FROM app_world.\"TSKILLTABLE\""+where+" AND \"wSkillID\"="+std::to_string(ordinary))==-26&&
          Number(admin,"SELECT count(*) FROM app_world.map_checkpoints WHERE char_id="+std::to_string(c.char_id)+
                       " AND recovery_contract=3 AND skill_state IS NOT NULL AND transfer_body IS NULL")==1,
          "native checkpoint preserves learned rank and full DWORD remaining bits");
    auto changed=transfer::PersistenceSnapshot(*loaded,c.key,timers,150);
    Check(Throws([&]{map.CheckpointAuthorized(c,changed,1);}),"same core and revision with changed fresh timers is refused");
    auto reordered=sampled;auto p=std::make_shared<CharacterPayload>(*sampled.payload);std::reverse(p->skills.begin(),p->skills.end());reordered.payload=p;
    Check(!Throws([&]{map.CheckpointAuthorized(c,reordered,1);}),"reordered identical skills confirm exact checkpoint commit");
    changed.dwEXP=17;
    admin<<"CREATE TRIGGER synthetic_skill_receipt_fault BEFORE UPDATE ON app_world.map_checkpoints FOR EACH ROW EXECUTE FUNCTION public.reject_checkpoint()";
    Check(Throws([&]{map.CheckpointAuthorized(c,changed,2);}),"receipt failure rejects fresh timer checkpoint");
    admin<<"DROP TRIGGER synthetic_skill_receipt_fault ON app_world.map_checkpoints";
    Check(Number(admin,"SELECT \"dwEXP\" FROM app_world.\"TCHARTABLE\""+where)==loaded->dwEXP&&
          Number(admin,"SELECT \"dwRemainTick\" FROM app_world.\"TSKILLTABLE\""+where+" AND \"wSkillID\"="+std::to_string(ordinary))==-26,
          "failed fresh checkpoint rolls core and timers back atomically");
    for(int mutation=0;mutation<3;++mutation) {
        auto invalid=sampled;auto bad=std::make_shared<CharacterPayload>(*sampled.payload);
        if(mutation==0)++bad->skills.front().bLevel;
        if(mutation==1)bad->skills.pop_back();
        if(mutation==2)bad->skills.push_back(bad->skills.front());
        invalid.payload=bad;
        Check(Throws([&]{map.CheckpointAuthorized(c,invalid,2);}),"core save refuses changed rank missing or duplicate learned skill");
    }
    auto expired=transfer::PersistenceSnapshot(*loaded,c.key,timers,4294967395ULL);map.CheckpointAuthorized(c,expired,2);
    Check(expired.payload->skills.front().dwRemainTick==0&&
          Number(admin,"SELECT \"dwRemainTick\" FROM app_world.\"TSKILLTABLE\""+where+" AND \"wSkillID\"="+std::to_string(ordinary))==0,
          "expired runtime cooldown persists zero instead of rearming loaded value");
    Check(timers.TryUse(c.char_id,ordinary,4294967395ULL,5000),"expired skill can arm a new runtime cooldown");
    auto final=transfer::PersistenceSnapshot(*loaded,c.key,timers,4294967495ULL);
    admin<<"CREATE TRIGGER synthetic_skill_audit_fault BEFORE UPDATE ON app_global.\"TLOG\" FOR EACH ROW EXECUTE FUNCTION public.reject_map_save()";
    Check(Throws([&]{map.SaveAuthorized(c,final);}),"late logout audit failure rolls back fresh cooldown save");
    admin<<"DROP TRIGGER synthetic_skill_audit_fault ON app_global.\"TLOG\"";
    Check(Number(admin,"SELECT \"dwRemainTick\" FROM app_world.\"TSKILLTABLE\""+where+" AND \"wSkillID\"="+std::to_string(ordinary))==0,
          "failed final save retains previous skill checkpoint");
    map.SaveAuthorized(c,final);
    Check(!Throws([&]{map.SaveAuthorized(c,final);})&&Throws([&]{map.SaveAuthorized(c,expired);}),
          "lost final response confirms exact timers and refuses stale remaining duration");
    Check(Number(admin,"SELECT \"dwRemainTick\" FROM app_world.\"TSKILLTABLE\""+where+" AND \"wSkillID\"="+std::to_string(ordinary))==4900,
          "fresh final save commits sampled timers and closes primary");
    timers.TryUse(c.char_id,65534,2000,1000);
    Check(Throws([&]{(void)transfer::PersistenceSnapshot(*loaded,c.key,timers,2000);}),"unknown active timer cannot be dropped during fresh persistence");
}
}
