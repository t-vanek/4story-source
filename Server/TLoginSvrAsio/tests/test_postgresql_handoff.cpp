#include "services/postgresql_char_service.h"
#include "services/postgresql_login_owner.h"
#include "services/soci_auth_service.h"
#include "services/soci_map_server_locator.h"
#include "services/soci_session_terminator.h"
#include "services/bcrypt_util.h"
#include <soci/soci.h>
#include <barrier>
#include <thread>
#include <cstdlib>
#include <iostream>
using namespace tloginsvr::services;
using fourstory::db::SessionPool;
using fourstory::db::Backend;
namespace {
int failures=0;
void Check(bool ok,const char* label){std::cout<<(ok?"PASS ":"FAIL ")<<label<<'\n';if(!ok)++failures;}
long long Number(soci::session& sql,const std::string& query){long long n=-1;sql<<query,soci::into(n);return n;}
constexpr auto credential="d7c9416b31ba5b02b27fe10c13ad3cbd8d9ad81d";
}
int main(){
    const auto* conn=std::getenv("FOURSTORY_TEST_PG_CONNINFO");
    const auto* fixture=std::getenv("FOURSTORY_LOGIN_FIXTURE_CONNINFO");
    const auto* manifest=std::getenv("FOURSTORY_CATALOG_MANIFEST");
    const auto* routing=std::getenv("FOURSTORY_ROUTING_MANIFEST");
    if(!conn||!fixture||!manifest||!routing)return 77;
    try {
        SessionPool pool(Backend::PostgreSQL,conn,4),ap(Backend::PostgreSQL,fixture,1);
        auto al=ap.Acquire();auto& admin=*al;
        const auto hash=bcrypt_util::MakeBcryptHash(credential);
        for(int u=501;u<=514;++u){const auto name="SyntheticRoute"+std::to_string(u);
            admin<<"INSERT INTO app_global.\"TACCOUNT_PW\"(\"dwUserID\",\"szUserID\",\"szPasswd\") VALUES(:u,:n,:h)",soci::use(u),soci::use(name),soci::use(hash);
            admin<<"INSERT INTO app_global.\"TUSERINFOTABLE\"(\"dwUserID\",\"bAgreement\") VALUES(:u,1)",soci::use(u);
        }
        admin<<"INSERT INTO app_global.\"TGROUP\"(\"bGroupID\",\"szNAME\",\"bType\") VALUES(1,'Synthetic first',0),(2,'Synthetic second',0)";
        admin<<"INSERT INTO app_world.worlds VALUES(1,0,735812),(2,1,72057594037927936)";
        admin<<"INSERT INTO app_world.routing_worlds VALUES(1,1),(2,1)";
        admin<<"INSERT INTO app_global.\"TCHANNEL\"(\"bGroupID\",\"bChannel\",\"szNAME\") SELECT g,c,'Synthetic channel' FROM generate_series(1,2) g,generate_series(1,3) c";
        admin<<"INSERT INTO app_global.\"TMACHINE\" SELECT c,'Synthetic host',0 FROM generate_series(1,3) c";
        admin<<"INSERT INTO app_global.\"TSERVER\" SELECT g,c,4,c,5814+c FROM generate_series(1,2) g,generate_series(1,3) c";
        admin<<"INSERT INTO app_global.\"TIPADDR\" VALUES(1,'192.0.2.11',1),(1,'192.0.2.12',1),(2,'192.0.2.21',1),(3,'192.0.2.31',1)";
        PostgreSQLLoginOwner owner(conn);SociAuthService auth(pool,owner.Token());
        SociSessionTerminator term(pool,owner.Token());
        PostgreSQLCharService chars(pool,owner.Token(),manifest);
        SociMapServerLocator routes(pool,nullptr,owner.Token(),routing);
        auto create=[&](int user,const char* name,int world=1){
            auto login=auth.Authenticate({"SyntheticRoute"+std::to_string(user),credential,"192.0.2.50",0x2918});
            if(login.status!=AuthStatus::Success)throw std::runtime_error("fixture auth");
            CharacterCreateRequest req;req.user_id=user;req.session_key=login.session_key;req.group_id=world;req.country=4;req.name=name;
            auto c=chars.Create(req);if(c.status!=CreateCharResult::Success)throw std::runtime_error("fixture char");
            return StartRequest{user,login.session_key,static_cast<std::uint8_t>(world),1,c.char_id};
        };
        auto a=create(501,"RouteHero"),b=create(502,"SecondHero");
        const auto row=" FROM app_world.\"TCHARTABLE\" WHERE \"dwCharID\"="+std::to_string(a.char_id);
        Check(Number(admin,"SELECT \"wMapID\""+row)==2010,"backup creation starts on map 2010 before routing fallback");
        auto q=a;q.session_key++;Check(routes.StartAuthorized(q).status==StartStatus::NoServer,"wrong session key refused");
        q=a;q.char_id=0;Check(routes.StartAuthorized(q).status==StartStatus::NoServer,"zero character retains original missing-character result 1");
        q=a;q.char_id=-1;Check(routes.StartAuthorized(q).status==StartStatus::NoServer,"unsupported unsigned character sentinel is refused with result 1");
        q=a;q.char_id=b.char_id;Check(routes.StartAuthorized(q).status==StartStatus::NoServer,"other account character refused");
        q=a;q.group_id=2;Check(routes.StartAuthorized(q).status==StartStatus::NoServer,"character identity scoped to world");
        q=a;q.group_id=9;Check(routes.StartAuthorized(q).status==StartStatus::NoGroup,"unprovisioned world returns SR_NOGROUP=2");
        q=a;q.channel=8;Check(routes.StartAuthorized(q).status==StartStatus::NoGroup,"unprovisioned channel refused");
        admin<<"UPDATE app_global.\"TUSERINFOTABLE\" SET \"bAgreement\"=0 WHERE \"dwUserID\"=501";
        Check(routes.StartAuthorized(a).status==StartStatus::NoServer,"database agreement is required");
        admin<<"UPDATE app_global.\"TUSERINFOTABLE\" SET \"bAgreement\"=1 WHERE \"dwUserID\"=501";
        admin<<"UPDATE app_global.\"TCURRENTUSER\" SET \"bLocked\"=1 WHERE \"dwUserID\"=501";
        Check(routes.StartAuthorized(a).status==StartStatus::NoServer,"locked duplicate session cannot hand off");
        admin<<"UPDATE app_global.\"TCURRENTUSER\" SET \"bLocked\"=0 WHERE \"dwUserID\"=501";
        admin<<"UPDATE app_global.\"TALLCHARTABLE\" SET \"bDelete\"=1 WHERE \"dwUserID\"=501";
        Check(routes.StartAuthorized(a).status==StartStatus::NoServer,"deleted directory entry cannot enter map");
        admin<<"UPDATE app_global.\"TALLCHARTABLE\" SET \"bDelete\"=0 WHERE \"dwUserID\"=501";
        admin<<"UPDATE app_world.routing_worlds SET source_group=99 WHERE group_id=1";
        Check(routes.StartAuthorized(a).status==StartStatus::NoGroup,"unmapped cell and spawn never fall back to first server");
        admin<<"UPDATE app_world.routing_worlds SET source_group=1 WHERE group_id=1";
        admin<<"UPDATE app_global.\"TIPADDR\" SET \"bActive\"=0 WHERE \"bMachineID\"=1";
        Check(routes.StartAuthorized(a).status==StartStatus::NoServer,"inactive endpoint refused");
        admin<<"UPDATE app_global.\"TIPADDR\" SET \"bActive\"=1 WHERE \"bMachineID\"=1";
        Check(Number(admin,"SELECT \"wMapID\""+row)==2010 && Number(admin,"SELECT sum(\"bRouteID\") FROM app_global.\"TMACHINE\"")==0 && Number(admin,"SELECT count(*) FROM app_global.map_handoff")==0,"all rejected requests preserve coordinates, counters and reservation state");
        admin<<"CREATE FUNCTION app_global.synthetic_handoff_failure() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN RAISE EXCEPTION 'synthetic late failure'; END $$";
        admin<<"CREATE TRIGGER synthetic_handoff_failure BEFORE INSERT ON app_global.map_handoff FOR EACH ROW EXECUTE FUNCTION app_global.synthetic_handoff_failure()";
        Check(routes.StartAuthorized(a).status==StartStatus::Internal,"late database failure returns SR_INTERNAL=3");
        Check(Number(admin,"SELECT \"wMapID\""+row)==2010 && Number(admin,"SELECT sum(\"bRouteID\") FROM app_global.\"TMACHINE\"")==0 && Number(admin,"SELECT \"dwCharID\" FROM app_global.\"TCURRENTUSER\" WHERE \"dwUserID\"=501")==0,"late failure rolls back fallback, rotation and selected character together");
        admin<<"DROP TRIGGER synthetic_handoff_failure ON app_global.map_handoff";
        a.channel=2;auto selected=routes.StartAuthorized(a);
        Check(selected.status==StartStatus::Success && selected.endpoint && selected.endpoint->server_id==2 && selected.endpoint->port==5816 && selected.endpoint->ipv4[3]==21,"logical channel 2 resolves original owner 2, not first registered server");
        Check(Number(admin,"SELECT count(*)"+row+" AND \"wMapID\"=0 AND \"fPosY\"=0 AND \"fPosX\"=(SELECT \"fPosX\" FROM route_compat.\"TSPAWNPOSCHART\" WHERE \"wID\"=15003) AND \"fPosZ\"=(SELECT \"fPosZ\" FROM route_compat.\"TSPAWNPOSCHART\" WHERE \"wID\"=15003)")==1,"fallback persists backed-up spawn X/Z and original Y=0 behavior");
        Check(Number(admin,"SELECT count(*) FROM app_global.map_handoff h JOIN app_global.\"TCURRENTUSER\" s ON s.\"dwKEY\"=h.session_key WHERE h.user_id=501 AND s.\"dwCharID\"=h.char_id AND s.\"bGroupID\"=h.world_id AND s.\"bChannel\"=h.channel AND s.\"bLuckyNumber\" BETWEEN 0 AND 99 AND h.expires_at-h.created_at BETWEEN interval '59 seconds' AND interval '61 seconds'")==1,"pending handoff stores selected identity and bounded lucky number");
        std::string expiry;admin<<"SELECT expires_at::text FROM app_global.map_handoff WHERE user_id=501",soci::into(expiry);
        auto again=routes.StartAuthorized(a);std::string expiry_after;admin<<"SELECT expires_at::text FROM app_global.map_handoff WHERE user_id=501",soci::into(expiry_after);
        Check(again.status==StartStatus::Success && again.endpoint && again.endpoint->ipv4==selected.endpoint->ipv4 && expiry==expiry_after,"idempotent START retains endpoint and original expiry");
        q=a;q.channel=3;Check(routes.StartAuthorized(q).status==StartStatus::NoServer,"pending session cannot change selected channel");
        CharacterCreateRequest req;req.user_id=501;req.session_key=a.session_key;req.group_id=1;req.country=4;req.name="AfterHandoff";req.slot=1;
        Check(chars.Create(req).status==CreateCharResult::Internal,"post-handoff lobby cannot create another character");
        Check(chars.DeleteAuthorized(501,1,a.char_id,credential,a.session_key)==DeleteCharResult::Internal,"post-handoff lobby cannot delete selected character");
        term.Terminate(501,a.session_key,TerminationReason::Disconnect);
        Check(Number(admin,"SELECT count(*) FROM app_global.map_handoff WHERE user_id=501")==1 && Number(admin,"SELECT count(*) FROM app_global.\"TLOG\" WHERE \"dwUserID\"=501 AND \"timeLOGOUT\"=\"timeLOGIN\"")==1,"disconnect after committed START preserves pending session without premature logout");
        admin<<"UPDATE app_global.map_handoff SET expires_at=clock_timestamp()-interval '1 second' WHERE user_id=501";
        auto relogin=auth.Authenticate({"SyntheticRoute501",credential,"192.0.2.50",0x2918});
        Check(relogin.status==AuthStatus::Success && relogin.session_key!=a.session_key && Number(admin,"SELECT count(*) FROM app_global.map_handoff WHERE user_id=501")==0,"expired unclaimed handoff allows fresh login with new key");
        Check(Number(admin,"SELECT count(*) FROM app_global.\"TLOG\" WHERE \"dwUserID\"=501 AND \"timeLOGOUT\">\"timeLOGIN\"")==1,"expiry stamps only old session logout audit");
        a.session_key=relogin.session_key;a.channel=1;
        std::barrier gate(3);StartResponse ar,br;
        std::jthread t1([&]{gate.arrive_and_wait();ar=routes.StartAuthorized(a);});
        std::jthread t2([&]{gate.arrive_and_wait();br=routes.StartAuthorized(a);});gate.arrive_and_wait();t1.join();t2.join();
        Check(ar.status==StartStatus::Success && br.status==StartStatus::Success && ar.endpoint->ipv4==br.endpoint->ipv4 && Number(admin,"SELECT \"bRouteID\" FROM app_global.\"TMACHINE\" WHERE \"bMachineID\"=1")==1,"concurrent same-session START commits one rotation and identical endpoint");
        Check(!routes.Lookup(501,1,1,a.char_id),"unkeyed native API refuses routing");
        SociMapServerLocator stale(pool,nullptr,std::string(64,'0'),routing);
        Check(stale.StartAuthorized(b).status==StartStatus::Internal,"stale process token cannot create a handoff");
        bool refused=false;
        try { SociMapServerLocator wrong(pool,nullptr,owner.Token(),std::string(64,'0')); } catch (...) { refused=true; }
        Check(refused,"unpublished routing manifest is rejected at startup");
        admin<<"UPDATE reconstruction.import_runs SET status='running' WHERE id=(SELECT run_id FROM runtime_control.routing_catalog)";
        Check(routes.StartAuthorized(b).status==StartStatus::Internal,"catalog status drift refuses new handoff after startup");
        admin<<"UPDATE reconstruction.import_runs SET status='verified' WHERE id=(SELECT run_id FROM runtime_control.routing_catalog)";
        auto second=create(503,"WorldTwoHero",2);auto mapped=routes.StartAuthorized(second);
        Check(mapped.status==StartStatus::Success && mapped.endpoint->server_id==1,"explicit world 2 to source group 1 mapping routes independent world");
        auto racer=create(504,"DeleteRaceHero");std::barrier race(3);StartResponse rr;DeleteCharResult dr=DeleteCharResult::Failed;
        std::jthread rt([&]{race.arrive_and_wait();rr=routes.StartAuthorized(racer);});
        std::jthread dt([&]{race.arrive_and_wait();dr=chars.DeleteAuthorized(504,1,racer.char_id,credential,racer.session_key);});race.arrive_and_wait();rt.join();dt.join();
        Check((rr.status==StartStatus::Success && dr==DeleteCharResult::Internal)||(rr.status==StartStatus::NoServer && dr==DeleteCharResult::Success),"concurrent START versus delete has exactly one committed outcome");
        // An established non-pending Map session must survive Login recovery.
        admin<<"UPDATE app_global.\"TCURRENTUSER\" SET \"dwCharID\"=:c WHERE \"dwUserID\"=502",soci::use(b.char_id);
        admin<<"UPDATE app_global.map_handoff SET expires_at=clock_timestamp()-interval '1 second' WHERE user_id=501";
        term.ClearStaleSessions();
        Check(Number(admin,"SELECT count(*) FROM app_global.\"TCURRENTUSER\" WHERE \"dwUserID\"=501")==0 && Number(admin,"SELECT count(*) FROM app_global.\"TCURRENTUSER\" WHERE \"dwUserID\"=502")==1 && Number(admin,"SELECT count(*) FROM app_global.map_handoff WHERE user_id=503")==1,"startup removes expired pending handoff while retaining Map session and unexpired handoff");
        std::cout<<"Handoff checks complete, failures="<<failures<<'\n';return failures?1:0;
    }catch(const std::exception&){std::cerr<<"Handoff fixture failed (database diagnostics suppressed)\n";return 1;}
}
