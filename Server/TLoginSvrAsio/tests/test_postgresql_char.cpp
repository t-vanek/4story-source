#include "services/postgresql_char_service.h"
#include "services/postgresql_login_owner.h"
#include "services/soci_auth_service.h"
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
long long Number(soci::session& sql,const std::string& q){long long n=-1;sql<<q,soci::into(n);return n;}
constexpr auto credential="d7c9416b31ba5b02b27fe10c13ad3cbd8d9ad81d";
}
int main(){
    const auto* conn=std::getenv("FOURSTORY_TEST_PG_CONNINFO");
    const auto* fixture=std::getenv("FOURSTORY_LOGIN_FIXTURE_CONNINFO");
    const auto* manifest=std::getenv("FOURSTORY_CATALOG_MANIFEST");
    if(!conn||!fixture||!manifest)return 77;
    try {
        SessionPool pool(Backend::PostgreSQL,conn,4),admin_pool(Backend::PostgreSQL,fixture,1);
        auto admin_lease=admin_pool.Acquire();auto& admin=*admin_lease;
        const auto hash=bcrypt_util::MakeBcryptHash(credential);
        for(int u=301;u<=311;++u){const auto name="SyntheticChar"+std::to_string(u);
            admin<<"INSERT INTO app_global.\"TACCOUNT_PW\"(\"dwUserID\",\"szUserID\",\"szPasswd\") VALUES(:u,:name,:hash)",soci::use(u),soci::use(name),soci::use(hash);
            admin<<"INSERT INTO app_global.\"TUSERINFOTABLE\"(\"dwUserID\",\"bAgreement\") VALUES(:u,1)",soci::use(u);
        }
        admin<<"INSERT INTO app_global.\"TGROUP\"(\"bGroupID\",\"szNAME\",\"bType\",\"bStatus\",\"wBusy\",\"wFull\",\"dwMaxUser\") VALUES(1,'Synthetic normal',0,0,0,0,100),(2,'Synthetic second',0,0,0,0,100)";
        admin<<"INSERT INTO app_world.worlds VALUES(1,0,735812),(2,1,72057594037927936)";
        PostgreSQLLoginOwner owner(conn);SociAuthService auth(pool,owner.Token());
        PostgreSQLCharService chars(pool,owner.Token(),manifest);
        auto login=auth.Authenticate({"SyntheticChar301",credential,"192.0.2.31",0x2918});
        Check(login.status==AuthStatus::Success,"real native authentication supplies character session");
        CharacterCreateRequest req;req.user_id=301;req.session_key=login.session_key;req.group_id=1;req.country=4;req.name="SourceHero";
        auto created=chars.Create(req);
        Check(created.status==CreateCharResult::Success && created.char_id>0 && created.create_count==1 && created.starting_level==1,"source starter character created atomically");
        if(created.status!=CreateCharResult::Success)return 1;
        auto rows=chars.List(301,1);
        Check(rows.size()==1 && rows[0].char_id==created.char_id && rows[0].start_act==1 && !rows[0].items.empty(),"lobby reads persisted source start action and equipped items");
        Check(chars.List(302,1).empty() && chars.List(301,2).empty(),"listing scopes both account and world");
        Check(Number(admin,"SELECT \"dwHP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwUserID\"=301")==169 && Number(admin,"SELECT \"dwMP\" FROM app_world.\"TCHARTABLE\" WHERE \"dwUserID\"=301")==163,"HP/MP match backed-up class/race formula (169/163)");
        Check(Number(admin,"SELECT count(*) FROM app_world.\"TITEMTABLE\"")==10 && Number(admin,"SELECT min(\"dlID\") FROM app_world.\"TITEMTABLE\"")==735813,"ten backed-up starters allocated above original world-zero high-water");
        Check(Number(admin,"SELECT count(*) FROM app_world.\"TINVENTABLE\" WHERE \"dEndTime\"='1900-01-01'")==2 && Number(admin,"SELECT count(*) FROM app_world.\"THOTKEYTABLE\"")==2 && Number(admin,"SELECT count(*) FROM app_world.\"TSKILLTABLE\"")>0,"inventories preserve zero-date sentinel; skills and hotkeys persist");
        Check(Number(admin,"SELECT \"dwGold\" FROM app_world.\"TPOSTTABLE\"")==9999 && Number(admin,"SELECT octet_length(\"szMessage\") FROM app_world.\"TPOSTTABLE\"")==109,"welcome mail preserves original encoded message and gold");
        auto vet=chars.GetVeteranLevels();Check(vet.first==19&&vet.second==29&&vet.third==93,"veteran levels are original global catalog 19/29/93");
        req.name="sourcehero";req.slot=1;Check(chars.Create(req).status==CreateCharResult::DuplicateName,"case-insensitive directory blocks duplicate names");
        req.name="SlotConflict";req.slot=0;Check(chars.Create(req).status==CreateCharResult::InvalidSlot,"occupied live slot rejected");
        req.name="StaleSession";req.slot=1;req.session_key++;Check(chars.Create(req).status==CreateCharResult::Internal,"stale session key cannot create");req.session_key--;
        req.group_id=9;Check(chars.Create(req).status==CreateCharResult::NoGroup,"unprovisioned world rejected");req.group_id=1;
        const auto before=Number(admin,"SELECT item_high_water FROM app_world.worlds WHERE group_id=1");
        admin<<"CREATE FUNCTION app_world.synthetic_failure() RETURNS trigger LANGUAGE plpgsql AS $$ BEGIN RAISE EXCEPTION 'synthetic late failure'; END $$";
        admin<<"CREATE TRIGGER synthetic_failure BEFORE INSERT ON app_world.\"TPOSTTABLE\" FOR EACH ROW EXECUTE FUNCTION app_world.synthetic_failure()";
        req.name="RollbackHero";Check(chars.Create(req).status==CreateCharResult::Internal,"late mail failure refuses success");
        Check(Number(admin,"SELECT count(*) FROM app_world.\"TCHARTABLE\"")==1 && Number(admin,"SELECT count(*) FROM app_global.\"TALLCHARTABLE\"")==1 && Number(admin,"SELECT item_high_water FROM app_world.worlds WHERE group_id=1")==before && Number(admin,"SELECT count(*) FROM app_world.\"TITEMTABLE\"")==10,"late failure rolls back character, directory, items and allocator together");
        admin<<"DROP TRIGGER synthetic_failure ON app_world.\"TPOSTTABLE\"";
        req.name="Summoner";req.char_class=5;auto summon=chars.Create(req);Check(summon.status==CreateCharResult::Success,"summoner source path creates successfully");
        Check(Number(admin,"SELECT count(*) FROM app_world.\"TRECALLMONTABLE\" WHERE \"wMonID\"=770 AND \"dwATTR\"=66537 AND \"dwHP\"=413 AND \"dwMP\"=371 AND \"wPosX\"=3664")==1,"recall uses backed-up summon attributes and SQL integer coordinate conversion");
        req.slot=2;req.name="VeteranHero";req.level_option=1;auto veteran=chars.Create(req);Check(veteran.status==CreateCharResult::Success&&veteran.starting_level==19&&veteran.create_count==3,"veteran creation returns original level and live count");
        Check(chars.DeleteAuthorized(301,1,created.char_id,"wrong",login.session_key)==DeleteCharResult::InvalidPassword && static_cast<int>(DeleteCharResult::InvalidPassword)==1,"wrong delete credential returns original DR_INVALIDPASSWD=1");
        admin<<"INSERT INTO app_world.guild_membership VALUES(1,:id,7,12,3)",soci::use(created.char_id);
        Check(chars.DeleteAuthorized(301,1,created.char_id,credential,login.session_key)==DeleteCharResult::Failed&&static_cast<int>(DeleteCharResult::Failed)==4,"guild membership blocks with original DR_GUILD=4");
        admin<<"DELETE FROM app_world.guild_membership";
        Check(chars.DeleteAuthorized(301,2,created.char_id,credential,login.session_key)==DeleteCharResult::Failed,"wrong world cannot delete owned character");
        Check(chars.DeleteAuthorized(301,1,created.char_id,credential,login.session_key)==DeleteCharResult::Success,"low-level deletion commits");
        Check(Number(admin,"SELECT count(*) FROM app_world.\"TITEMTABLE\" WHERE \"dwOwnerID\"="+std::to_string(created.char_id))==0 && Number(admin,"SELECT count(*) FROM app_global.\"TALLCHARTABLE\" WHERE \"dwCharID\"="+std::to_string(created.char_id))==0 && Number(admin,"SELECT count(*) FROM app_world.\"TTITLETABLE\" WHERE \"dwCharID\"="+std::to_string(created.char_id))==0,"hard delete cleans children and releases global name");
        Check(chars.DeleteAuthorized(301,1,veteran.char_id,credential,login.session_key)==DeleteCharResult::Success,"high-level deletion commits");
        req.name="VeteranHero";req.slot=3;Check(chars.Create(req).status==CreateCharResult::DuplicateName,"soft-deleted veteran still reserves name");
        auto a=auth.Authenticate({"SyntheticChar302",credential,"192.0.2.32",0x2918});
        CharacterCreateRequest first=req;first.user_id=302;first.session_key=a.session_key;first.level_option=0;first.char_class=0;first.name="RacingHero";first.slot=0;
        auto b=auth.Authenticate({"SyntheticChar303",credential,"192.0.2.33",0x2918});auto second=first;second.user_id=303;second.session_key=b.session_key;
        std::barrier gate(3);CharacterCreateResponse ar,br;
        std::jthread t1([&]{gate.arrive_and_wait();ar=chars.Create(first);});std::jthread t2([&]{gate.arrive_and_wait();br=chars.Create(second);});gate.arrive_and_wait();t1.join();t2.join();
        Check((ar.status==CreateCharResult::Success&&br.status==CreateCharResult::DuplicateName)||(br.status==CreateCharResult::Success&&ar.status==CreateCharResult::DuplicateName),"concurrent cross-account name claim has exactly one winner");
        first.name="SameSlotFirst";first.slot=1;second=first;second.name="SameSlotSecond";
        std::barrier slots(3);std::jthread t3([&]{slots.arrive_and_wait();ar=chars.Create(first);});std::jthread t4([&]{slots.arrive_and_wait();br=chars.Create(second);});slots.arrive_and_wait();t3.join();t4.join();
        Check((ar.status==CreateCharResult::Success&&br.status==CreateCharResult::InvalidSlot)||(br.status==CreateCharResult::Success&&ar.status==CreateCharResult::InvalidSlot),"concurrent same-account slot claim serializes with safe retry");
        CharacterCreateRequest extra=first;extra.slot=0;extra.group_id=2;extra.name="SecondWorld";
        auto other_world=chars.Create(extra);Check(other_world.status==CreateCharResult::Success,"same account slot in another world is independent");
        Check(Number(admin,"SELECT min(\"dlID\") FROM app_world.\"TITEMTABLE\" WHERE \"bWorldID\"=2")==72057594037927937LL,"second world item IDs use its separate 56-bit range");
        Check(Number(admin,"SELECT count(*) FROM app_world.\"TITEMTABLE\"")==Number(admin,"SELECT count(DISTINCT \"dlID\") FROM app_world.\"TITEMTABLE\""),"all concurrent allocations have globally unique item IDs");
        PostgreSQLCharService stale(pool,std::string(64,'0'),manifest);first.name="FencedHero";first.slot=4;
        Check(stale.Create(first).status==CreateCharResult::Internal,"old process token cannot mutate characters");
        bool bad=false;try{PostgreSQLCharService wrong(pool,owner.Token(),std::string(64,'0'));}catch(...){bad=true;}Check(bad,"wrong manifest fails before service startup");
        std::cout<<"Character checks complete, failures="<<failures<<'\n';return failures?1:0;
    } catch(...){std::cerr<<"Character fixture failed (database diagnostics suppressed)\n";return 1;}
}
