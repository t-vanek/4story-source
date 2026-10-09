#include "postgresql_map_owner.h"
#include <openssl/rand.h>
#include <stdexcept>
#include <vector>

namespace tmapsvr {
namespace {
constexpr int kMapNamespace=0x344d4150; // 4MAP, separate from native Login.
class OwnerBusy : public std::runtime_error {
public: OwnerBusy():std::runtime_error("Another native Map owner is active for this world/server"){}
};
}
PostgreSQLMapOwner::PostgreSQLMapOwner(const std::string& connection,std::uint8_t world,std::uint8_t server)
    :m_control(fourstory::db::Backend::PostgreSQL,connection,1),m_lease(m_control.Acquire()),m_world(world),m_server(server) {
    try {
        auto& sql=*m_lease;
        sql<<"SET statement_timeout='5s'";sql<<"SET lock_timeout='3s'";
        const int node=(m_world<<8)|m_server;int locked=0;
        sql<<"SELECT CASE WHEN pg_try_advisory_lock(:ns,:node) THEN 1 ELSE 0 END",
            soci::use(kMapNamespace,"ns"),soci::use(node,"node"),soci::into(locked);
        if(!locked)throw OwnerBusy();
        unsigned char bytes[32];if(RAND_bytes(bytes,sizeof(bytes))!=1)throw std::runtime_error("Random generation failed");
        constexpr char hex[]="0123456789abcdef";for(auto b:bytes){m_token+=hex[b>>4];m_token+=hex[b&15];}
        sql<<"SELECT pg_backend_pid()",soci::into(m_backend_pid);
        soci::transaction tx(sql);
        sql<<"INSERT INTO app_world.map_runtime_owner(world_id,server_id,owner_token,backend_pid) VALUES(:w,:s,:t,:p) "
             "ON CONFLICT(world_id,server_id) DO UPDATE SET owner_token=EXCLUDED.owner_token,backend_pid=EXCLUDED.backend_pid,acquired_at=clock_timestamp()",
            soci::use(m_world,"w"),soci::use(m_server,"s"),soci::use(m_token,"t"),soci::use(m_backend_pid,"p");
        // Token replacement waits for old transaction FOR SHARE locks. Pre-ready
        // claims cannot contain gameplay writes. Recover those under account locks.
        std::vector<int> users;
        {soci::rowset<int> rows=(sql.prepare<<"SELECT user_id FROM app_world.map_sessions WHERE world_id=:w AND server_id=:s "
            "AND owner_token<>:t AND phase IN ('claimed','loaded') AND authority_epoch=0 ORDER BY user_id",
            soci::use(m_world,"w"),soci::use(m_server,"s"),soci::use(m_token,"t"));for(int u:rows)users.push_back(u);}
        for(int user:users){
            int id=0;sql<<"SELECT \"dwUserID\" FROM app_global.\"TACCOUNT_PW\" WHERE \"dwUserID\"=:u FOR UPDATE",soci::use(user,"u"),soci::into(id);
            sql<<"WITH safe AS (DELETE FROM app_world.map_sessions WHERE world_id=:w AND server_id=:s AND user_id=:u "
                 "AND owner_token<>:t AND phase IN ('claimed','loaded') AND authority_epoch=0 RETURNING session_key), audit AS "
                 "(UPDATE app_global.\"TLOG\" SET \"timeLOGOUT\"=CURRENT_TIMESTAMP WHERE \"dwKEY\" IN (SELECT session_key FROM safe)) "
                 "DELETE FROM app_global.\"TCURRENTUSER\" WHERE \"dwUserID\"=:u AND \"dwKEY\" IN (SELECT session_key FROM safe)",
                soci::use(m_world,"w"),soci::use(m_server,"s"),soci::use(user,"u"),soci::use(m_token,"t");
        }
        // Only v1 ready sessions with an exact atomic receipt may roll back
        // their transient tail to the last committed core. Pre-v1/orphaned or
        // externally drifted state never acquires this recovery permission.
        users.clear();
        {soci::rowset<int> rows=(sql.prepare<<"SELECT user_id FROM app_world.map_sessions WHERE world_id=:w AND server_id=:s "
            "AND owner_token<>:t AND phase IN ('ready','loaded') ORDER BY user_id",
            soci::use(m_world,"w"),soci::use(m_server,"s"),soci::use(m_token,"t"));for(int u:rows)users.push_back(u);}
        for(int user:users){
            int id=0,recovered=0;
            sql<<"SELECT \"dwUserID\" FROM app_global.\"TACCOUNT_PW\" WHERE \"dwUserID\"=:u FOR UPDATE",soci::use(user,"u"),soci::into(id);
            sql<<R"SQL(WITH eligible AS (
              SELECT m.char_id,m.session_key FROM app_world.map_sessions m
              JOIN app_world.map_checkpoints p ON p.world_id=m.world_id AND p.char_id=m.char_id
                AND p.user_id=m.user_id AND p.server_id=m.server_id AND p.session_key=m.session_key
                AND p.owner_token=m.owner_token AND p.connection_id=m.connection_id AND p.authority_epoch=m.authority_epoch
              JOIN app_global."TCURRENTUSER" u ON u."dwKEY"=m.session_key AND u."dwUserID"=m.user_id
                AND u."dwCharID"=m.char_id AND u."bGroupID"=m.world_id AND u."bChannel"=m.channel
              JOIN app_world."TCHARTABLE" c ON c."bWorldID"=m.world_id AND c."dwCharID"=m.char_id
                AND c."dwUserID"=m.user_id AND c."bDelete"=0
              JOIN app_global."TALLCHARTABLE" d ON d."bWorldID"=m.world_id AND d."dwCharID"=m.char_id
                AND d."dwUserID"=m.user_id AND d."bDelete"=0
              WHERE m.world_id=:w AND m.server_id=:s AND m.user_id=:u AND m.owner_token<>:t AND m.phase IN ('ready','loaded')
                AND ((m.authority_epoch=0 AND m.phase='ready') OR p.transfer_body IS NOT NULL) AND p.recovery_contract IN (1,2) AND p.outcome='active'
                AND p.core_state=app_world.map_core_state(m.world_id,m.char_id)
            ), receipts AS (
              UPDATE app_world.map_checkpoints SET outcome='recovered',recovered_at=clock_timestamp()
              WHERE world_id=:w AND char_id IN (SELECT char_id FROM eligible) RETURNING char_id
            ), core_logout AS (
              UPDATE app_world."TCHARTABLE" SET "dLogoutDate"=date_trunc('minute',timezone('UTC',CURRENT_TIMESTAMP)+interval '30 seconds')
              WHERE "bWorldID"=:w AND "dwCharID" IN (SELECT char_id FROM receipts)
            ), closed AS (
              DELETE FROM app_world.map_sessions WHERE world_id=:w AND char_id IN (SELECT char_id FROM receipts) RETURNING session_key
            ), audited AS (
              UPDATE app_global."TLOG" SET "timeLOGOUT"=CURRENT_TIMESTAMP WHERE "dwKEY" IN (SELECT session_key FROM closed)
            ), released AS (
              DELETE FROM app_global."TCURRENTUSER" WHERE "dwUserID"=:u AND "dwKEY" IN (SELECT session_key FROM closed) RETURNING 1
            ) SELECT count(*)::integer FROM released)SQL",
                soci::use(m_world,"w"),soci::use(m_server,"s"),soci::use(user,"u"),soci::use(m_token,"t"),soci::into(recovered);
            m_recovered+=recovered;
        }
        m_recovered+=RecoverPreparedMapTransfers(sql,m_world,m_server,m_token);
        sql<<"UPDATE app_world.map_sessions SET phase='orphaned',updated_at=clock_timestamp() WHERE world_id=:w AND server_id=:s AND owner_token<>:t",
            soci::use(m_world,"w"),soci::use(m_server,"s"),soci::use(m_token,"t");
        sql<<"SELECT count(*) FROM app_world.map_sessions WHERE world_id=:w AND server_id=:s AND phase='orphaned'",
            soci::use(m_world,"w"),soci::use(m_server,"s"),soci::into(m_orphaned);
        // Replicas contain no unsaved authoritative state. A replacement may
        // retire only this target's old token, without clearing another Map's
        // primary claim or its current-user reservation.
        sql<<"DELETE FROM app_world.map_replicas WHERE world_id=:w AND target_server=:s AND target_token<>:t",
            soci::use(m_world,"w"),soci::use(m_server,"s"),soci::use(m_token,"t");
        tx.commit();
    }catch(const OwnerBusy&){throw;}catch(...){throw std::runtime_error("Native Map ownership claim failed");}
}
bool PostgreSQLMapOwner::Healthy(){
    try{int found=0;*m_lease<<"SELECT count(*) FROM app_world.map_runtime_owner WHERE world_id=:w AND server_id=:s "
        "AND owner_token=:t AND backend_pid=pg_backend_pid()",soci::use(m_world,"w"),soci::use(m_server,"s"),soci::use(m_token,"t"),soci::into(found);return found==1;}
    catch(...){return false;}
}
std::unique_ptr<soci::transaction> BeginMapTransaction(soci::session& sql,int world,int server,const std::string& token,bool repeatable_read){
    if(token.empty())throw std::runtime_error("Native Map write requires process ownership");
    auto tx=std::make_unique<soci::transaction>(sql);
    if(repeatable_read)sql<<"SET TRANSACTION ISOLATION LEVEL REPEATABLE READ";
    sql<<"SET LOCAL statement_timeout='5s'";sql<<"SET LOCAL lock_timeout='3s'";
    int found=0;sql<<"SELECT 1 FROM app_world.map_runtime_owner WHERE world_id=:w AND server_id=:s AND owner_token=:t FOR SHARE",
        soci::use(world,"w"),soci::use(server,"s"),soci::use(token,"t"),soci::into(found);
    if(!sql.got_data()||found!=1)throw std::runtime_error("Native Map ownership lost");
    return tx;
}
}
