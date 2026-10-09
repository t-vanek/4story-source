#include "postgresql_map_service.h"
#include "postgresql_map_owner.h"
#include <openssl/sha.h>
#include <soci/soci.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace tmapsvr {
namespace {
auto OrderedSkills(const CharSnapshot& s) {
    if(!s.payload||s.payload->skills.size()>255)throw std::runtime_error("Invalid learned skill checkpoint");
    auto rows=s.payload->skills;
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){return a.wSkillID<b.wSkillID;});
    for(std::size_t i=1;i<rows.size();++i)if(rows[i-1].wSkillID==rows[i].wSkillID)
        throw std::runtime_error("Duplicate learned skill checkpoint");
    return rows;
}
std::string SkillJson(const CharSnapshot& s) {
    std::string json="[";
    for(const auto& row:OrderedSkills(s)) {
        if(json.size()>1)json+=',';
        json+='['+std::to_string(row.wSkillID)+','+std::to_string(row.bLevel)+','+std::to_string(row.dwRemainTick)+']';
    }
    return json+']';
}
std::string Fingerprint(const MapSessionClaim& claim,const CharSnapshot& s) {
    if(s.persistence_uncertain||s.dwCharID!=claim.char_id||!s.payload||!std::isfinite(s.fPosX)||!std::isfinite(s.fPosY)||!std::isfinite(s.fPosZ))
        throw std::runtime_error("Invalid native character checkpoint");
    // Recovery contract v1: exact, ordered little-endian core values. Never hash
    // struct padding, pointers, derived stats or timestamps assigned by PostgreSQL.
    std::vector<unsigned char> bytes;
    const auto append=[&]<class T>(T value){
        using U=std::make_unsigned_t<T>;U v=static_cast<U>(value);
        for(std::size_t i=0;i<sizeof(T);++i){bytes.push_back(static_cast<unsigned char>(v&255));v>>=8;}
    };
    append(s.bLevel);append(s.dwEXP);append(s.dwHP);append(s.dwMP);
    append(s.dwGold);append(s.dwSilver);append(s.dwCooper);append(s.wSkillPoint);append(s.dwRegion);
    append(s.wMapID);append(s.wSpawnID);append(s.wLastSpawnID);append(s.dwLastDestination);append(s.wTemptedMon);
    append(s.bAftermath);append(s.bStartAct);append(std::bit_cast<std::uint32_t>(s.fPosX));
    append(std::bit_cast<std::uint32_t>(s.fPosY));append(std::bit_cast<std::uint32_t>(s.fPosZ));append(s.wDIR);
    append(s.bStatLevel);append(s.bStatPoint);append(s.dwStatExp);
    unsigned char digest[SHA256_DIGEST_LENGTH];SHA256(bytes.data(),bytes.size(),digest);
    constexpr char hex[]="0123456789abcdef";std::string result;
    for(auto b:digest){result+=hex[b>>4];result+=hex[b&15];}return result;
}
struct Receipt {bool found=false,core_matches=false;long long revision=0;int contract=0;std::string fingerprint,outcome,transfer_hash;};
Receipt ReadReceipt(soci::session& sql,const MapSessionClaim& claim,int server,const std::string& token){
    const int world=claim.group;const long long cid=claim.char_id,uid=claim.user_id,key=claim.key,generation=claim.connection_id,epoch=claim.authority_epoch;
    Receipt r;int matches=0;
    sql<<"SELECT revision,fingerprint,outcome,CASE WHEN app_world.map_checkpoint_matches(map_checkpoints) THEN 1 ELSE 0 END,COALESCE(transfer_hash,''),recovery_contract "
         "FROM app_world.map_checkpoints WHERE world_id=:w AND char_id=:c AND user_id=:u AND session_key=:k "
         "AND server_id=:s AND owner_token=:t AND connection_id=:g AND authority_epoch=:epoch AND recovery_contract IN (1,2,3) FOR UPDATE",
        soci::use(world,"w"),soci::use(cid,"c"),soci::use(uid,"u"),soci::use(key,"k"),soci::use(server,"s"),
        soci::use(token,"t"),soci::use(generation,"g"),soci::use(epoch,"epoch"),soci::into(r.revision),soci::into(r.fingerprint),soci::into(r.outcome),soci::into(matches),soci::into(r.transfer_hash),soci::into(r.contract);
    r.found=sql.got_data();r.core_matches=matches==1;return r;
}
}
bool PostgreSQLMapService::SkillCheckpointMatches(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) {
    const int world=c.group,character=c.char_id;const auto json=SkillJson(s);int matches=0;
    sql<<"SELECT CASE WHEN app_world.map_skill_state(CAST(:w AS smallint),CAST(:c AS integer))=CAST(:skills AS jsonb) THEN 1 ELSE 0 END",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(json,"skills"),soci::into(matches);
    return matches==1;
}
void PostgreSQLMapService::StoreSkillCheckpoint(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) {
    const int world=c.group,character=c.char_id;const auto skills=OrderedSkills(s);
    // This operation only persists timers. Learning, forgetting or changing a
    // rank must not be smuggled through a core save. Lock and validate every row
    // before writing, preserving the source signed SMALLINT/INT bit patterns.
    std::size_t n=0;
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT \"wSkillID\",\"bLevel\" FROM app_world.\"TSKILLTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c ORDER BY (\"wSkillID\"::integer & 65535) FOR UPDATE",
        soci::use(world,"w"),soci::use(character,"c"));
        for(const auto& row:rows) {
            if(n>=skills.size()||static_cast<std::uint16_t>(row.get<int>(0))!=skills[n].wSkillID||row.get<int>(1)!=skills[n].bLevel)
                throw std::runtime_error("Learned skills changed outside cooldown checkpoint");
            ++n;
        }
    }
    if(n!=skills.size())throw std::runtime_error("Learned skills missing from cooldown checkpoint");
    for(const auto& row:skills) {
        const int id=std::bit_cast<std::int16_t>(row.wSkillID),remaining=std::bit_cast<std::int32_t>(row.dwRemainTick);
        sql<<"UPDATE app_world.\"TSKILLTABLE\" SET \"dwRemainTick\"=:r WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c AND \"wSkillID\"=:id",
            soci::use(remaining,"r"),soci::use(world,"w"),soci::use(character,"c"),soci::use(id,"id");
    }
    sql<<"UPDATE app_world.map_checkpoints SET recovery_contract=3,skill_state=app_world.map_skill_state(world_id,char_id) WHERE world_id=:w AND char_id=:c",
        soci::use(world,"w"),soci::use(character,"c");
}
std::string PostgreSQLMapService::CoreFingerprint(const MapSessionClaim& c,const CharSnapshot& s) const {return Fingerprint(c,s);}
void PostgreSQLMapService::RecordCheckpoint(soci::session& sql,const MapSessionClaim& claim,long long revision,
                                          const std::string& fingerprint,const std::string& outcome) const {
    const int world=claim.group,server=m_config.server;const long long cid=claim.char_id,uid=claim.user_id,key=claim.key,generation=claim.connection_id,epoch=claim.authority_epoch;
    int stored=0;
    sql<<"INSERT INTO app_world.map_checkpoints(world_id,char_id,user_id,server_id,session_key,owner_token,connection_id,authority_epoch,revision,"
         "fingerprint,recovery_contract,core_state,outcome) VALUES(:w,:c,:u,:s,:k,:t,:g,:epoch,:r,:f,1,app_world.map_core_state(CAST(:w AS smallint),CAST(:c AS integer)),:o) "
         "ON CONFLICT(world_id,char_id) DO UPDATE SET user_id=EXCLUDED.user_id,server_id=EXCLUDED.server_id,"
         "session_key=EXCLUDED.session_key,owner_token=EXCLUDED.owner_token,connection_id=EXCLUDED.connection_id,authority_epoch=EXCLUDED.authority_epoch,"
         "revision=EXCLUDED.revision,fingerprint=EXCLUDED.fingerprint,recovery_contract=1,skill_state=NULL,core_state=EXCLUDED.core_state,"
         "saved_at=clock_timestamp(),outcome=EXCLUDED.outcome,recovered_at=NULL,transfer_body=NULL,transfer_hash=NULL,character_manifest=NULL,routing_manifest=NULL,actor_manifest=NULL "
         "WHERE app_world.map_checkpoints.outcome IN ('logout','recovered') OR "
         "(app_world.map_checkpoints.owner_token=EXCLUDED.owner_token AND app_world.map_checkpoints.connection_id=EXCLUDED.connection_id AND app_world.map_checkpoints.authority_epoch=EXCLUDED.authority_epoch) RETURNING 1",
        soci::use(world,"w"),soci::use(cid,"c"),soci::use(uid,"u"),soci::use(server,"s"),soci::use(key,"k"),
        soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(epoch,"epoch"),soci::use(revision,"r"),soci::use(fingerprint,"f"),soci::use(outcome,"o"),soci::into(stored);
    if(!sql.got_data()||stored!=1)throw std::runtime_error("Conflicting native checkpoint ownership");
}
void PostgreSQLMapService::MarkReady(const MapSessionClaim& claim,const CharSnapshot& s){
    if(claim.role==MapSessionRole::Replica){MarkReplicaReady(claim);return;}
    const auto fingerprint=Fingerprint(claim,s);
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;
    if(!LockAccount(sql,claim)||!LockClaim(sql,claim,phase)||phase!="loaded")throw std::runtime_error("Map admission claim is not loaded");
    const long long key=claim.key;int n=0;
    sql<<"SELECT 1 FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\"=:k AND \"bLocked\"=0",soci::use(key),soci::into(n);
    if(!sql.got_data())throw std::runtime_error("Map admission session was revoked");
    WriteCore(sql,claim,s,0);RecordCheckpoint(sql,claim,0,fingerprint,"active");StoreTransferCheckpoint(sql,claim,s);
    sql<<"UPDATE app_world.map_sessions SET phase='ready',updated_at=clock_timestamp() WHERE session_key=:k",soci::use(key);
    tx->commit();
}
void PostgreSQLMapService::CheckpointAuthorized(const MapSessionClaim& claim,const CharSnapshot& s,std::uint64_t revision){
    if(claim.role!=MapSessionRole::Primary)throw std::runtime_error("Replica cannot checkpoint primary state");
    const auto fingerprint=Fingerprint(claim,s);
    if(!revision||revision>static_cast<std::uint64_t>(std::numeric_limits<long long>::max()))throw std::runtime_error("Invalid checkpoint revision");
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;
    if(!LockAccount(sql,claim)||!LockClaim(sql,claim,phase)||phase!="ready")throw std::runtime_error("Checkpoint claim is not ready");
    const auto r=ReadReceipt(sql,claim,m_config.server,m_config.owner_token);
    if(!r.found||!r.core_matches||r.outcome!="active")throw std::runtime_error("Native checkpoint receipt missing or durable state drifted");
    if(revision==static_cast<std::uint64_t>(r.revision)&&fingerprint==r.fingerprint&&TransferFingerprint(claim,s)==r.transfer_hash&&(r.contract!=3||SkillCheckpointMatches(sql,claim,s))){tx->commit();return;}
    if(revision!=static_cast<std::uint64_t>(r.revision)+1)throw std::runtime_error("Native checkpoint revision conflict");
    WriteCore(sql,claim,s,0);RecordCheckpoint(sql,claim,static_cast<long long>(revision),fingerprint,"active");StoreTransferCheckpoint(sql,claim,s);tx->commit();
}
void PostgreSQLMapService::SaveAuthorized(const MapSessionClaim& claim,const CharSnapshot& s){
    if(s.persistence_uncertain)throw std::runtime_error("Item transaction outcome requires process recovery");
    if(claim.role!=MapSessionRole::Primary)throw std::runtime_error("Replica cannot save primary state");
    const auto fingerprint=Fingerprint(claim,s);
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    if(!LockAccount(sql,claim))throw std::runtime_error("Native save account invalid");
    const auto r=ReadReceipt(sql,claim,m_config.server,m_config.owner_token);
    // Read-only confirmation of the exact previous final commit. A different
    // payload or later connection cannot masquerade as an acknowledged save.
    if(r.found&&r.outcome=="logout"&&r.fingerprint==fingerprint&&r.core_matches&&TransferFingerprint(claim,s)==r.transfer_hash&&(r.contract!=3||SkillCheckpointMatches(sql,claim,s))){tx->commit();return;}
    std::string phase;
    if(!LockClaim(sql,claim,phase)) {
        // An outgoing frozen source may close after the target committed. Only
        // its exact transferred core receipt can confirm that ownership left;
        // no target row, account reservation or checkpoint is written here.
        const int world=claim.group,character=claim.char_id,user=claim.user_id,key=claim.key;
        const long long generation=claim.connection_id,epoch=claim.authority_epoch;int confirmed=0;
        sql<<"SELECT 1 FROM app_world.map_transfers WHERE world_id=:w AND char_id=:c AND user_id=:u AND session_key=:k "
             "AND source_token=:t AND source_connection=:g AND source_epoch=:e AND core_fingerprint=:f AND phase='consumed' LIMIT 1",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(user,"u"),soci::use(key,"k"),soci::use(m_config.owner_token,"t"),
            soci::use(generation,"g"),soci::use(epoch,"e"),soci::use(fingerprint,"f"),soci::into(confirmed);
        if(!sql.got_data())throw std::runtime_error("Native save ownership changed without transfer receipt");
        tx->commit();return;
    }
    if((phase!="ready"&&phase!="transferring")||!r.found||r.outcome!="active"||!r.core_matches||
       r.revision==std::numeric_limits<long long>::max())throw std::runtime_error("Native save claim or checkpoint invalid");
    if(phase=="transferring") {
        const int world=claim.group,character=claim.char_id;const long long generation=claim.connection_id,epoch=claim.authority_epoch;
        int cancelled=0;
        sql<<"UPDATE app_world.map_transfers SET phase='cancelled',settled_at=clock_timestamp() "
             "WHERE world_id=:w AND char_id=:c AND source_token=:t AND source_connection=:g AND source_epoch=:e "
             "AND core_fingerprint=:f AND phase='prepared' RETURNING 1",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),
            soci::use(epoch,"e"),soci::use(fingerprint,"f"),soci::into(cancelled);
        if(!sql.got_data())throw std::runtime_error("Frozen transfer changed before abort save");
    }
    WriteCore(sql,claim,s,1);RecordCheckpoint(sql,claim,r.revision+1,fingerprint,"logout");
    if(phase=="transferring") {
        // Cancellation saves the prepared graph itself, including fields absent
        // from the core/client projection. Its exact core was checked above.
        const int world=claim.group,character=claim.char_id;const long long generation=claim.connection_id,epoch=claim.authority_epoch;
        sql<<"UPDATE app_world.map_checkpoints p SET recovery_contract=2,skill_state=NULL,transfer_body=t.body,transfer_hash=t.body_sha256,"
             "character_manifest=t.character_manifest,routing_manifest=t.routing_manifest,actor_manifest=t.actor_manifest "
             "FROM app_world.map_transfers t WHERE t.world_id=:w AND t.char_id=:c AND t.source_token=:token "
             "AND t.source_connection=:g AND t.source_epoch=:e AND t.phase='cancelled' "
             "AND p.world_id=t.world_id AND p.char_id=t.char_id",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(m_config.owner_token,"token"),soci::use(generation,"g"),soci::use(epoch,"e");
    }else StoreTransferCheckpoint(sql,claim,s);
    CloseClaim(sql,claim);tx->commit();
}
std::string PostgreSQLMapService::ConsumeSkillItem(const MapSessionClaim& c,std::uint16_t skill,
    const ItemInstance& before,const CharSnapshot& after) {
    if(c.role!=MapSessionRole::Primary||c.authority_epoch||!after.payload||after.payload->transfer_state||
       !before.bCount||before.bInvenID==254||before.durable_hash.size()!=64||!before.dlID||
       before.dlID>static_cast<std::uint64_t>(std::numeric_limits<long long>::max()))
        throw std::runtime_error("Unsupported native reagent transaction");
    if(std::none_of(after.payload->skills.begin(),after.payload->skills.end(),[&](const auto& row){return row.wSkillID==skill&&row.bLevel;}))
        throw std::runtime_error("Reagent skill is not learned");
    unsigned found=0;
    for(const auto& bag:after.payload->bags)for(const auto& item:bag.items)if(item.dlID==before.dlID) {
        ++found;
        if(bag.bag.bInvenID!=before.bInvenID||item.bItemID!=before.bItemID||item.wItemID!=before.wItemID||
           item.bCount+1!=before.bCount)throw std::runtime_error("Reagent projection differs from exact decrement");
    }
    if(found!=(before.bCount>1?1U:0U))throw std::runtime_error("Reagent projection has wrong item cardinality");
    const auto fingerprint=Fingerprint(c,after);
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;
    if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||phase!="ready")throw std::runtime_error("Reagent claim is not ready primary");
    CheckCatalogs(sql);
    const auto receipt=ReadReceipt(sql,c,m_config.server,m_config.owner_token);
    if(!receipt.found||!receipt.core_matches||receipt.outcome!="active"||receipt.contract!=3)
        throw std::runtime_error("Reagent recovery receipt changed");
    const long long session_key=c.key;int unlocked=0;
    sql<<"SELECT 1 FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\"=:k AND \"bLocked\"=0",soci::use(session_key),soci::into(unlocked);
    if(!sql.got_data())throw std::runtime_error("Reagent session was revoked");
    const int world=c.group,character=c.char_id,slot=before.bItemID,bag=before.bInvenID;
    const int item=std::bit_cast<std::int16_t>(before.wItemID),skill_id=std::bit_cast<std::int16_t>(skill);
    const long long id=before.dlID;const int count=before.bCount;int matched=0;
    sql<<"SELECT 1 FROM character_compat.\"TSKILLCHART\" WHERE \"wID\"=:s AND \"wItemID\"=:i AND \"dwWeaponID\"=0",
        soci::use(skill_id,"s"),soci::use(item,"i"),soci::into(matched);
    if(!sql.got_data())throw std::runtime_error("Reagent requirement differs from pinned chart");
    std::string hash;
    sql<<"SELECT app_world.item_fingerprint(i) FROM app_world.\"TITEMTABLE\" i WHERE \"bWorldID\"=:w AND \"dlID\"=:id "
         "AND \"dwOwnerID\"=:c AND \"bOwnerType\"=0 AND \"bStorageType\"=0 AND \"dwStorageID\"=:bag AND \"bItemID\"=:slot "
         "AND \"wItemID\"=:item AND \"bCount\"=:count FOR UPDATE",
        soci::use(world,"w"),soci::use(id,"id"),soci::use(character,"c"),soci::use(bag,"bag"),soci::use(slot,"slot"),
        soci::use(item,"item"),soci::use(count,"count"),soci::into(hash);
    if(!sql.got_data()||hash!=before.durable_hash)throw std::runtime_error("Reagent item changed since hydration");
    std::string after_hash;
    if(count==1)sql<<"DELETE FROM app_world.\"TITEMTABLE\" WHERE \"bWorldID\"=:w AND \"dlID\"=:id",soci::use(world,"w"),soci::use(id,"id");
    else sql<<"UPDATE app_world.\"TITEMTABLE\" i SET \"bCount\"=\"bCount\"-1 WHERE \"bWorldID\"=:w AND \"dlID\"=:id RETURNING app_world.item_fingerprint(i)",
        soci::use(world,"w"),soci::use(id,"id"),soci::into(after_hash);
    WriteCore(sql,c,after,0);
    // This is an immediate gameplay receipt, not a periodic revision. The
    // runtime checkpoint lease prevents an older sweep overwriting this state.
    RecordCheckpoint(sql,c,receipt.revision,fingerprint,"active");StoreSkillCheckpoint(sql,c,after);
    const int server=m_config.server,unsigned_skill=skill,remaining=count-1;
    const long long generation=c.connection_id,epoch=c.authority_epoch;
    sql<<"INSERT INTO app_world.skill_item_consumptions(world_id,char_id,server_id,owner_token,connection_id,authority_epoch,skill_id,item_id,"
         "before_count,after_count,before_hash,after_hash,core_fingerprint) VALUES(:w,:c,:s,:t,:g,:e,:skill,:id,:before,:after,:bh,NULLIF(:ah,''),:f)",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(server,"s"),soci::use(m_config.owner_token,"t"),
        soci::use(generation,"g"),soci::use(epoch,"e"),soci::use(unsigned_skill,"skill"),soci::use(id,"id"),
        soci::use(count,"before"),soci::use(remaining,"after"),soci::use(before.durable_hash,"bh"),soci::use(after_hash,"ah"),soci::use(fingerprint,"f");
    tx->commit();return after_hash;
}
void PostgreSQLMapService::WriteCore(soci::session& sql,const MapSessionClaim& claim,const CharSnapshot& s,int logout) {
    const int world=claim.group;const long long character=claim.char_id,user=claim.user_id;
    // Core snapshot only. Durable children are never deleted/recreated here.
    // Signed bindings reproduce SQL Server SMALLINT/INT storage of wire bits.
    const int bLevel=s.bLevel;
    const int dwEXP=std::bit_cast<std::int32_t>(s.dwEXP);
    const int dwHP=std::bit_cast<std::int32_t>(s.dwHP);
    const int dwMP=std::bit_cast<std::int32_t>(s.dwMP);
    const int dwGold=std::bit_cast<std::int32_t>(s.dwGold);
    const int dwSilver=std::bit_cast<std::int32_t>(s.dwSilver);
    const int dwCooper=std::bit_cast<std::int32_t>(s.dwCooper);
    const int wSkillPoint=std::bit_cast<std::int16_t>(s.wSkillPoint);
    const int dwRegion=std::bit_cast<std::int32_t>(s.dwRegion);
    const int wMapID=std::bit_cast<std::int16_t>(s.wMapID);
    const int wSpawnID=std::bit_cast<std::int16_t>(s.wSpawnID);
    const int wLastSpawnID=std::bit_cast<std::int16_t>(s.wLastSpawnID);
    const int dwLastDestination=std::bit_cast<std::int32_t>(s.dwLastDestination);
    const int wTemptedMon=std::bit_cast<std::int16_t>(s.wTemptedMon);
    const int bAftermath=s.bAftermath;
    const int bStartAct=s.bStartAct;
    const double fPosX=s.fPosX;
    const double fPosY=s.fPosY;
    const double fPosZ=s.fPosZ;
    const int wDIR=std::bit_cast<std::int16_t>(s.wDIR);
    const int bStatLevel=s.bStatLevel;
    const int bStatPoint=s.bStatPoint;
    const int dwStatExp=std::bit_cast<std::int32_t>(s.dwStatExp);
    int updated=0;
    sql<<R"SQL(UPDATE app_world."TCHARTABLE" SET "bLevel"=:bLevel,"dwEXP"=:dwEXP,"dwHP"=:dwHP,"dwMP"=:dwMP,"dwGold"=:dwGold,"dwSilver"=:dwSilver,"dwCooper"=:dwCooper,"wSkillPoint"=:wSkillPoint,"dwRegion"=:dwRegion,"wMapID"=:wMapID,"wSpawnID"=:wSpawnID,"wLastSpawnID"=:wLastSpawnID,"dwLastDestination"=:dwLastDestination,"wTemptedMon"=:wTemptedMon,"bAftermath"=:bAftermath,"bStartAct"=:bStartAct,"fPosX"=:fPosX,"fPosY"=:fPosY,"fPosZ"=:fPosZ,"wDIR"=:wDIR,"bStatLevel"=:bStatLevel,"bStatPoint"=:bStatPoint,"dwStatExp"=:dwStatExp,"dLogoutDate"=CASE WHEN :logout=1 THEN date_trunc('minute',timezone('UTC',CURRENT_TIMESTAMP)+interval '30 seconds') ELSE "dLogoutDate" END WHERE "bWorldID"=:w AND "dwCharID"=:c AND "dwUserID"=:u AND "bDelete"=0 RETURNING 1)SQL",
        soci::use(logout,"logout"),soci::use(bLevel,"bLevel"),
        soci::use(dwEXP,"dwEXP"),
        soci::use(dwHP,"dwHP"),
        soci::use(dwMP,"dwMP"),
        soci::use(dwGold,"dwGold"),
        soci::use(dwSilver,"dwSilver"),
        soci::use(dwCooper,"dwCooper"),
        soci::use(wSkillPoint,"wSkillPoint"),
        soci::use(dwRegion,"dwRegion"),
        soci::use(wMapID,"wMapID"),
        soci::use(wSpawnID,"wSpawnID"),
        soci::use(wLastSpawnID,"wLastSpawnID"),
        soci::use(dwLastDestination,"dwLastDestination"),
        soci::use(wTemptedMon,"wTemptedMon"),
        soci::use(bAftermath,"bAftermath"),
        soci::use(bStartAct,"bStartAct"),
        soci::use(fPosX,"fPosX"),
        soci::use(fPosY,"fPosY"),
        soci::use(fPosZ,"fPosZ"),
        soci::use(wDIR,"wDIR"),
        soci::use(bStatLevel,"bStatLevel"),
        soci::use(bStatPoint,"bStatPoint"),
        soci::use(dwStatExp,"dwStatExp"),
        soci::use(world,"w"),soci::use(character,"c"),soci::use(user,"u"),soci::into(updated);
    if(!sql.got_data()||updated!=1)throw std::runtime_error("Character save owner changed");
    sql<<"SELECT 1 FROM app_global.\"TALLCHARTABLE\" WHERE \"bWorldID\"=:w "
         "AND \"dwCharID\"=:c AND \"dwUserID\"=:u AND \"bDelete\"=0",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(user,"u"),soci::into(updated);
    if(!sql.got_data()||updated!=1)throw std::runtime_error("Character directory save owner changed");
}

}
