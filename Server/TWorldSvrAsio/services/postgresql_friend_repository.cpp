#include "postgresql_friend_repository.h"
#include "postgresql_world_owner.h"
#include <soci/soci.h>
#include <algorithm>
#include <bit>
#include <stdexcept>
#include <set>
namespace tworldsvr {
namespace {
struct StaleActor {};
int Signed(std::uint32_t id){return std::bit_cast<std::int32_t>(id);}
std::string Hex(const std::string& bytes) {
    std::string out;constexpr char hex[]="0123456789abcdef";
    for(unsigned char b:bytes){out+=hex[b>>4];out+=hex[b&15];}return out;
}
std::string Unhex(const std::string& hex) {
    if(hex.size()%2)throw std::runtime_error("Invalid original social text");
    std::string out;for(std::size_t n=0;n<hex.size();n+=2)out+=static_cast<char>(std::stoul(hex.substr(n,2),nullptr,16));
    return out;
}
std::string Decode(soci::session& sql,const std::string& bytes,const std::string& encoding) {
    if(bytes.size()>20||bytes.find('\0')!=std::string::npos)throw std::runtime_error("Invalid source group text");
    if(std::all_of(bytes.begin(),bytes.end(),[](unsigned char b){return b<128;}))return bytes;
    if(encoding.empty())throw std::runtime_error("Explicit native World text profile required");
    const auto hex=Hex(bytes);std::string text,roundtrip,probe;
    sql<<"SELECT convert_from(decode(:h,'hex'),:e),encode(convert_to(convert_from(decode(:h,'hex'),:e),:e),'hex'),"
         "encode(convert_to('Mail',:e),'hex')",soci::use(hex,"h"),soci::use(encoding,"e"),
         soci::into(text),soci::into(roundtrip),soci::into(probe);
    if(roundtrip!=hex||probe!="4d61696c")throw std::runtime_error("Lossless ANSI World text profile required");
    return text;
}
std::string WireName(soci::session& sql,const std::string& text,const std::string& encoding) {
    if(text.find('\0')!=std::string::npos)throw std::runtime_error("NUL in social character name");
    if(std::all_of(text.begin(),text.end(),[](unsigned char b){return b<128;}))return text;
    if(encoding.empty())throw std::runtime_error("Explicit native World name encoding required");
    std::string hex,roundtrip,probe;
    sql<<"SELECT encode(convert_to(:t,:e),'hex'),convert_from(convert_to(:t,:e),:e),encode(convert_to('Mail',:e),'hex')",
         soci::use(text,"t"),soci::use(encoding,"e"),soci::into(hex),soci::into(roundtrip),soci::into(probe);
    if(roundtrip!=text||probe!="4d61696c")throw std::runtime_error("Nonrepresentable social name");
    auto bytes=Unhex(hex);if(bytes.size()>50)throw std::runtime_error("Source name width exceeded");return bytes;
}
}
PostgreSQLFriendRepository::PostgreSQLFriendRepository(fourstory::db::SessionPool& pool,std::uint8_t world,
    std::string token,std::string encoding,std::function<void()> stop)
 :m_pool(pool),m_world(world),m_token(std::move(token)),m_encoding(std::move(encoding)),m_stop(std::move(stop)) {
    if(pool.GetBackend()!=fourstory::db::Backend::PostgreSQL)throw std::runtime_error("Native social persistence requires PostgreSQL");
    auto lease=pool.Acquire();auto tx=BeginWorldTransaction(*lease,m_world,m_token);int count=0;
    *lease<<"SELECT count(name_bytes) FROM app_world.\"TFRIENDGROUPTABLE\" WHERE false",soci::into(count);
    *lease<<"SELECT count(\"bGroup\") FROM app_world.\"TFRIENDTABLE\" WHERE false",soci::into(count);
    *lease<<"SELECT count(\"dwTime\") FROM app_world.\"TSOULMATETABLE\" WHERE false",soci::into(count);
    *lease<<"SELECT count(session_key) FROM app_world.social_mutations WHERE false",soci::into(count);
    tx->commit();
}
void PostgreSQLFriendRepository::Fail() {
    if(!m_failed.exchange(true)&&m_stop)m_stop();
}
void PostgreSQLFriendRepository::LockActors(soci::session& sql,std::uint32_t character,std::uint32_t target,
                                            FriendAuthority a,bool mutual) {
    if(!a.key||!a.server||(mutual&&(!a.other_key||!a.other_server)))throw StaleActor{};
    // Match native Map order: process row, account, claim/current-user, char.
    // Acquiring a Map owner after its account could deadlock its takeover.
    std::set<int> servers{a.server};if(mutual)servers.insert(a.other_server);
    for(const auto server:servers) {
        int found=0;
        sql<<"SELECT 1 FROM app_world.map_runtime_owner o WHERE o.world_id=:w AND o.server_id=:s "
             "AND EXISTS(SELECT 1 FROM pg_locks l WHERE l.pid=o.backend_pid AND l.locktype='advisory' "
             "AND l.classid=CAST(877478224 AS oid) AND l.objid=CAST((o.world_id::integer<<8)|o.server_id::integer AS oid) "
             "AND l.objsubid=2 AND l.granted) FOR SHARE OF o",
             soci::use(m_world,"w"),soci::use(server,"s"),soci::into(found);
        if(!sql.got_data()||found!=1)throw StaleActor{};
    }
    std::set<std::uint32_t> users;
    const auto lookup=[&](std::uint32_t c) {
        const int id=Signed(c);int user=0;
        sql<<"SELECT \"dwUserID\" FROM app_world.\"TCHARTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c",
            soci::use(m_world,"w"),soci::use(id,"c"),soci::into(user);
        if(!sql.got_data())throw StaleActor{};
        users.insert(static_cast<std::uint32_t>(user));
    };
    lookup(character);if(mutual)lookup(target);
    for(const auto user:users) {
        const int id=Signed(user);int found=0;
        sql<<"SELECT 1 FROM app_global.\"TACCOUNT_PW\" WHERE \"dwUserID\"=:u FOR UPDATE",
            soci::use(id,"u"),soci::into(found);
        if(!sql.got_data()||found!=1)throw StaleActor{};
    }
}
void PostgreSQLFriendRepository::GuardActor(soci::session& sql,std::uint32_t character,FriendAuthority a,bool loading) {
    if(!a.key||!a.server)throw StaleActor{};
    const int id=Signed(character),key=Signed(a.key),server=a.server,load=loading?1:0;int found=0;
    // Share locks hold the original key/current primary and process generation
    // through COMMIT, so Login cleanup, Map handoff and takeover cannot pass a
    // social write. The channel/authority metadata stays on the server.
    sql<<"SELECT 1 FROM app_world.map_sessions m JOIN app_global.\"TCURRENTUSER\" u ON u.\"dwKEY\"=m.session_key "
         "AND u.\"dwUserID\"=m.user_id AND u.\"dwCharID\"=m.char_id AND u.\"bGroupID\"=m.world_id AND u.\"bChannel\"=m.channel "
         "JOIN app_world.map_runtime_owner o ON o.world_id=m.world_id AND o.server_id=m.server_id AND o.owner_token=m.owner_token "
         "JOIN app_world.\"TCHARTABLE\" c ON c.\"bWorldID\"=m.world_id AND c.\"dwCharID\"=m.char_id AND c.\"dwUserID\"=m.user_id AND c.\"bDelete\"=0 "
         "WHERE m.world_id=:w AND m.char_id=:c AND m.session_key=:k AND m.server_id=:s "
         "AND (m.phase='ready' OR (:loading=1 AND m.phase IN ('claimed','loaded'))) "
         "AND EXISTS(SELECT 1 FROM pg_locks l WHERE l.pid=o.backend_pid AND l.locktype='advisory' "
         "AND l.classid=CAST(877478224 AS oid) AND l.objid=CAST((m.world_id::integer<<8)|m.server_id::integer AS oid) "
         "AND l.objsubid=2 AND l.granted) FOR SHARE OF m,u,o,c",
         soci::use(m_world,"w"),soci::use(id,"c"),soci::use(key,"k"),soci::use(server,"s"),soci::use(load,"loading"),soci::into(found);
    if(!sql.got_data()||found!=1)throw StaleActor{};
}
bool PostgreSQLFriendRepository::AuthorizeSoulmateOffer(soci::session& sql,std::uint32_t character,
                                                       std::uint32_t target,FriendAuthority a) {
    try {
        LockActors(sql,character,target,a,target!=0);
        if(!target){GuardActor(sql,character,a);return true;}
        if(character<target) {
            GuardActor(sql,character,a);GuardActor(sql,target,{a.other_key,a.other_server});
        } else {
            GuardActor(sql,target,{a.other_key,a.other_server});GuardActor(sql,character,a);
        }
        return true;
    } catch(const StaleActor&) {return false;}
}
bool PostgreSQLFriendRepository::AuthorizeCommunication(soci::session& sql,std::uint32_t character,FriendAuthority a,bool loading) {
    try {
        LockActors(sql,character,0,a,false);GuardActor(sql,character,a,loading);
        const int key=Signed(a.key);int ready=0;
        sql<<"SELECT 1 FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\"=:k AND \"bLocked\"=0",soci::use(key),soci::into(ready);
        return sql.got_data()&&ready==1;
    } catch(const StaleActor&) {return false;}
}
FriendLoad PostgreSQLFriendRepository::LoadForChar(std::uint32_t character,FriendAuthority a) {
    std::lock_guard guard(m_mutex);if(m_failed)throw std::runtime_error("Native social persistence stopped");
    try {
        auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginWorldTransaction(sql,m_world,m_token);
        LockActors(sql,character,0,a,false);GuardActor(sql,character,a,true);FriendLoad out;const int id=Signed(character);
        {soci::rowset<soci::row> rows=(sql.prepare<<"SELECT \"bGroup\",encode(name_bytes,'hex') FROM app_world.\"TFRIENDGROUPTABLE\" "
            "WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c ORDER BY \"bGroup\"",soci::use(m_world,"w"),soci::use(id,"c"));
         for(const auto& r:rows)out.groups.emplace_back(static_cast<std::uint8_t>(r.get<int>(0)),Unhex(r.get<std::string>(1)));}
        struct Raw {std::uint32_t id;std::string name;std::uint8_t level,klass,group;bool blocked;};
        std::vector<Raw> forward,reverse;
        {soci::rowset<soci::row> rows=(sql.prepare<<"SELECT f.\"dwFriendID\",c.\"szNAME\",c.\"bLevel\",c.\"bClass\",f.\"bGroup\","
            "EXISTS(SELECT 1 FROM app_world.\"TPROTECTEDTABLE\" p WHERE p.\"bWorldID\"=f.\"bWorldID\" "
            "AND p.\"dwCharID\"=f.\"dwFriendID\" AND p.\"dwProtected\"=f.\"dwCharID\")::integer "
            "FROM app_world.\"TFRIENDTABLE\" f JOIN app_world.\"TCHARTABLE\" c ON c.\"bWorldID\"=f.\"bWorldID\" "
            "AND c.\"dwCharID\"=f.\"dwFriendID\" WHERE f.\"bWorldID\"=:w AND f.\"dwCharID\"=:c AND c.\"bDelete\"=0 "
            "ORDER BY (f.\"dwFriendID\"::bigint & 4294967295)",soci::use(m_world,"w"),soci::use(id,"c"));
         for(const auto& r:rows)forward.push_back({static_cast<std::uint32_t>(r.get<int>(0)),r.get<std::string>(1),
            static_cast<std::uint8_t>(r.get<int>(2)),static_cast<std::uint8_t>(r.get<int>(3)),static_cast<std::uint8_t>(r.get<int>(4)),r.get<int>(5)!=0});}
        {soci::rowset<soci::row> rows=(sql.prepare<<"SELECT f.\"dwCharID\",c.\"szNAME\",EXISTS(SELECT 1 FROM app_world.\"TPROTECTEDTABLE\" p "
            "WHERE p.\"bWorldID\"=f.\"bWorldID\" AND p.\"dwCharID\"=f.\"dwFriendID\" AND p.\"dwProtected\"=f.\"dwCharID\")::integer "
            "FROM app_world.\"TFRIENDTABLE\" f JOIN app_world.\"TCHARTABLE\" c ON c.\"bWorldID\"=f.\"bWorldID\" "
            "AND c.\"dwCharID\"=f.\"dwCharID\" WHERE f.\"bWorldID\"=:w AND f.\"dwFriendID\"=:c AND c.\"bDelete\"=0 "
            "ORDER BY (f.\"dwCharID\"::bigint & 4294967295)",soci::use(m_world,"w"),soci::use(id,"c"));
         for(const auto& r:rows)reverse.push_back({static_cast<std::uint32_t>(r.get<int>(0)),r.get<std::string>(1),0,0,0,r.get<int>(2)!=0});}
        for(const auto& r:forward)out.forward.push_back({r.id,WireName(sql,r.name,m_encoding),r.level,r.klass,r.group,r.blocked});
        for(const auto& r:reverse)out.reverse.push_back({r.id,WireName(sql,r.name,m_encoding),0,0,0,r.blocked});
        // TVIEW_SOULMATE joins names/level/class against the directed target,
        // for both CTBLSoulmate (incoming) and CTBLMySoulmate (own row).
        std::vector<IncomingSoulmate> mates;
        {soci::rowset<soci::row> rows=(sql.prepare<<"SELECT s.\"dwCharID\",s.\"dwTarget\",s.\"dwTime\",c.\"szNAME\",c.\"bLevel\",c.\"bClass\" "
            "FROM app_world.\"TSOULMATETABLE\" s JOIN app_world.\"TCHARTABLE\" c ON c.\"bWorldID\"=s.\"bWorldID\" "
            "AND c.\"dwCharID\"=s.\"dwTarget\" WHERE s.\"bWorldID\"=:w AND (s.\"dwCharID\"=:c OR s.\"dwTarget\"=:c) "
            "ORDER BY (s.\"dwCharID\"::bigint & 4294967295)",soci::use(m_world,"w"),soci::use(id,"c"));
         for(const auto& r:rows)mates.push_back({static_cast<std::uint32_t>(r.get<int>(0)),static_cast<std::uint32_t>(r.get<int>(1)),
            static_cast<std::uint32_t>(r.get<int>(2)),r.get<std::string>(3),static_cast<std::uint8_t>(r.get<int>(4)),static_cast<std::uint8_t>(r.get<int>(5))});}
        long long now=0;sql<<"SELECT floor(extract(epoch FROM clock_timestamp()))::bigint",soci::into(now);
        for(auto& mate:mates) {
            if(mate.time&&now-static_cast<long long>(mate.time)>=86400) {
                const int owner=Signed(mate.owner),target=Signed(mate.target),key=Signed(a.key),server=a.server;
                // Source load expires the exact directed row, including an
                // incoming row. Authorized by the logged-in graph participant;
                // no unrelated social row or target pairing is changed.
                sql<<"DELETE FROM app_world.\"TSOULMATETABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:o AND \"dwTarget\"=:t",
                    soci::use(m_world,"w"),soci::use(owner,"o"),soci::use(target,"t");
                sql<<"INSERT INTO app_world.social_mutations(world_id,owner_token,char_id,session_key,source_server,kind,target_id) "
                    "VALUES(:w,:token,:c,:key,:server,'soulmate_del',:target)",soci::use(m_world,"w"),soci::use(m_token,"token"),
                    soci::use(owner,"c"),soci::use(key,"key"),soci::use(server,"server"),soci::use(target,"target");
                continue;
            }
            mate.target_name=WireName(sql,mate.target_name,m_encoding);
            if(mate.owner==character) {
                out.has_soulmate=true;out.soulmate_target=mate.target;out.soulmate_time=mate.time;
                out.soulmate_name=mate.target_name;out.soulmate_level=mate.target_level;out.soulmate_class=mate.target_class;
            }
            if(mate.target==character&&!mate.time)out.incoming_soulmates.push_back(std::move(mate));
        }
        if(out.groups.size()>255||out.forward.size()>255||out.reverse.size()>255)
            throw std::runtime_error("Native social graph exceeds original BYTE response");
        tx->commit();return out;
    } catch(const StaleActor&) {FriendLoad rejected;rejected.authority_valid=false;return rejected;}
      catch(...) {Fail();throw std::runtime_error("Native social load failed; World stopped for reconciliation");}
}
bool PostgreSQLFriendRepository::Mutate(const std::string& kind,std::uint32_t character,std::uint32_t target,
    std::uint8_t group,const std::string& name,std::uint32_t time,FriendAuthority a) {
    std::lock_guard guard(m_mutex);if(m_failed)throw std::runtime_error("Native social persistence stopped");
    try {
        auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginWorldTransaction(sql,m_world,m_token);
        const int id=Signed(character),other=Signed(target),bucket=group,key=Signed(a.key),server=a.server,stamp=Signed(time);
        LockActors(sql,character,target,a,kind=="friend_mutual");
        const auto actor=[&]{GuardActor(sql,character,a);};
        const auto counterpart=[&]{GuardActor(sql,target,{a.other_key,a.other_server});};
        // Match unsigned source map iteration and lock both live actors in one
        // order. A stale request rolls back; it never erases a successor's state.
        if(kind=="friend_mutual") {
            if(character<target){actor();counterpart();}else{counterpart();actor();}
        } else actor();
        if(a.require_unlocked||kind=="group_delete") {
            const long long unsigned_key=a.key;int blocked=0;
            // Same account lock as Map secure-code writes; only flags/key are
            // read, never the credential bytes. Protected disconnect erasure
            // deliberately bypasses this gate, as the source does.
            sql<<"SELECT count(*) FROM app_global.secure_codes s JOIN app_world.\"TCHARTABLE\" c "
                "ON c.\"dwUserID\"=s.user_id WHERE c.\"bWorldID\"=:w AND c.\"dwCharID\"=:c "
                "AND NOT s.disabled AND s.unlocked_key<>:key",soci::use(m_world,"w"),soci::use(id,"c"),
                soci::use(unsigned_key,"key"),soci::into(blocked);
            if(blocked)return false;
        }
        const auto insert=[&](int from,int to){sql<<"INSERT INTO app_world.\"TFRIENDTABLE\"(\"bWorldID\",\"dwCharID\",\"dwFriendID\",\"bGroup\") "
            "VALUES(:w,:c,:t,0) ON CONFLICT(\"bWorldID\",\"dwCharID\",\"dwFriendID\") DO NOTHING",
            soci::use(m_world,"w"),soci::use(from,"c"),soci::use(to,"t");};
        if(kind=="friend_insert")insert(id,other);
        else if(kind=="friend_mutual"){if(character<target){insert(id,other);insert(other,id);}else{insert(other,id);insert(id,other);}}
        else if(kind=="friend_erase")sql<<"DELETE FROM app_world.\"TFRIENDTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"dwFriendID\"=:t",
            soci::use(m_world,"w"),soci::use(id,"c"),soci::use(other,"t");
        else if(kind=="group_make"||kind=="group_rename") {
            const auto text=Decode(sql,name,m_encoding),hex=Hex(name);
            if(kind=="group_make")sql<<"INSERT INTO app_world.\"TFRIENDGROUPTABLE\"(\"bWorldID\",\"dwCharID\",\"bGroup\",\"szName\",name_bytes) "
                "VALUES(:w,:c,:g,:n,decode(:h,'hex'))",soci::use(m_world,"w"),soci::use(id,"c"),soci::use(bucket,"g"),soci::use(text,"n"),soci::use(hex,"h");
            else sql<<"UPDATE app_world.\"TFRIENDGROUPTABLE\" SET \"szName\"=:n,name_bytes=decode(:h,'hex') WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"bGroup\"=:g",
                soci::use(text,"n"),soci::use(hex,"h"),soci::use(m_world,"w"),soci::use(id,"c"),soci::use(bucket,"g");
        } else if(kind=="group_delete")sql<<"DELETE FROM app_world.\"TFRIENDGROUPTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"bGroup\"=:g",
            soci::use(m_world,"w"),soci::use(id,"c"),soci::use(bucket,"g");
        else if(kind=="group_change")sql<<"UPDATE app_world.\"TFRIENDTABLE\" SET \"bGroup\"=:g WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"dwFriendID\"=:t",
            soci::use(bucket,"g"),soci::use(m_world,"w"),soci::use(id,"c"),soci::use(other,"t");
        else if(kind=="soulmate_reg")sql<<"INSERT INTO app_world.\"TSOULMATETABLE\"(\"bWorldID\",\"dwCharID\",\"dwTarget\",\"dwTime\") "
            "VALUES(:w,:c,:t,0) ON CONFLICT(\"bWorldID\",\"dwCharID\") DO UPDATE SET \"dwTarget\"=EXCLUDED.\"dwTarget\",\"dwTime\"=0",
            soci::use(m_world,"w"),soci::use(id,"c"),soci::use(other,"t");
        else if(kind=="soulmate_del")sql<<"DELETE FROM app_world.\"TSOULMATETABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"dwTarget\"=:t",
            soci::use(m_world,"w"),soci::use(id,"c"),soci::use(other,"t");
        else if(kind=="soulmate_end") {
            int changed=0;
            sql<<"WITH ended AS (UPDATE app_world.\"TSOULMATETABLE\" SET \"dwTime\"=:time "
                "WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND (:target=0 OR \"dwTarget\"=:target) AND \"dwTime\"=0 RETURNING 1) "
                "SELECT count(*) FROM ended",soci::use(stamp,"time"),soci::use(m_world,"w"),soci::use(id,"c"),
                soci::use(other,"target"),soci::into(changed);
            if(changed!=1)return false;
        }
        else throw std::logic_error("Unknown native social operation");
        // One audit entry per accepted invocation, not a hash of request bytes.
        // Identical valid repeated commands remain distinct source actions.
        sql<<"INSERT INTO app_world.social_mutations(world_id,owner_token,char_id,session_key,source_server,kind,target_id,group_id) "
             "VALUES(:w,:token,:c,:key,:server,:kind,:target,:group)",soci::use(m_world,"w"),soci::use(m_token,"token"),soci::use(id,"c"),
             soci::use(key,"key"),soci::use(server,"server"),soci::use(kind,"kind"),soci::use(other,"target"),soci::use(bucket,"group");
        tx->commit();return true;
    } catch(const StaleActor&) {return false;}
      catch(...) {Fail();throw std::runtime_error("Native social write failed; reconciliation required, no replay");}
}
bool PostgreSQLFriendRepository::MakeGroup(std::uint32_t c,std::uint8_t g,const std::string& n,FriendAuthority a){return Mutate("group_make",c,0,g,n,0,a);}
bool PostgreSQLFriendRepository::DeleteGroup(std::uint32_t c,std::uint8_t g,FriendAuthority a){return Mutate("group_delete",c,0,g,"",0,a);}
bool PostgreSQLFriendRepository::RenameGroup(std::uint32_t c,std::uint8_t g,const std::string& n,FriendAuthority a){return Mutate("group_rename",c,0,g,n,0,a);}
bool PostgreSQLFriendRepository::ChangeFriendGroup(std::uint32_t c,std::uint32_t f,std::uint8_t g,FriendAuthority a){return Mutate("group_change",c,f,g,"",0,a);}
bool PostgreSQLFriendRepository::InsertFriend(std::uint32_t c,std::uint32_t f,FriendAuthority a){return Mutate("friend_insert",c,f,0,"",0,a);}
bool PostgreSQLFriendRepository::InsertMutualFriends(std::uint32_t c,std::uint32_t f,FriendAuthority a){return Mutate("friend_mutual",c,f,0,"",0,a);}
bool PostgreSQLFriendRepository::EraseFriend(std::uint32_t c,std::uint32_t f,FriendAuthority a){return Mutate("friend_erase",c,f,0,"",0,a);}
bool PostgreSQLFriendRepository::RegSoulmate(std::uint32_t c,std::uint32_t t,FriendAuthority a){return Mutate("soulmate_reg",c,t,0,"",0,a);}
bool PostgreSQLFriendRepository::DelSoulmate(std::uint32_t c,std::uint32_t t,FriendAuthority a){return Mutate("soulmate_del",c,t,0,"",0,a);}
bool PostgreSQLFriendRepository::EndSoulmate(std::uint32_t c,std::uint32_t time,FriendAuthority a){return Mutate("soulmate_end",c,0,0,"",time,a);}
bool PostgreSQLFriendRepository::EndSoulmateExact(std::uint32_t c,std::uint32_t target,std::uint32_t time,FriendAuthority a){return Mutate("soulmate_end",c,target,0,"",time,a);}
}
