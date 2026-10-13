#include "postgresql_world_owner.h"
#include <openssl/rand.h>
#include <stdexcept>
namespace tworldsvr {
namespace {constexpr int kWorldNamespace=0x34574c44;}
PostgreSQLWorldOwner::PostgreSQLWorldOwner(const std::string& connection,std::uint8_t world)
 :m_control(fourstory::db::Backend::PostgreSQL,connection,1),m_lease(m_control.Acquire()),m_world(world) {
    try {
        auto& sql=*m_lease;sql<<"SET statement_timeout='5s'";sql<<"SET lock_timeout='3s'";
        int acquired=0;
        sql<<"SELECT CASE WHEN pg_try_advisory_lock(:ns,:w) THEN 1 ELSE 0 END",
            soci::use(kWorldNamespace,"ns"),soci::use(m_world,"w"),soci::into(acquired);
        if(!acquired)throw std::runtime_error("World already owned");
        unsigned char bytes[32];if(RAND_bytes(bytes,sizeof(bytes))!=1)throw std::runtime_error("Owner randomness unavailable");
        constexpr char hex[]="0123456789abcdef";for(auto b:bytes){m_token+=hex[b>>4];m_token+=hex[b&15];}
        sql<<"SELECT pg_backend_pid()",soci::into(m_pid);
        soci::transaction tx(sql);
        sql<<"INSERT INTO app_world.world_runtime_owner(world_id,owner_token,backend_pid) VALUES(:w,:t,:p) "
             "ON CONFLICT(world_id) DO UPDATE SET owner_token=EXCLUDED.owner_token,backend_pid=EXCLUDED.backend_pid,acquired_at=clock_timestamp()",
             soci::use(m_world,"w"),soci::use(m_token,"t"),soci::use(m_pid,"p");
        tx.commit();
    } catch(...) {throw std::runtime_error("Native World ownership claim failed; no retry");}
}
bool PostgreSQLWorldOwner::Healthy() {
    try {int found=0;*m_lease<<"SELECT count(*) FROM app_world.world_runtime_owner o WHERE o.world_id=:w "
        "AND o.owner_token=:t AND o.backend_pid=pg_backend_pid() AND EXISTS(SELECT 1 FROM pg_locks l "
        "WHERE l.pid=o.backend_pid AND l.locktype='advisory' AND l.classid=CAST(:ns AS oid) "
        "AND l.objid=CAST(:w AS oid) AND l.objsubid=2 AND l.granted)",
        soci::use(m_world,"w"),soci::use(m_token,"t"),soci::use(kWorldNamespace,"ns"),soci::into(found);return found==1;}
    catch(...) {return false;}
}
std::unique_ptr<soci::transaction> BeginWorldTransaction(soci::session& sql,int world,const std::string& token,bool snapshot) {
    if(token.empty())throw std::runtime_error("Native World transaction requires ownership");
    auto tx=std::make_unique<soci::transaction>(sql);
    if(snapshot)sql<<"SET TRANSACTION ISOLATION LEVEL REPEATABLE READ";
    sql<<"SET LOCAL statement_timeout='10s'";sql<<"SET LOCAL lock_timeout='3s'";
    int found=0;
    sql<<"SELECT 1 FROM app_world.world_runtime_owner o WHERE o.world_id=:w AND o.owner_token=:t "
        "AND EXISTS(SELECT 1 FROM pg_locks l WHERE l.pid=o.backend_pid AND l.locktype='advisory' "
        "AND l.classid=CAST(:ns AS oid) AND l.objid=CAST(:w AS oid) AND l.objsubid=2 AND l.granted) FOR SHARE OF o",
        soci::use(world,"w"),soci::use(token,"t"),soci::use(kWorldNamespace,"ns"),soci::into(found);
    if(!sql.got_data()||found!=1)throw std::runtime_error("Native World ownership lost");
    return tx;
}
}
