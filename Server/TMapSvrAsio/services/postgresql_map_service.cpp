#include "postgresql_map_service.h"
#include "postgresql_map_owner.h"
#include <soci/soci.h>
#include <stdexcept>
#include <limits>

namespace tmapsvr {
namespace {
bool Hash(const std::string& s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
}
PostgreSQLMapService::PostgreSQLMapService(fourstory::db::SessionPool& pool,PostgreSQLMapConfig config)
    :m_pool(pool),m_config(std::move(config)){
    if(pool.GetBackend()!=fourstory::db::Backend::PostgreSQL||!Hash(m_config.owner_token)||
       !Hash(m_config.character_manifest)||!Hash(m_config.routing_manifest)||!Hash(m_config.actor_manifest))
        throw std::runtime_error("Native Map requires owned PostgreSQL and all verified catalog hashes");
    try{auto lease=pool.Acquire();soci::transaction tx(*lease);*lease<<"SET TRANSACTION ISOLATION LEVEL REPEATABLE READ, READ ONLY";CheckCatalogs(*lease);tx.commit();}
    catch(...){throw std::runtime_error("Native Map schema/catalog validation failed");}
}
void PostgreSQLMapService::CheckCatalogs(soci::session& sql) const {
    int n=0;
    sql<<"SELECT (SELECT count(*) FROM character_compat.catalog_release WHERE manifest_sha256=:c AND status='verified') + "
         "(SELECT count(*) FROM route_compat.catalog_release WHERE manifest_sha256=:r AND status='verified') + "
         "(SELECT count(*) FROM actor_compat.catalog_release WHERE manifest_sha256=:a AND status='verified')",
        soci::use(m_config.character_manifest,"c"),soci::use(m_config.routing_manifest,"r"),soci::use(m_config.actor_manifest,"a"),soci::into(n);
    if(n!=3)throw std::runtime_error("Native Map release changed or unavailable");
    sql<<"SELECT count(*) FROM actor_compat.statistics_release",soci::into(n);
    if(n!=1)throw std::runtime_error("Native Map requires the four-table actor release");
}
bool PostgreSQLMapService::LockAccount(soci::session& sql,const MapSessionClaim& c) const {
    if(c.group!=m_config.world||!c.user_id||!c.key||!c.char_id||!c.connection_id||
       c.connection_id>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())||
       c.authority_epoch>static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))return false;
    const long long user=c.user_id;int n=0;
    sql<<"SELECT 1 FROM app_global.\"TACCOUNT_PW\" WHERE \"dwUserID\"=:u FOR UPDATE",soci::use(user,"u"),soci::into(n);
    return sql.got_data()&&n==1;
}
bool PostgreSQLMapService::LockClaim(soci::session& sql,const MapSessionClaim& c,std::string& phase) const {
    if(c.role!=MapSessionRole::Primary)return false;
    const int world=c.group,server=m_config.server,channel=c.channel;
    const long long user=c.user_id,key=c.key,character=c.char_id,generation=c.connection_id,epoch=c.authority_epoch;
    sql<<"SELECT m.phase FROM app_world.map_sessions m JOIN app_global.\"TCURRENTUSER\" s ON s.\"dwKEY\"=m.session_key "
         "WHERE m.world_id=:w AND m.server_id=:s AND m.char_id=:c AND m.user_id=:u AND m.session_key=:k AND m.channel=:ch "
         "AND m.owner_token=:t AND m.connection_id=:g AND m.authority_epoch=:epoch AND m.phase NOT IN ('orphaned','quarantined') "
         "AND s.\"dwUserID\"=m.user_id AND s.\"dwCharID\"=m.char_id AND s.\"bGroupID\"=m.world_id AND s.\"bChannel\"=m.channel FOR UPDATE OF s,m",
        soci::use(world,"w"),soci::use(server,"s"),soci::use(character,"c"),soci::use(user,"u"),soci::use(key,"k"),
        soci::use(channel,"ch"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(epoch,"epoch"),soci::into(phase);
    return sql.got_data();
}
std::optional<MapSessionInfo> PostgreSQLMapService::LookupSession(std::uint32_t user,std::uint32_t key){
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    const long long uid=user,k=key;int ch=0,world=0,character=0,locked=0;
    sql<<"SELECT \"bGroupID\",\"bChannel\",\"dwCharID\",\"bLocked\" FROM app_global.\"TCURRENTUSER\" WHERE \"dwUserID\"=:u AND \"dwKEY\"=:k",
        soci::use(uid,"u"),soci::use(k,"k"),soci::into(world),soci::into(ch),soci::into(character),soci::into(locked);
    if(!sql.got_data())return {};
    MapSessionInfo out;out.dwUserID=user;out.dwKEY=key;out.dwCharID=static_cast<std::uint32_t>(character);
    out.bGroupID=static_cast<std::uint8_t>(world);out.bChannel=static_cast<std::uint8_t>(ch);out.bLocked=locked!=0;
    tx->commit();return out;
}
std::optional<MapSessionInfo> PostgreSQLMapService::ClaimSession(const MapSessionClaim& c,const MapSessionInfo&){
    if(c.authority_epoch)return {}; // only initial socket admission claims epoch zero
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    if(!LockAccount(sql,c))return {};
    CheckCatalogs(sql);
    const int world=c.group,server=m_config.server,channel=c.channel;
    const long long user=c.user_id,key=c.key,character=c.char_id,generation=c.connection_id;
    int n=0;
    sql<<"SELECT 1 FROM app_global.\"TCURRENTUSER\" s JOIN app_global.\"TUSERINFOTABLE\" i USING(\"dwUserID\") "
         "JOIN app_world.\"TCHARTABLE\" c ON c.\"bWorldID\"=s.\"bGroupID\" AND c.\"dwCharID\"=s.\"dwCharID\" "
         "JOIN app_global.\"TALLCHARTABLE\" d ON d.\"bWorldID\"=c.\"bWorldID\" AND d.\"dwCharID\"=c.\"dwCharID\" "
         "WHERE s.\"dwUserID\"=:u AND s.\"dwKEY\"=:k AND s.\"dwCharID\"=:c AND s.\"bGroupID\"=:w AND s.\"bChannel\"=:ch "
         "AND s.\"bLocked\"=0 AND i.\"bAgreement\"=1 AND c.\"dwUserID\"=:u AND c.\"bDelete\"=0 AND d.\"dwUserID\"=:u AND d.\"bDelete\"=0 FOR UPDATE OF s,c",
        soci::use(user,"u"),soci::use(key,"k"),soci::use(character,"c"),soci::use(world,"w"),soci::use(channel,"ch"),soci::into(n);
    if(!sql.got_data())return {};
    sql<<"SELECT 1 FROM app_global.map_handoff WHERE session_key=:k AND user_id=:u AND world_id=:w AND char_id=:c "
         "AND channel=:ch AND server_id=:s AND routing_manifest=:r AND expires_at>clock_timestamp() FOR UPDATE",
        soci::use(key,"k"),soci::use(user,"u"),soci::use(world,"w"),soci::use(character,"c"),soci::use(channel,"ch"),
        soci::use(server,"s"),soci::use(m_config.routing_manifest,"r"),soci::into(n);
    if(!sql.got_data()) {
        auto replica=ClaimReplica(sql,c);
        if(replica)tx->commit();
        return replica;
    }
    sql<<"INSERT INTO app_world.map_sessions(world_id,char_id,user_id,session_key,channel,server_id,owner_token,connection_id,routing_manifest,character_manifest,phase) "
         "VALUES(:w,:c,:u,:k,:ch,:s,:t,:g,:r,:cm,'claimed')",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(user,"u"),soci::use(key,"k"),soci::use(channel,"ch"),soci::use(server,"s"),
        soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(m_config.routing_manifest,"r"),soci::use(m_config.character_manifest,"cm");
    sql<<"DELETE FROM app_global.map_handoff WHERE session_key=:k",soci::use(key,"k");
    tx->commit();MapSessionInfo out;out.dwUserID=c.user_id;out.dwKEY=c.key;out.dwCharID=c.char_id;out.bGroupID=c.group;out.bChannel=c.channel;return out;
}
void PostgreSQLMapService::MarkReady(const MapSessionClaim&){throw std::runtime_error("Native ready requires its initial core checkpoint");}
void PostgreSQLMapService::CloseClaim(soci::session& sql,const MapSessionClaim& c) const {
    const long long key=c.key,user=c.user_id;
    sql<<"UPDATE app_global.\"TLOG\" SET \"timeLOGOUT\"=CURRENT_TIMESTAMP WHERE \"dwKEY\"=:k AND \"dwUserID\"=:u",soci::use(key,"k"),soci::use(user,"u");
    sql<<"DELETE FROM app_world.map_sessions WHERE session_key=:k",soci::use(key,"k");
    sql<<"DELETE FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\"=:k AND \"dwUserID\"=:u",soci::use(key,"k"),soci::use(user,"u");
}
void PostgreSQLMapService::ReleaseSession(const MapSessionClaim& c){
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    if(!LockAccount(sql,c))return;
    std::string phase;if(!LockClaim(sql,c,phase)) {
        const int world=c.group,character=c.char_id;const long long generation=c.connection_id,epoch=c.authority_epoch;int uncertain=0;
        sql<<"SELECT 1 FROM app_world.map_sessions WHERE world_id=:w AND char_id=:c AND owner_token=:t "
             "AND connection_id=:g AND authority_epoch>:e",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(epoch,"e"),soci::into(uncertain);
        if(sql.got_data())throw std::runtime_error("Unconfirmed primary transfer retains local reservation");
        ReleaseReplica(sql,c);tx->commit();return;
    }
    if(phase=="ready"||phase=="transferring")throw std::runtime_error("Authoritative Map session must save before release");
    if(phase=="loaded"&&c.authority_epoch) {
        int retired=0;
        const int world=c.group,character=c.char_id;const long long generation=c.connection_id,epoch=c.authority_epoch;
        sql<<"UPDATE app_world.map_checkpoints SET outcome='logout',saved_at=clock_timestamp() WHERE world_id=:w AND char_id=:c "
             "AND owner_token=:t AND connection_id=:g AND authority_epoch=:e AND outcome='active' "
             "AND app_world.map_checkpoint_matches(map_checkpoints) RETURNING 1",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(epoch,"e"),soci::into(retired);
        if(!sql.got_data())throw std::runtime_error("Transferred load receipt changed before release");
    }
    CloseClaim(sql,c);tx->commit();
}
std::optional<CharSnapshot> PostgreSQLMapService::LoadChar(std::uint32_t){throw std::runtime_error("Native Map load requires its connection claim");}
void PostgreSQLMapService::SaveChar(const CharSnapshot&){throw std::runtime_error("Native Map save requires its connection claim");}
}
