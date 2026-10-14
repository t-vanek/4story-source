#include "postgresql_chat_repository.h"
#include "postgresql_friend_repository.h"
#include "postgresql_world_owner.h"
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
}
PostgreSQLChatRepository::PostgreSQLChatRepository(PostgreSQLFriendRepository& authority):m_authority(authority) {
    auto lease=authority.m_pool.Acquire();auto tx=BeginWorldTransaction(*lease,authority.m_world,authority.m_token);int rows=0;
    *lease<<"SELECT count(*) FROM (SELECT world_id,ends_at,name_bytes FROM app_world.chat_bans WHERE false) required",soci::into(rows);
    *lease<<"SELECT count(*) FROM (SELECT world_id,owner_token,target_id,name_bytes,issuer_id,session_key,source_server,"
        "source_map_token,source_connection_id,source_authority_epoch,minutes,replace_deadline,ends_at "
        "FROM app_world.chat_ban_mutations WHERE false) required",soci::into(rows);
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
