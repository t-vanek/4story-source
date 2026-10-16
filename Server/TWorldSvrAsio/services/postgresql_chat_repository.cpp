#include "postgresql_chat_repository.h"
#include "postgresql_friend_repository.h"
#include "postgresql_world_owner.h"
#include "fourstory/db/native_map_topology.h"
#include "char_registry.h"
#include <soci/soci.h>
#include <algorithm>
#include <bit>
#include <mutex>
#include <stdexcept>
namespace tworldsvr {
namespace {
int Signed(std::uint32_t value){return std::bit_cast<std::int32_t>(value);}
std::string Hex(const std::string& bytes) {
    constexpr char digits[]="0123456789abcdef";std::string out;
    for(unsigned char byte:bytes){out+=digits[byte>>4];out+=digits[byte&15];}return out;
}
std::string WireName(soci::session& sql,const std::string& text,const std::string& encoding) {
    if(text.empty()||text.find('\0')!=std::string::npos)throw std::runtime_error("Invalid native communication name");
    if(std::all_of(text.begin(),text.end(),[](unsigned char byte){return byte<128;})) {
        if(text.size()>50)throw std::runtime_error("Source communication name width exceeded");return text;
    }
    if(encoding.empty())throw std::runtime_error("Explicit ANSI communication name profile required");
    std::string hex,roundtrip,probe;
    sql<<"SELECT encode(convert_to(:t,:e),'hex'),convert_from(convert_to(:t,:e),:e),encode(convert_to('Mail',:e),'hex')",
        soci::use(text,"t"),soci::use(encoding,"e"),soci::into(hex),soci::into(roundtrip),soci::into(probe);
    if(roundtrip!=text||probe!="4d61696c"||hex.size()>100)throw std::runtime_error("Lossless ANSI communication profile required");
    std::string bytes;
    for(std::size_t i=0;i<hex.size();i+=2)bytes+=static_cast<char>(std::stoul(hex.substr(i,2),nullptr,16));return bytes;
}
void RequireOperatorRelease(soci::session& sql,int world) {
    int count=0;sql<<"SELECT count(*) FROM world_compat.operator_release WHERE world_id=:w AND status='verified'",
        soci::use(world,"w"),soci::into(count);
    if(count!=1)throw std::runtime_error("Complete verified operator projection required");
}
PartyIdentity ReadPartyIdentity(soci::session& sql,const std::uint8_t world,
    const std::string& encoding,std::uint32_t character,FriendAuthority a) {
    const int id=Signed(character),key=Signed(a.key),server=a.server;
    int guilds=0;
    sql<<"SELECT count(*) FROM app_world.guild_membership WHERE world_id=:w AND char_id=:c",
        soci::use(world,"w"),soci::use(id,"c"),soci::into(guilds);
    // Native admission excludes unhydrated guilds. An absent cache entry is
    // never evidence for the source's guildless NAME_NULL.
    if(guilds!=0)return {};
    std::string name,token;long long connection=0,epoch=0;
    sql<<"SELECT c.\"szNAME\",m.owner_token,m.connection_id,m.authority_epoch "
         "FROM app_world.map_sessions m JOIN app_world.\"TCHARTABLE\" c ON c.\"bWorldID\"=m.world_id "
         "AND c.\"dwCharID\"=m.char_id JOIN app_global.\"TCURRENTUSER\" u ON u.\"dwKEY\"=m.session_key "
         "AND u.\"dwUserID\"=m.user_id AND u.\"dwCharID\"=m.char_id WHERE m.world_id=:w AND m.char_id=:c "
         "AND m.session_key=:k AND m.server_id=:s AND m.phase='ready' AND u.\"bLocked\"=0",
        soci::use(world,"w"),soci::use(id,"c"),soci::use(key,"k"),soci::use(server,"s"),
        soci::into(name),soci::into(token),soci::into(connection),soci::into(epoch);
    if(!sql.got_data()||connection<=0||epoch<0)return {};
    return {true,true,WireName(sql,name,encoding),token,connection,epoch};
}
}
PostgreSQLChatRepository::PostgreSQLChatRepository(PostgreSQLFriendRepository& authority):m_authority(authority) {
    auto lease=authority.m_pool.Acquire();auto tx=BeginWorldTransaction(*lease,authority.m_world,authority.m_token);int rows=0;
    *lease<<"SELECT count(*) FROM (SELECT world_id,ends_at,name_bytes FROM app_world.chat_bans WHERE false) required",soci::into(rows);
    *lease<<"SELECT count(*) FROM (SELECT world_id,owner_token,target_id,name_bytes,issuer_id,session_key,source_server,"
        "source_map_token,source_connection_id,source_authority_epoch,minutes,replace_deadline,ends_at "
        "FROM app_world.chat_ban_mutations WHERE false) required",soci::into(rows);
    *lease<<"SELECT count(*) FROM (SELECT world_id,char_id FROM app_world.guild_membership WHERE false) required",soci::into(rows);
    *lease<<"SELECT count(*) FROM (SELECT owner_token,connection_id,authority_epoch FROM app_world.map_sessions WHERE false) required",soci::into(rows);
    *lease<<"SELECT count(*) FROM (SELECT routing_manifest,routing_source_group,routing_topology "
        "FROM app_world.map_runtime_owner WHERE false) required",soci::into(rows);
    *lease<<"SELECT count(*) FROM (SELECT world_id,source_group,manifest_sha256 FROM route_compat.map_topology_release WHERE false) required",soci::into(rows);
    *lease<<"SELECT count(*) FROM (SELECT \"bGroup\",\"bChannel\",\"wMapID\",\"wUnitID\",\"bServerID\" "
        "FROM route_compat.source_map_regions WHERE false) required",soci::into(rows);
    *lease<<"SELECT count(*) FROM (SELECT \"bGroupID\",\"wMapID\",\"bLogChannel\",\"bPhyChannel\",\"bServerID\" "
        "FROM route_compat.source_map_channels WHERE false) required",soci::into(rows);
    *lease<<"SELECT count(*) FROM (SELECT primary_server,primary_token,primary_connection,primary_epoch,"
        "target_server,target_token,connection_id,phase FROM app_world.map_replicas WHERE false) required",soci::into(rows);
    tx->commit();
}
std::vector<std::uint32_t> PostgreSQLChatRepository::LoadOperators() {
    auto& repo=m_authority;std::lock_guard guard(repo.m_mutex);
    if(repo.m_failed)throw std::runtime_error("Native communication authority stopped");
    try {
        auto lease=repo.m_pool.Acquire();auto& sql=*lease;auto tx=BeginWorldTransaction(sql,repo.m_world,repo.m_token,true);
        RequireOperatorRelease(sql,repo.m_world);std::vector<std::uint32_t> out;
        {soci::rowset<int> rows=(sql.prepare<<"SELECT \"dwOperatorID\" FROM app_world.\"TOPERATORTABLE\" "
            "WHERE \"bWorldID\"=:w ORDER BY (\"dwOperatorID\"::bigint & 4294967295)",soci::use(repo.m_world,"w"));
         for(int id:rows)out.push_back(static_cast<std::uint32_t>(id));}
        if(out.size()>65535)throw std::runtime_error("Operator list exceeds source WORD count");
        tx->commit();return out;
    } catch(...) {repo.Fail();throw std::runtime_error("Native operator bootstrap failed; no implicit empty authorization");}
}
ChatAuthority PostgreSQLChatRepository::Authorize(std::uint32_t character,FriendAuthority a,bool loading) {
    auto& repo=m_authority;std::lock_guard guard(repo.m_mutex);
    if(repo.m_failed)throw std::runtime_error("Native communication authority stopped");
    try {
        auto lease=repo.m_pool.Acquire();auto& sql=*lease;auto tx=BeginWorldTransaction(sql,repo.m_world,repo.m_token);
        if(!repo.AuthorizeCommunication(sql,character,a,loading))return {};
        RequireOperatorRelease(sql,repo.m_world);
        const int id=Signed(character);int op=0;std::string name;
        sql<<"SELECT c.\"szNAME\",EXISTS(SELECT 1 FROM app_world.\"TOPERATORTABLE\" o WHERE o.\"bWorldID\"=c.\"bWorldID\" "
             "AND o.\"dwOperatorID\"=c.\"dwCharID\")::integer FROM app_world.\"TCHARTABLE\" c WHERE c.\"bWorldID\"=:w AND c.\"dwCharID\"=:c",
            soci::use(repo.m_world,"w"),soci::use(id,"c"),soci::into(name),soci::into(op);
        if(!sql.got_data())return {};
        const auto hex=Hex(WireName(sql,name,repo.m_encoding));long long until=0;
        sql<<"SELECT ends_at FROM app_world.chat_bans WHERE world_id=:w AND name_bytes=decode(:name,'hex')",
            soci::use(repo.m_world,"w"),soci::use(hex,"name"),soci::into(until);
        tx->commit();return {true,op!=0,until};
    } catch(...) {repo.Fail();throw std::runtime_error("Native communication authorization failed; World stopped without replay");}
}
std::string PostgreSQLChatRepository::MapOwner(std::uint8_t server) {
    auto& repo=m_authority;std::lock_guard guard(repo.m_mutex);
    if(repo.m_failed)throw std::runtime_error("Native peer authority stopped");
    try {
        auto lease=repo.m_pool.Acquire();auto& sql=*lease;auto tx=BeginWorldTransaction(sql,repo.m_world,repo.m_token,true);
        std::string token,manifest;const int sid=server;int source_group=-1;
        soci::indicator manifest_status=soci::i_null,group_status=soci::i_null;
        sql<<"SELECT o.owner_token,o.routing_manifest,o.routing_source_group FROM app_world.map_runtime_owner o WHERE o.world_id=:w AND o.server_id=:s "
             "AND EXISTS(SELECT 1 FROM pg_locks l WHERE l.pid=o.backend_pid AND l.locktype='advisory' "
             "AND l.classid=CAST(877478224 AS oid) AND l.objid=CAST((o.world_id::integer<<8)|o.server_id::integer AS oid) "
             "AND l.objsubid=2 AND l.granted) FOR SHARE OF o",
            soci::use(repo.m_world,"w"),soci::use(sid,"s"),soci::into(token),
            soci::into(manifest,manifest_status),soci::into(source_group,group_status);
        if(!sql.got_data()||manifest_status!=soci::i_ok||group_status!=soci::i_ok)return {};
        const auto topology=fourstory::db::ReadNativeMapTopology(sql,static_cast<std::uint8_t>(repo.m_world),server,manifest);
        if(source_group!=topology.source_group)return {};
        const auto shape=topology.Json();int matches=0;
        sql<<"SELECT count(*) FROM app_world.map_runtime_owner WHERE world_id=:w AND server_id=:s AND owner_token=:t "
            "AND routing_topology=CAST(:shape AS jsonb)",soci::use(repo.m_world,"w"),soci::use(sid,"s"),
            soci::use(token,"t"),soci::use(shape,"shape"),soci::into(matches);
        if(matches!=1)return {};
        tx->commit();return token;
    } catch(...) {repo.Fail();throw std::runtime_error("Native peer owner lookup failed; World stopped without replay");}
}
PartyIdentity PostgreSQLChatRepository::AuthorizeParty(std::uint32_t character,FriendAuthority a) {
    auto& repo=m_authority;std::lock_guard guard(repo.m_mutex);
    if(repo.m_failed)throw std::runtime_error("Native party authority stopped");
    try {
        auto lease=repo.m_pool.Acquire();auto& sql=*lease;auto tx=BeginWorldTransaction(sql,repo.m_world,repo.m_token);
        if(!repo.AuthorizeCommunication(sql,character,a,false))return {};
        auto out=ReadPartyIdentity(sql,repo.m_world,repo.m_encoding,character,a);
        if(!out.valid)return {};
        tx->commit();return out;
    } catch(...) {repo.Fail();throw std::runtime_error("Native party authorization failed; World stopped without replay");}
}
std::array<PartyIdentity,2> PostgreSQLChatRepository::AuthorizePartyPair(
    std::uint32_t character,std::uint32_t target,FriendAuthority a) {
    auto& repo=m_authority;std::lock_guard guard(repo.m_mutex);
    if(repo.m_failed)throw std::runtime_error("Native party authority stopped");
    try {
        auto lease=repo.m_pool.Acquire();auto& sql=*lease;auto tx=BeginWorldTransaction(sql,repo.m_world,repo.m_token);
        // Both Map owners/accounts/claims are locked in the same deterministic
        // order as existing mutual social operations, until this read commits.
        if(!target||!repo.AuthorizeSoulmateOffer(sql,character,target,a))return {};
        std::array<PartyIdentity,2> out{
            ReadPartyIdentity(sql,repo.m_world,repo.m_encoding,character,a),
            ReadPartyIdentity(sql,repo.m_world,repo.m_encoding,target,{a.other_key,a.other_server})};
        if(!out[0].valid||!out[1].valid)return {};
        tx->commit();return out;
    } catch(...) {repo.Fail();throw std::runtime_error("Native party pair authorization failed; World stopped without replay");}
}
std::array<PartyIdentity,2> PostgreSQLChatRepository::AuthorizePartyReplica(
    std::uint32_t character,FriendAuthority a) {
    auto& repo=m_authority;std::lock_guard guard(repo.m_mutex);
    if(repo.m_failed)throw std::runtime_error("Native party authority stopped");
    try {
        auto lease=repo.m_pool.Acquire();auto& sql=*lease;auto tx=BeginWorldTransaction(sql,repo.m_world,repo.m_token);
        if(!repo.AuthorizeReplicaParty(sql,character,a))return {};
        const auto primary=ReadPartyIdentity(sql,repo.m_world,repo.m_encoding,character,a);
        if(!primary.valid)return {};
        const int id=Signed(character),key=Signed(a.key),source=a.server,target=a.other_server;
        std::string token;long long connection=0,epoch=0;
        sql<<"SELECT r.target_token,r.connection_id,r.primary_epoch FROM app_world.map_replicas r "
             "JOIN app_world.map_sessions m ON m.world_id=r.world_id AND m.char_id=r.char_id "
             "AND m.server_id=r.primary_server AND m.owner_token=r.primary_token "
             "AND m.connection_id=r.primary_connection AND m.authority_epoch=r.primary_epoch "
             "JOIN app_world.map_runtime_owner o ON o.world_id=r.world_id AND o.server_id=r.target_server "
             "AND o.owner_token=r.target_token WHERE r.world_id=:w AND r.char_id=:c AND m.session_key=:k "
             "AND m.server_id=:source AND m.phase='ready' AND r.target_server=:target AND r.phase='ready' "
             "FOR SHARE OF r",
            soci::use(repo.m_world,"w"),soci::use(id,"c"),soci::use(key,"k"),soci::use(source,"source"),
            soci::use(target,"target"),soci::into(token),soci::into(connection),soci::into(epoch);
        // Sorted process locks proved both live advisory owners. Revalidate the
        // primary connection/epoch relationship while holding the ready replica.
        if(!sql.got_data()||connection<=0||epoch!=primary.authority_epoch)return {};
        std::array<PartyIdentity,2> out{primary,PartyIdentity{true,true,primary.name,token,connection,epoch}};
        tx->commit();return out;
    } catch(...) {repo.Fail();throw std::runtime_error("Native replica party authorization failed; World stopped without replay");}
}
std::optional<std::int64_t> PostgreSQLChatRepository::Ban(std::uint32_t target,const std::string& name,
    FriendAuthority target_a,std::uint16_t minutes,std::uint32_t issuer,FriendAuthority issuer_a,bool replace) {
    auto& repo=m_authority;std::lock_guard guard(repo.m_mutex);
    if(repo.m_failed)throw std::runtime_error("Native communication authority stopped");
    try {
        auto lease=repo.m_pool.Acquire();auto& sql=*lease;auto tx=BeginWorldTransaction(sql,repo.m_world,repo.m_token);
        // Sorted participants match the Map/World account locking order. The
        // automatic sender is the target; a GM has an independently fenced key.
        if(issuer&&issuer!=target) {
            auto a=issuer_a;a.other_key=target_a.key;a.other_server=target_a.server;
            if(!repo.AuthorizeSoulmateOffer(sql,issuer,target,a))return {};
        } else if(!repo.AuthorizeCommunication(sql,target,target_a,false))return {};
        RequireOperatorRelease(sql,repo.m_world);
        const int target_id=Signed(target),issuer_id=Signed(issuer),key=Signed(issuer?issuer_a.key:target_a.key);
        const int server=replace?0:issuer?issuer_a.server:target_a.server;
        if(issuer&&issuer!=target) {
            const int target_key=Signed(target_a.key),issuer_key=Signed(issuer_a.key);int current=0;
            sql<<"SELECT count(*) FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\" IN (:target,:issuer) AND \"bLocked\"=0",
                soci::use(target_key,"target"),soci::use(issuer_key,"issuer"),soci::into(current);
            if(current!=2)return {};
        }
        if(issuer) {
            int allowed=0;sql<<"SELECT 1 FROM app_world.\"TOPERATORTABLE\" WHERE \"bWorldID\"=:w AND \"dwOperatorID\"=:c",
                soci::use(repo.m_world,"w"),soci::use(issuer_id,"c"),soci::into(allowed);
            if(!sql.got_data()||allowed!=1)return {};
        }
        std::string actual;
        sql<<"SELECT \"szNAME\" FROM app_world.\"TCHARTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c",
            soci::use(repo.m_world,"w"),soci::use(target_id,"c"),soci::into(actual);
        if(!sql.got_data()||WireName(sql,actual,repo.m_encoding)!=name)return {};
        const auto hex=Hex(name);const int duration=minutes,replacing=replace?1:0;long long deadline=0;
        std::string source_token;long long source_connection=0,source_epoch=0;
        if(server) {
            const int actor=issuer?issuer_id:target_id;
            sql<<"SELECT owner_token,connection_id,authority_epoch FROM app_world.map_sessions WHERE world_id=:w "
                 "AND char_id=:c AND session_key=:key AND server_id=:server AND phase='ready'",
                soci::use(repo.m_world,"w"),soci::use(actor,"c"),soci::use(key,"key"),soci::use(server,"server"),
                soci::into(source_token),soci::into(source_connection),soci::into(source_epoch);
            if(!sql.got_data())return {};
        }
        sql<<"INSERT INTO app_world.chat_bans(world_id,name_bytes,ends_at) "
             "VALUES(:w,decode(:name,'hex'),CASE WHEN :minutes=0 AND :replace=0 THEN 0 ELSE floor(extract(epoch FROM clock_timestamp()))::bigint+:minutes*60 END) "
             "ON CONFLICT(world_id,name_bytes) DO UPDATE SET ends_at=CASE WHEN :minutes=0 AND :replace=0 THEN 0 "
             "ELSE (CASE WHEN :replace=1 THEN floor(extract(epoch FROM clock_timestamp()))::bigint "
             "ELSE greatest(app_world.chat_bans.ends_at,floor(extract(epoch FROM clock_timestamp()))::bigint) END)+:minutes*60 END RETURNING ends_at",
            soci::use(repo.m_world,"w"),soci::use(hex,"name"),soci::use(duration,"minutes"),soci::use(replacing,"replace"),soci::into(deadline);
        sql<<"INSERT INTO app_world.chat_ban_mutations(world_id,owner_token,target_id,name_bytes,issuer_id,session_key,source_server,"
             "source_map_token,source_connection_id,source_authority_epoch,minutes,replace_deadline,ends_at) "
             "VALUES(:w,:token,:target,decode(:name,'hex'),:issuer,:key,:server,NULLIF(:map_token,''),NULLIF(:connection,0),"
             "CASE WHEN :server=0 THEN NULL ELSE :epoch END,:minutes,CAST(:replace AS integer)<>0,:until)",
            soci::use(repo.m_world,"w"),soci::use(repo.m_token,"token"),soci::use(target_id,"target"),soci::use(hex,"name"),
            soci::use(issuer_id,"issuer"),soci::use(key,"key"),soci::use(server,"server"),soci::use(source_token,"map_token"),
            soci::use(source_connection,"connection"),soci::use(source_epoch,"epoch"),soci::use(duration,"minutes"),soci::use(replacing,"replace"),soci::use(deadline,"until");
        tx->commit();return deadline;
    } catch(...) {repo.Fail();throw std::runtime_error("Native chat ban commit failed; World stopped for reconciliation, no replay");}
}
}
