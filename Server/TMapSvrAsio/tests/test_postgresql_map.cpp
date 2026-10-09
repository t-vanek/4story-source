#include "services/postgresql_map_service.h"
#include "services/postgresql_map_owner.h"
#include "services/client_senders.h"
#include "services/main_transfer_codec.h"
#include "../../TLoginSvrAsio/services/postgresql_char_service.h"
#include "../../TLoginSvrAsio/services/postgresql_login_owner.h"
#include "../../TLoginSvrAsio/services/soci_auth_service.h"
#include "../../TLoginSvrAsio/services/soci_map_server_locator.h"
#include "../../TLoginSvrAsio/services/soci_session_terminator.h"
#include "../../TLoginSvrAsio/services/bcrypt_util.h"
#include <soci/soci.h>
#include <barrier>
#include <thread>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using fourstory::db::SessionPool;
using fourstory::db::Backend;
namespace login=tloginsvr::services;
namespace {
int passed=0;const char* stage="startup";
void Check(bool ok,const char* label){if(!ok)throw std::runtime_error(label);std::cout<<"PASS "<<label<<std::endl;++passed;}
template<class F> bool Throws(F&& f){try{f();return false;}catch(...){return true;}}
long long Number(soci::session& sql,const std::string& query){long long n=-1;sql<<query,soci::into(n);return n;}
constexpr auto credential="d7c9416b31ba5b02b27fe10c13ad3cbd8d9ad81d";
}
#include "replica_fixture.h"
#include "main_transfer_fixture.h"
#include "skill_checkpoint_fixture.h"
#include "skill_timing_fixture.h"
#include "skill_reagent_fixture.h"
#include "graph_reagent_fixture.h"
int main(){
    const auto* conn=std::getenv("FOURSTORY_TEST_PG_CONNINFO");
    const auto* mapconn=std::getenv("FOURSTORY_MAP_PG_CONNINFO");
    const auto* fixture=std::getenv("FOURSTORY_LOGIN_FIXTURE_CONNINFO");
    const auto* manifest=std::getenv("FOURSTORY_CATALOG_MANIFEST");
    const auto* routing=std::getenv("FOURSTORY_ROUTING_MANIFEST");
    const auto* actor=std::getenv("FOURSTORY_ACTOR_MANIFEST");
    if(!conn||!mapconn||!fixture||!manifest||!routing||!actor)return 77;
    try{
        SessionPool pool(Backend::PostgreSQL,conn,4),mpool(Backend::PostgreSQL,mapconn,4),ap(Backend::PostgreSQL,fixture,1);
        auto al=ap.Acquire();auto& admin=*al;
        stage="fixture";const auto hash=login::bcrypt_util::MakeBcryptHash(credential);
        for(int u=701;u<=722;++u){const auto name="SyntheticMap"+std::to_string(u);
            admin<<"INSERT INTO app_global.\"TACCOUNT_PW\"(\"dwUserID\",\"szUserID\",\"szPasswd\") VALUES(:u,:n,:h)",soci::use(u),soci::use(name),soci::use(hash);
            admin<<"INSERT INTO app_global.\"TUSERINFOTABLE\"(\"dwUserID\",\"bAgreement\") VALUES(:u,1)",soci::use(u);}
        admin<<"INSERT INTO app_global.\"TGROUP\"(\"bGroupID\",\"szNAME\",\"bType\") VALUES(1,'Synthetic native map',0)";
        admin<<"INSERT INTO app_world.worlds VALUES(1,0,735812)";
        admin<<"INSERT INTO app_world.routing_worlds VALUES(1,1)";
        admin<<"INSERT INTO app_global.\"TCHANNEL\"(\"bGroupID\",\"bChannel\",\"szNAME\") VALUES(1,1,'Synthetic channel')";
        admin<<"INSERT INTO app_global.\"TMACHINE\" VALUES(1,'Synthetic host',0)";
        admin<<"INSERT INTO app_global.\"TSERVER\" VALUES(1,1,4,1,5815)";
        admin<<"INSERT INTO app_global.\"TIPADDR\" VALUES(1,'127.0.0.1',1)";
        stage="services";
        login::PostgreSQLLoginOwner login_owner(conn);login::SociAuthService auth(pool,login_owner.Token());
        login::SociSessionTerminator term(pool,login_owner.Token());
        login::PostgreSQLCharService chars(pool,login_owner.Token(),manifest);
        login::SociMapServerLocator routes(pool,nullptr,login_owner.Token(),routing);
        auto owner=std::make_unique<tmapsvr::PostgreSQLMapOwner>(mapconn,1,1);
        auto config=[&]{return tmapsvr::PostgreSQLMapConfig{1,1,owner->Token(),manifest,routing,actor};};
        tmapsvr::PostgreSQLMapService map(mpool,config());
        Check(owner->Healthy(),"Map owns dedicated advisory session and persisted token");
        Check(Throws([&]{tmapsvr::PostgreSQLMapOwner duplicate(mapconn,1,1);}),"second live owner refused");
        auto wrong=config();wrong.actor_manifest=std::string(64,'0');
        Check(Throws([&]{tmapsvr::PostgreSQLMapService bad(mpool,wrong);}),"wrong actor manifest refused before use");
        auto create=[&](int user,const char* name,std::uint64_t generation){
            auto result=auth.Authenticate({"SyntheticMap"+std::to_string(user),credential,"192.0.2.50",0x2918});
            if(result.status!=login::AuthStatus::Success)throw std::runtime_error("fixture auth");
            login::CharacterCreateRequest req;req.user_id=user;req.session_key=result.session_key;req.group_id=1;req.country=4;req.name=name;
            auto c=chars.Create(req);if(c.status!=login::CreateCharResult::Success)throw std::runtime_error("fixture creation");
            auto start=routes.StartAuthorized({user,result.session_key,1,1,c.char_id});
            if(start.status!=login::StartStatus::Success||!start.endpoint||start.endpoint->server_id!=1)throw std::runtime_error("fixture route");
            return tmapsvr::MapSessionClaim{static_cast<std::uint32_t>(user),result.session_key,static_cast<std::uint32_t>(c.char_id),1,1,generation};
        };
        auto claim=[&](const tmapsvr::MapSessionClaim& c){auto candidate=map.LookupSession(c.user_id,c.key);return candidate&&map.ClaimSession(c,*candidate).has_value();};
        stage="claim";auto a=create(701,"MapHero",1);
        // Synthetic nonzero options exercise source-derived conversion without
        // misrepresenting the unmodified historical starter item values.
        admin<<"UPDATE app_world.\"TITEMTABLE\" SET \"bMagic1\"=3,\"wValue1\"=100,\"dwTime1\"=305419777,\"dwTime3\"=305402420,\"dwTime5\"=591724554,\"dwTime6\"=878073464 WHERE \"dlID\"=(SELECT min(\"dlID\") FROM app_world.\"TITEMTABLE\" WHERE \"dwOwnerID\"=:c AND \"dwStorageID\"=254)",soci::use(a.char_id,"c");
        auto altered=a;altered.char_id++;Check(!claim(altered),"foreign character cannot consume Login handoff");
        altered=a;altered.channel=2;Check(!claim(altered),"wrong channel cannot consume Login handoff");
        Check(claim(a),"native Map atomically consumes exact Login handoff");
        Check(!claim(a),"consumed handoff cannot be replayed");
        Check(Number(admin,"SELECT count(*) FROM app_global.map_handoff WHERE user_id=701")==0&&Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=701")==1,"one durable claim replaces pending handoff");
        Check(Throws([&]{map.MarkReady(a);}),"uncheckpointed admission cannot become ready");
        altered=a;altered.connection_id++;map.ReleaseSession(altered);
        Check(Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=701")==1,"stale connection generation cannot release claim");
        term.Terminate(a.user_id,a.key,login::TerminationReason::Disconnect,a.char_id);
        Check(Number(admin,"SELECT count(*) FROM app_global.\"TCURRENTUSER\" WHERE \"dwUserID\"=701")==1,
            "delayed Login disconnect preserves already claimed Map session");
        stage="load";auto snap=map.LoadAuthorized(a);
        Check(snap&&snap->payload&&!snap->payload->bags.empty()&&!snap->payload->skills.empty()&&!snap->payload->hotkeys.empty(),"actual backup-derived starter bags skills and hotkeys loaded");
        Check(snap->wMapID==0&&snap->dwMaxHP>0&&snap->dwMaxMP>0&&snap->payload->next_exp==30,"spawn fallback and source experience/stat charts hydrated");
        std::size_t items=0,magic=0;for(const auto& bag:snap->payload->bags)for(const auto& item:bag.items){++items;magic+=item.magic.size();}
        Check(items>0&&magic>0,"synthetic raw magic derives client values through backup chart");
        bool source_values=false,all_source=true;
        tmapsvr::transfer::State transfer;transfer.character=*snap;transfer.key=a.key;
        for(const auto& bag:snap->payload->bags)for(const auto& item:bag.items){
            all_source&=item.source&&item.source->id==item.dlID&&item.source->owner_id==a.char_id;
            if(item.source){transfer.items.push_back(*item.source);
                if(!item.source->magic.empty())source_values=item.source->magic[0].id==3&&item.source->magic[0].value==100&&
                    item.source->eld==305419777&&item.source->color==305402420&&item.source->companion==591724554&&item.source->texture==878073464&&
                    item.bELD==1&&item.wColor==4660&&item.wCompanion==10&&item.wCustomTex==22136;
            }
        }
        Check(all_source&&source_values,"native hydration retains raw magic and full DWORD item attributes alongside the client projection");
        const auto transferred=tmapsvr::transfer::Decode(tmapsvr::transfer::Encode(transfer));
        Check(transferred&&transferred->items.size()==items&&transferred->character.dwHP==snap->dwHP,
              "native loaded items enter the complete source transfer codec without database reload");
        Check(!map.LoadAuthorized(a),"duplicate load cannot replace resident state");
        Check(map.OwnsCell(a,snap->wMapID,snap->fPosX,snap->fPosZ)&&!map.OwnsCell(a,65000,snap->fPosX,snap->fPosZ),"main-cell decision uses pinned routing and exact claim");
        auto endpoints=map.ResolveAuthorized(a,{1,99});Check(endpoints.size()==1&&endpoints[0].ip_addr==0x0100007f,"native route resolver returns active IPv4 octets only");
        const auto info=tmapsvr::EncodeCharInfoAck(*snap,"2026-10-09 00:00:00");Check(info.size()>300,"CHARINFO includes hydrated variable lists");
        const auto children_before=Number(admin,"SELECT count(*) FROM app_world.\"TITEMTABLE\" WHERE \"dwOwnerID\"="+std::to_string(a.char_id));
        Check(Throws([&]{map.SaveAuthorized(a,*snap);}),"pre-ready save refused");map.MarkReady(a,*snap);
        Check(Throws([&]{map.ReleaseSession(a);}),"ready session cannot be released without save");
        stage="checkpoint";
        Check(Number(admin,"SELECT revision FROM app_world.map_checkpoints WHERE user_id=701")==0,"ready admission has an atomic initial checkpoint");
        snap->fPosX+=2;snap->dwEXP=3;
        map.CheckpointAuthorized(a,*snap,1);
        Check(Number(admin,"SELECT revision FROM app_world.map_checkpoints WHERE user_id=701 AND outcome='active'")==1&&
              Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=701 AND phase='ready'")==1,
              "checkpoint advances receipt while preserving the active claim");
        Check(Number(admin,"SELECT count(*) FROM app_global.\"TLOG\" WHERE \"dwUserID\"=701 AND \"timeLOGOUT\"=\"timeLOGIN\"")==1,
              "checkpoint does not log out the online character");
        Check(!Throws([&]{map.CheckpointAuthorized(a,*snap,1);}),"identical checkpoint revision confirms prior commit");
        auto conflicting=*snap;conflicting.dwEXP++;
        Check(Throws([&]{map.CheckpointAuthorized(a,conflicting,1);}),"same revision with different core is refused");
        Check(Throws([&]{map.CheckpointAuthorized(a,*snap,3);}),"checkpoint cannot skip a revision");
        Check(Throws([&]{map.CheckpointAuthorized(altered,*snap,2);}),"stale generation cannot checkpoint");
        admin<<"CREATE FUNCTION public.reject_checkpoint() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN RAISE EXCEPTION 'synthetic checkpoint fault'; END $$";
        admin<<"CREATE TRIGGER synthetic_checkpoint_fault BEFORE UPDATE ON app_world.map_checkpoints FOR EACH ROW EXECUTE FUNCTION public.reject_checkpoint()";
        Check(Throws([&]{map.CheckpointAuthorized(a,conflicting,2);}),"late receipt failure refuses checkpoint");
        Check(Number(admin,"SELECT \"dwEXP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwUserID\"=701")==3&&
              Number(admin,"SELECT revision FROM app_world.map_checkpoints WHERE user_id=701")==1,"core and receipt roll back together");
        admin<<"DROP TRIGGER synthetic_checkpoint_fault ON app_world.map_checkpoints";
        stage="save rollback";
        admin<<"CREATE FUNCTION public.reject_map_save() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN RAISE EXCEPTION 'synthetic fault'; END $$";
        admin<<"CREATE TRIGGER synthetic_save_fault BEFORE UPDATE ON app_global.\"TLOG\" FOR EACH ROW EXECUTE FUNCTION public.reject_map_save()";
        snap->fPosX+=5;snap->dwEXP+=7;
        Check(Throws([&]{map.SaveAuthorized(a,*snap);}),"logout audit failure rolls back core save and claim release");
        Check(Number(admin,"SELECT \"dwEXP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwCharID\"="+std::to_string(a.char_id))==3&&Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=701")==1,"failed save keeps original durable state and ready reservation");
        admin<<"DROP TRIGGER synthetic_save_fault ON app_global.\"TLOG\"";
        stage="save";map.SaveAuthorized(a,*snap);map.ReleaseSession(a);
        Check(Number(admin,"SELECT count(*) FROM app_global.\"TCURRENTUSER\" WHERE \"dwUserID\"=701")==0&&Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=701")==0,"successful save atomically closes current session and map claim");
        Check(Number(admin,"SELECT \"dwEXP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwCharID\"="+std::to_string(a.char_id))==10&&Number(admin,"SELECT count(*) FROM app_world.\"TITEMTABLE\" WHERE \"dwOwnerID\"="+std::to_string(a.char_id))==children_before,"core save preserves progression and every child item");
        Check(!Throws([&]{map.SaveAuthorized(a,*snap);}),"exact final receipt confirms a lost commit response without rewriting");
        snap->dwEXP++;
        Check(Throws([&]{map.SaveAuthorized(a,*snap);}),"different stale final payload cannot overwrite saved character");
        Check(Throws([&]{map.CheckpointAuthorized(a,*snap,2);}),"late checkpoint cannot recreate a logged-out claim");
        stage="fresh skills";auto skill_character=create(715,"SkillHero",15);
        VerifyFreshSkillCheckpoint(admin,map,skill_character);
        const auto skill_login=auth.Authenticate({"SyntheticMap715",credential,"192.0.2.50",0x2918});
        Check(skill_login.status==login::AuthStatus::Success&&routes.StartAuthorized({715,skill_login.session_key,1,1,static_cast<int>(skill_character.char_id)}).status==login::StartStatus::Success,
              "fresh timer logout allows authenticated relogin");
        skill_character.key=skill_login.session_key;skill_character.connection_id=115;
        Check(claim(skill_character),"fresh timer relogin claims new generation");
        const auto skill_restored=map.LoadAuthorized(skill_character);
        Check(skill_restored&&!skill_restored->payload->transfer_state&&skill_restored->payload->skills.front().dwRemainTick==4900,
              "fresh relogin hydrates exact saved skill timer and rank through ordinary database load");
        map.ReleaseSession(skill_character);
        stage="native timing";
        VerifyNativeSkillTiming(admin,map,create(717,"TimingHero",17),false);
        VerifyNativeSkillTiming(admin,map,create(718,"BrokenTimingHero",18),true);
        stage="native reagents";
        VerifyNativeReagent(admin,map,create(719,"ReagentHero",19),false);
        VerifyNativeReagent(admin,map,create(720,"ReagentRecovery",20),true);
        stage="zero HP";auto dead=create(702,"DeadHero",2);
        admin<<"UPDATE app_world.\"TCHARTABLE\" SET \"dwHP\"=0,\"dwMP\"=0 WHERE \"dwUserID\"=702";
        Check(claim(dead),"second character handoff accepted");auto ds=map.LoadAuthorized(dead);
        Check(ds&&ds->dwHP==0&&ds->dwMP==0&&ds->bDead&&ds->dwMaxHP>0,"zero stored HP means dead and zero MP stays zero");map.ReleaseSession(dead);
        stage="race";auto race=create(703,"RaceHero",3);auto other=race;other.connection_id=4;std::barrier gate(3);bool accepted[2]{};
        std::thread t1([&]{gate.arrive_and_wait();accepted[0]=claim(race);});std::thread t2([&]{gate.arrive_and_wait();accepted[1]=claim(other);});gate.arrive_and_wait();t1.join();t2.join();
        Check(accepted[0]!=accepted[1],"concurrent claims have exactly one winner");map.ReleaseSession(accepted[0]?race:other);
        stage="permissions";{auto l=mpool.Acquire();Check(Throws([&]{*l<<"UPDATE app_world.\"TITEMTABLE\" SET \"wItemID\"=0";}),"Map consumption role cannot change item templates");
            Check(Throws([&]{*l<<"DELETE FROM app_world.skill_item_consumptions";}),"Map consumption role cannot discard its audit receipts");
            Check(Throws([&]{*l<<"UPDATE app_world.\"TSKILLTABLE\" SET \"bLevel\"=2";}),"Map cooldown role cannot change learned ranks");
            Check(Throws([&]{*l<<"DELETE FROM app_world.\"TSKILLTABLE\"";}),"Map cooldown role cannot forget skills");
            Check(Throws([&]{*l<<"DELETE FROM app_world.map_checkpoints";}),"Map runtime role cannot discard recovery receipts");
            Check(Throws([&]{*l<<"SELECT 1 FROM legacy_game.\"TITEMCHART\"";}),"Map core role cannot access historical tables");}
        stage="checkpoint/final race";auto save_race=create(709,"SaveRaceHero",9);Check(claim(save_race),"checkpoint/final race fixture claimed");
        auto final_snap=map.LoadAuthorized(save_race);map.MarkReady(save_race,*final_snap);
        auto earlier=*final_snap;earlier.dwEXP=15;final_snap->dwEXP=25;
        std::barrier save_gate(3);bool final_ok=false;
        std::thread checkpoint_thread([&]{save_gate.arrive_and_wait();try{map.CheckpointAuthorized(save_race,earlier,1);}catch(...){};});
        std::thread final_thread([&]{save_gate.arrive_and_wait();try{map.SaveAuthorized(save_race,*final_snap);final_ok=true;}catch(...){};});
        save_gate.arrive_and_wait();checkpoint_thread.join();final_thread.join();
        Check(final_ok&&Number(admin,"SELECT \"dwEXP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwUserID\"=709")==25&&
              Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=709")==0,
              "concurrent checkpoint cannot overwrite or outlive final save");
        const auto relogin=auth.Authenticate({"SyntheticMap709",credential,"192.0.2.50",0x2918});
        Check(relogin.status==login::AuthStatus::Success&&
              chars.DeleteAuthorized(709,1,save_race.char_id,credential,relogin.session_key)==login::DeleteCharResult::Success&&
              Number(admin,"SELECT count(*) FROM app_world.map_checkpoints WHERE user_id=709")==0,
              "authorized low-level deletion cleans its completed checkpoint receipt");
        term.Terminate(709,relogin.session_key,login::TerminationReason::Disconnect,0);
        stage="replicas";VerifyReplicas(admin,mpool,map,mapconn,manifest,routing,actor,create(710,"ReplicaHero",10));
        stage="graph reagents";
        for(int user:{721,722}) {
            auto original=create(user,user==721?"GraphReagent":"GraphRecovery",user);
            VerifyGraphReagent(admin,mpool,map,mapconn,manifest,routing,actor,original,user==722);
            const auto login=auth.Authenticate({"SyntheticMap"+std::to_string(user),credential,"192.0.2.50",0x2918});
            Check(login.status==login::AuthStatus::Success,"graph reagent account can authenticate after save or recovery");
            Check(routes.StartAuthorized({user,login.session_key,1,1,static_cast<int>(original.char_id)}).status==login::StartStatus::Success,
                  "graph reagent character receives fresh Login handoff");
            auto resumed=original;resumed.key=login.session_key;resumed.connection_id+=100;
            Check(claim(resumed),"graph reagent relogin claims fresh epoch zero");
            auto restored=*map.LoadAuthorized(resumed);const auto remaining=tmapsvr::FindSkillReagent(restored,8412);
            Check(restored.payload->transfer_state&&(user==721?!remaining:(remaining&&remaining->bCount==2)),
                  "relogin uses consumed graph inventory instead of stale normalized item rows");
            map.MarkReady(resumed,restored);
            if(remaining) {
                auto after=restored;tmapsvr::ConsumeReagentProjection(after,*remaining);
                const auto hash=map.ConsumeSkillItem(resumed,36,*remaining,after);PublishReagentHash(after,remaining->dlID,hash);restored=after;
                Check(tmapsvr::FindSkillReagent(restored,8412)->bCount==1,"restored graph can consume at epoch zero without downgrading storage contract");
            }
            map.SaveAuthorized(resumed,restored);
        }
        stage="primary transfer";VerifyMainTransfer(admin,mpool,map,mapconn,manifest,routing,actor,create(711,"TransferHero",11));
        stage="transfer recovery";auto transferred_crash=create(712,"TransferCrash",12);
        VerifyMainTransferRecovery(admin,mpool,map,mapconn,manifest,routing,actor,transferred_crash);
        auto recovered_login=auth.Authenticate({"SyntheticMap712",credential,"192.0.2.50",0x2918});
        Check(recovered_login.status==login::AuthStatus::Success,"recovered transferred account can authenticate again");
        auto recovered_route=routes.StartAuthorized({712,recovered_login.session_key,1,1,static_cast<int>(transferred_crash.char_id)});
        Check(recovered_route.status==login::StartStatus::Success,"recovered transferred character can request a fresh Login handoff");
        auto resumed=transferred_crash;resumed.key=recovered_login.session_key;resumed.connection_id=113;
        Check(claim(resumed),"new primary claims recovered transfer under fresh key and generation");
        auto resumed_snap=map.LoadAuthorized(resumed);
        Check(resumed_snap&&resumed_snap->dwEXP==95&&resumed_snap->payload->transfer_state&&resumed_snap->payload->skills.front().dwRemainTick==299970,
              "fresh load restores recovered graph instead of stale durable skill and core rows");
        map.MarkReady(resumed,*resumed_snap);map.SaveAuthorized(resumed,*resumed_snap);
        const auto delete_transferred=auth.Authenticate({"SyntheticMap712",credential,"192.0.2.50",0x2918});
        Check(delete_transferred.status==login::AuthStatus::Success&&
              chars.DeleteAuthorized(712,1,resumed.char_id,credential,delete_transferred.session_key)==login::DeleteCharResult::Success&&
              Number(admin,"SELECT count(*) FROM app_world.map_checkpoints WHERE user_id=712")==0&&
              Number(admin,"SELECT count(*) FROM app_world.map_transfers WHERE user_id=712")==0,
              "authorized low-level deletion removes transferred character graph and journal receipts");
        term.Terminate(712,delete_transferred.session_key,login::TerminationReason::Disconnect,0);
        VerifyMainTransferRecovery(admin,mpool,map,mapconn,manifest,routing,actor,create(714,"XferReadyCrash",14),2);
        VerifyMainTransferRecovery(admin,mpool,map,mapconn,manifest,routing,actor,create(713,"XferPrepCrash",13),1);
        stage="recovery";auto safe=create(704,"SafeHero",5);Check(claim(safe),"pre-ready recovery fixture claimed");
        auto dirty=create(705,"DirtyHero",6);Check(claim(dirty),"dirty recovery fixture claimed");auto dirty_snap=map.LoadAuthorized(dirty);map.MarkReady(dirty,*dirty_snap);
        // Simulate a ready session created by pre-018 code. It must stay blocked.
        admin<<"DELETE FROM app_world.map_checkpoints WHERE user_id=705";
        auto recover=create(707,"RecoverHero",7);Check(claim(recover),"recoverable fixture claimed");
        auto recovered_snap=map.LoadAuthorized(recover);map.MarkReady(recover,*recovered_snap);
        auto recover_skills=std::make_shared<tmapsvr::CharacterPayload>(*recovered_snap->payload);
        recover_skills->skills.front().dwRemainTick=4999;recovered_snap->payload=recover_skills;
        recovered_snap->dwEXP=12;recovered_snap->fPosX+=9;map.CheckpointAuthorized(recover,*recovered_snap,1);
        recovered_snap->dwEXP=99; // uncommitted runtime tail, never persisted
        auto drift=create(708,"DriftHero",8);Check(claim(drift),"drift fixture claimed");
        auto drift_snap=map.LoadAuthorized(drift);map.MarkReady(drift,*drift_snap);
        admin<<"UPDATE app_world.\"TCHARTABLE\" SET \"dwEXP\"=42 WHERE \"dwUserID\"=708";
        auto skill_drift=create(716,"SkillDrift",16);Check(claim(skill_drift),"skill drift recovery fixture claimed");
        auto skill_drift_snap=map.LoadAuthorized(skill_drift);map.MarkReady(skill_drift,*skill_drift_snap);
        admin<<"UPDATE app_world.\"TSKILLTABLE\" SET \"dwRemainTick\"=123 WHERE \"dwCharID\"=:c",soci::use(skill_drift.char_id,"c");
        Check(Throws([&]{map.CheckpointAuthorized(skill_drift,*skill_drift_snap,1);})&&Throws([&]{map.SaveAuthorized(skill_drift,*skill_drift_snap);}),
              "durable skill drift refuses checkpoint and logout without overwriting external change");
        const int pid=owner->BackendPid();int terminated=0;admin<<"SELECT CASE WHEN pg_terminate_backend(:p) THEN 1 ELSE 0 END",soci::use(pid),soci::into(terminated);
        Check(terminated==1&&!owner->Healthy(),"owner connection loss detected");owner.reset();
        admin<<"CREATE TRIGGER synthetic_recovery_fault BEFORE UPDATE ON app_global.\"TLOG\" FOR EACH ROW EXECUTE FUNCTION public.reject_map_save()";
        Check(Throws([&]{tmapsvr::PostgreSQLMapOwner failed(mapconn,1,1);}),"recovery audit failure refuses replacement startup");
        Check(Number(admin,"SELECT count(*) FROM app_world.map_checkpoints WHERE user_id=707 AND outcome='active'")==1&&
              Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=707 AND phase='ready'")==1,
              "failed recovery retains ready claim and committed receipt");
        admin<<"DROP TRIGGER synthetic_recovery_fault ON app_global.\"TLOG\"";
        owner=std::make_unique<tmapsvr::PostgreSQLMapOwner>(mapconn,1,1);
        Check(Number(admin,"SELECT count(*) FROM app_global.\"TCURRENTUSER\" WHERE \"dwUserID\"=704")==0,"replacement owner safely closes pre-ready claim");
        Check(owner->OrphanedSessions()==3&&Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=705 AND phase='orphaned'")==1,"replacement preserves pre-checkpoint ready state as orphaned");
        Check(Throws([&]{map.SaveAuthorized(dirty,*dirty_snap);}),"old process token cannot save after ownership replacement");
        Check(owner->RecoveredSessions()==3&&Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=707")==0&&
              Number(admin,"SELECT count(*) FROM app_global.\"TCURRENTUSER\" WHERE \"dwUserID\"=707")==0,
              "replacement recovers an exact committed checkpoint and releases the account");
        Check(Number(admin,"SELECT \"dwEXP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwUserID\"=707")==12&&
              Number(admin,"SELECT count(*) FROM app_world.map_checkpoints WHERE user_id=707 AND outcome='recovered' AND recovered_at IS NOT NULL")==1,
              "recovery retains last committed core and an explicit recovery receipt");
        Check(Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=708 AND phase='orphaned'")==1,
              "core drift refuses automatic recovery");
        Check(Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=716 AND phase='orphaned'")==1,
              "skill drift refuses automatic crash recovery");
        Check(Number(admin,"SELECT skill_state->0->>2 FROM app_world.map_checkpoints WHERE user_id=707")==4999&&
              Number(admin,"SELECT CASE WHEN app_world.map_checkpoint_matches(map_checkpoints) THEN 1 ELSE 0 END FROM app_world.map_checkpoints WHERE user_id=707")==1,
              "crash recovery retains exact committed skill cooldown alongside core");
        Check(Throws([&]{map.CheckpointAuthorized(recover,*recovered_snap,2);}),"old process cannot checkpoint after recovery");
        Check(Number(admin,"SELECT count(*) FROM app_world.map_sessions WHERE user_id=713")==0&&
              Number(admin,"SELECT count(*) FROM app_world.map_transfers WHERE user_id=713 AND phase='cancelled'")==1&&
              Number(admin,"SELECT \"dwEXP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwUserID\"=713")==95&&
              Number(admin,"SELECT count(*) FROM app_world.map_checkpoints WHERE user_id=713 AND outcome='recovered' AND transfer_body IS NOT NULL")==1,
              "source replacement recovers exact frozen prepared graph and cancels its unconsumed transfer");
        Check(Number(admin,"SELECT count(*) FROM app_world.map_checkpoints WHERE user_id=720 AND outcome='recovered' AND app_world.map_checkpoint_matches(map_checkpoints)")==1&&
              Number(admin,"SELECT i.\"bCount\" FROM app_world.\"TITEMTABLE\" i JOIN app_world.\"TCHARTABLE\" c ON c.\"dwCharID\"=i.\"dwOwnerID\" WHERE c.\"dwUserID\"=720 AND i.\"wItemID\"=8412")==1,
              "process replacement preserves committed reagent decrement with recovered core and skill receipt");
        std::cout<<"Native Map integration: "<<passed<<" checks passed\n";
    }catch(const soci::soci_error&){std::cerr<<"FAIL native Map stage="<<stage<<" (backend details suppressed)\n";return 1;}
    catch(const std::exception& e){std::cerr<<"FAIL native Map stage="<<stage<<": "<<e.what()<<"\n";return 1;}
}
