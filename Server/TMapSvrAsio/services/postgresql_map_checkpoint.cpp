#include "postgresql_map_service.h"
#include "native_npc_discount.h"
#include "postgresql_map_owner.h"
#include "maintained_effects.h"
#include "posture_effects.h"
#include "skill_effect.h"
#include "skill_engine.h"
#include "skill_cooldown.h"
#include "quest_conditions.h"
#include "MessageId.h"
#include "domain/main_transfer.h"
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <soci/soci.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <ctime>
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
         "AND server_id=:s AND owner_token=:t AND connection_id=:g AND authority_epoch=:epoch AND recovery_contract IN (1,2,3,4) FOR UPDATE",
        soci::use(world,"w"),soci::use(cid,"c"),soci::use(uid,"u"),soci::use(key,"k"),soci::use(server,"s"),
        soci::use(token,"t"),soci::use(generation,"g"),soci::use(epoch,"epoch"),soci::into(r.revision),soci::into(r.fingerprint),soci::into(r.outcome),soci::into(matches),soci::into(r.transfer_hash),soci::into(r.contract);
    r.found=sql.got_data();r.core_matches=matches==1;return r;
}
}
bool PostgreSQLMapService::MaintainCheckpointMatches(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) {
    const int world=c.group,character=c.char_id;const auto json=MaintainJson(s);int matches=0;
    sql<<"SELECT CASE WHEN app_world.map_maintain_state(CAST(:w AS smallint),CAST(:c AS integer))=CAST(:effects AS jsonb) THEN 1 ELSE 0 END",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(json,"effects"),soci::into(matches);
    return matches==1;
}
void PostgreSQLMapService::WriteMaintainedEffects(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) {
    const int world=c.group,character=c.char_id;const auto json=MaintainJson(s);
    sql<<"DELETE FROM app_world.map_maintained_effects WHERE world_id=:w AND char_id=:c",soci::use(world,"w"),soci::use(character,"c");
    sql<<"INSERT INTO app_world.map_maintained_effects(world_id,char_id,ordinal,skill_id,skill_level,remaining,attack_type,attack_id,host_type,host_id,attack_country) "
         "SELECT :w,:c,n-1,(e->>0)::integer,(e->>1)::smallint,(e->>2)::bigint,(e->>3)::smallint,(e->>4)::bigint,(e->>5)::smallint,(e->>6)::bigint,(e->>7)::smallint "
         "FROM jsonb_array_elements(CAST(:effects AS jsonb)) WITH ORDINALITY AS a(e,n)",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(json,"effects");
}
bool PostgreSQLMapService::SkillCheckpointMatches(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) {
    const int world=c.group,character=c.char_id;const auto json=SkillJson(s);int matches=0;
    sql<<"SELECT CASE WHEN app_world.map_skill_state(CAST(:w AS smallint),CAST(:c AS integer))=CAST(:skills AS jsonb) THEN 1 ELSE 0 END",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(json,"skills"),soci::into(matches);
    if(matches!=1||!MaintainCheckpointMatches(sql,c,s)||!ItemCooldownCheckpointMatches(sql,c,s))return false;
    return s.payload&&s.payload->quests&&quest_runtime::Json(*s.payload->quests)==quest_runtime::Json(ReadQuestProgress(sql,c));
}
void PostgreSQLMapService::StoreSkillCheckpoint(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) {
    WriteQuestTimers(sql,c,s);
    CheckItemCooldowns(sql,c,s);
    WriteItemCooldowns(sql,c,s);
    const int world=c.group,character=c.char_id;const auto skills=OrderedSkills(s);
    if(!MaintainCheckpointMatches(sql,c,s))throw std::runtime_error("Core checkpoint cannot mutate maintained effects");
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
    sql<<"UPDATE app_world.map_checkpoints SET recovery_contract=CASE WHEN app_world.map_maintain_state(world_id,char_id)='[]'::jsonb THEN 3 ELSE 4 END,"
         "maintain_state=NULLIF(app_world.map_maintain_state(world_id,char_id),'[]'::jsonb),skill_state=app_world.map_skill_state(world_id,char_id),quest_state=app_world.map_quest_state(world_id,char_id),item_cooldown_state=app_world.map_item_cooldown_state(world_id,char_id) WHERE world_id=:w AND char_id=:c",
        soci::use(world,"w"),soci::use(character,"c");
}
std::string PostgreSQLMapService::CoreFingerprint(const MapSessionClaim& c,const CharSnapshot& s) const {return Fingerprint(c,s);}
void PostgreSQLMapService::RecordCheckpoint(soci::session& sql,const MapSessionClaim& claim,long long revision,
                                          const std::string& fingerprint,const std::string& outcome) const {
    const int world=claim.group,server=m_config.server;const long long cid=claim.char_id,uid=claim.user_id,key=claim.key,generation=claim.connection_id,epoch=claim.authority_epoch;
    int stored=0;
    sql<<"INSERT INTO app_world.map_checkpoints(world_id,char_id,user_id,server_id,session_key,owner_token,connection_id,authority_epoch,revision,"
         "fingerprint,recovery_contract,core_state,during_state,item_cooldown_state,outcome) VALUES(:w,:c,:u,:s,:k,:t,:g,:epoch,:r,:f,1,app_world.map_core_state(CAST(:w AS smallint),CAST(:c AS integer)),app_world.map_exp_state(CAST(:w AS smallint),CAST(:c AS integer)),app_world.map_item_cooldown_state(CAST(:w AS smallint),CAST(:c AS integer)),:o) "
         "ON CONFLICT(world_id,char_id) DO UPDATE SET user_id=EXCLUDED.user_id,server_id=EXCLUDED.server_id,"
         "session_key=EXCLUDED.session_key,owner_token=EXCLUDED.owner_token,connection_id=EXCLUDED.connection_id,authority_epoch=EXCLUDED.authority_epoch,"
         "revision=EXCLUDED.revision,fingerprint=EXCLUDED.fingerprint,recovery_contract=1,skill_state=NULL,maintain_state=NULL,core_state=EXCLUDED.core_state,during_state=EXCLUDED.during_state,item_cooldown_state=EXCLUDED.item_cooldown_state,"
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
    CheckCastHead(sql,claim,s);
    const auto r=ReadReceipt(sql,claim,m_config.server,m_config.owner_token);
    if(!r.found||!r.core_matches||r.outcome!="active")throw std::runtime_error("Native checkpoint receipt missing or durable state drifted");
    if(revision==static_cast<std::uint64_t>(r.revision)&&fingerprint==r.fingerprint&&TransferFingerprint(claim,s)==r.transfer_hash&&((r.contract!=3&&r.contract!=4)||SkillCheckpointMatches(sql,claim,s))){tx->commit();return;}
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
    if(r.found&&r.outcome=="logout"&&r.fingerprint==fingerprint&&r.core_matches&&TransferFingerprint(claim,s)==r.transfer_hash&&((r.contract!=3&&r.contract!=4)||SkillCheckpointMatches(sql,claim,s))){tx->commit();return;}
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
        sql<<"UPDATE app_world.map_checkpoints p SET recovery_contract=2,skill_state=NULL,maintain_state=NULL,transfer_body=t.body,transfer_hash=t.body_sha256,"
             "character_manifest=t.character_manifest,routing_manifest=t.routing_manifest,actor_manifest=t.actor_manifest "
             "FROM app_world.map_transfers t WHERE t.world_id=:w AND t.char_id=:c AND t.source_token=:token "
             "AND t.source_connection=:g AND t.source_epoch=:e AND t.phase='cancelled' "
             "AND p.world_id=t.world_id AND p.char_id=t.char_id",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(m_config.owner_token,"token"),soci::use(generation,"g"),soci::use(epoch,"e");
    }else StoreTransferCheckpoint(sql,claim,s);
    CloseClaim(sql,claim);tx->commit();
}
std::vector<std::string> PostgreSQLMapService::ConsumeSkillItems(const MapSessionClaim& c,std::uint16_t skill,
    std::uint8_t hits,const std::vector<SkillItemDebit>& debits,const CharSnapshot& after) {
    return CommitSkillMutation(c,skill,hits,debits,after,nullptr,nullptr,nullptr);
}
SkillCastCommit PostgreSQLMapService::CommitSkillCast(const MapSessionClaim& c,const SkillCastRequest& request,
    const CharSnapshot& before,const CharSnapshot& after) {
    const std::size_t fixed=request.loop?45:62;
    if(request.acknowledgement.size()<fixed)throw std::runtime_error("Invalid cast acknowledgement");
    SkillCastCommit result;
    CommitSkillMutation(c,request.skill,std::to_integer<std::uint8_t>(request.acknowledgement[fixed-1]),
        request.debits,after,&request,&before,&result);
    return result;
}
std::vector<std::string> PostgreSQLMapService::CommitSkillMutation(const MapSessionClaim& c,std::uint16_t skill,
    std::uint8_t hits,const std::vector<SkillItemDebit>& debits,const CharSnapshot& proposed,
    const SkillCastRequest* cast,const CharSnapshot* cast_before,SkillCastCommit* cast_result) {
    auto after=proposed;
    const bool graph=after.payload&&after.payload->transfer_state;
    if(c.role!=MapSessionRole::Primary||(!graph&&c.authority_epoch)||!after.payload||(!cast&&debits.empty())||debits.size()>16)
        throw std::runtime_error("Unsupported native consumption transaction");
    if(std::none_of(after.payload->skills.begin(),after.payload->skills.end(),[&](const auto& row){return row.wSkillID==skill&&row.bLevel;}))
        throw std::runtime_error("Consumption skill is not learned");
    std::vector<std::uint64_t> seen;
    for(const auto& debit:debits) {
        const auto& before=debit.before;
        if(!debit.count||debit.count>before.bCount||before.bInvenID==254||before.durable_hash.size()!=64||!before.dlID||
           (!graph&&before.dlID>static_cast<std::uint64_t>(std::numeric_limits<long long>::max()))||
           std::find(seen.begin(),seen.end(),before.dlID)!=seen.end())throw std::runtime_error("Invalid consumption stack debit");
        seen.push_back(before.dlID);unsigned found=0;
        for(const auto& bag:after.payload->bags)for(const auto& item:bag.items)if(item.dlID==before.dlID) {
            ++found;
            if(bag.bag.bInvenID!=before.bInvenID||item.bInvenID!=before.bInvenID||item.bItemID!=before.bItemID||item.wItemID!=before.wItemID||
               item.bCount+debit.count!=before.bCount)throw std::runtime_error("Consumption projection differs from exact debit");
        }
        if(found!=(before.bCount>debit.count?1U:0U))throw std::runtime_error("Consumption projection has wrong item cardinality");
    }
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;
    if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||phase!="ready")throw std::runtime_error("Reagent claim is not ready primary");
    CheckCatalogs(sql);
    const auto receipt=ReadReceipt(sql,c,m_config.server,m_config.owner_token);
    if(!receipt.found||!receipt.core_matches||receipt.outcome!="active"||(graph?receipt.contract!=2:(receipt.contract!=3&&receipt.contract!=4)))
        throw std::runtime_error("Reagent recovery receipt changed");
    const long long session_key=c.key;int unlocked=0;
    sql<<"SELECT 1 FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\"=:k AND \"bLocked\"=0",soci::use(session_key),soci::into(unlocked);
    if(!sql.got_data())throw std::runtime_error("Reagent session was revoked");
    const int world=c.group,character=c.char_id;
    ReagentGraphPlan graph_plan;std::vector<std::string> hashes;auto kind=ConsumptionKind::Reagent;
    if(cast) {
        graph_plan.before_hash=ValidateInventoryState(sql,c,*cast_before,true);
        after=ValidateSkillCast(sql,c,*cast,*cast_before,proposed);
    }else CheckCastHead(sql,c,after);
    if(graph&&!debits.empty()) {
        graph_plan=ValidateGraphReagent(sql,c,skill,hits,debits,after);hashes=graph_plan.item_hashes;kind=graph_plan.kind;
    }else if(!debits.empty()) {
        kind=ValidateSkillConsumption(sql,c,skill,hits,debits,after,nullptr);
        for(const auto& debit:debits) {
            const auto& before=debit.before;
            const int slot=before.bItemID,bag=before.bInvenID,item=std::bit_cast<std::int16_t>(before.wItemID),count=before.bCount,quantity=debit.count;
            const long long id=std::bit_cast<std::int64_t>(before.dlID);std::string hash,after_hash;
            sql<<"SELECT app_world.item_fingerprint(i) FROM app_world.\"TITEMTABLE\" i WHERE \"bWorldID\"=:w AND \"dlID\"=:id "
                 "AND \"dwOwnerID\"=:c AND \"bOwnerType\"=0 AND \"bStorageType\"=0 AND \"dwStorageID\"=:bag AND \"bItemID\"=:slot "
                 "AND \"wItemID\"=:item AND \"bCount\"=:count FOR UPDATE",
                soci::use(world,"w"),soci::use(id,"id"),soci::use(character,"c"),soci::use(bag,"bag"),soci::use(slot,"slot"),
                soci::use(item,"item"),soci::use(count,"count"),soci::into(hash);
            if(!sql.got_data()||hash!=before.durable_hash)throw std::runtime_error("Consumption item changed since hydration");
            if(count==quantity)sql<<"DELETE FROM app_world.\"TITEMTABLE\" WHERE \"bWorldID\"=:w AND \"dlID\"=:id",soci::use(world,"w"),soci::use(id,"id");
            else sql<<"UPDATE app_world.\"TITEMTABLE\" i SET \"bCount\"=\"bCount\"-:q WHERE \"bWorldID\"=:w AND \"dlID\"=:id RETURNING app_world.item_fingerprint(i)",
                soci::use(quantity,"q"),soci::use(world,"w"),soci::use(id,"id"),soci::into(after_hash);
            hashes.push_back(after_hash);
        }
    }
    long long accepted_cast=0;
    if(cast) {
        auto p=std::make_shared<CharacterPayload>(*after.payload);
        for(std::size_t i=0;i<debits.size();++i)for(auto& bag:p->bags)for(auto& item:bag.items)
            if(item.dlID==debits[i].before.dlID)item.durable_hash=hashes[i];
        after.payload=p;
        if(!cast->loop&&!after.dwMP) {
            if(!SupportedPostures(*p))throw std::runtime_error("Zero-MP cast requires supported eternal effect semantics");
            for(std::size_t i=0;i<after.payload->effects->size();) {
                if(!ErasesAtZeroMp(*after.payload,after.payload->effects->at(i).skill)){++i;continue;}
                p=std::make_shared<CharacterPayload>(*after.payload);
                const auto ended=p->effects->at(i).skill;p->effects->erase(p->effects->begin()+i);after.payload=p;
                RefreshEquipment(sql,after,true);
                cast_result->ended.push_back({false,ended,std::make_shared<const CharSnapshot>(after)});
            }
            if(!graph)WriteMaintainedEffects(sql,c,after);
        }
        if(graph)graph_plan.after_hash=TransferFingerprint(c,after);
        const auto hex=[](const auto& bytes){constexpr char digits[]="0123456789abcdef";std::string out;
            for(auto v:bytes){const auto b=std::to_integer<unsigned>(v);out+=digits[b>>4];out+=digits[b&15];}return out;};
        const auto request_hex=hex(cast->request),ack_hex=hex(cast->acknowledgement);
        const auto bs=SkillJson(*cast_before),as=SkillJson(after),be=MaintainJson(*cast_before),ae=MaintainJson(after);
        const auto cast_fingerprint=Fingerprint(c,after);
        const int server=m_config.server,contract=graph?2:3,id=skill;
        const auto learned=std::find_if(after.payload->skills.begin(),after.payload->skills.end(),[&](const auto& row){return row.wSkillID==skill;});
        const int rank=learned->bLevel;
        const long long generation=c.connection_id,epoch=c.authority_epoch,previous=cast_before->payload->last_cast_id;
        const long long bhp=cast_before->dwHP,ahp=after.dwHP,bmp=cast_before->dwMP,amp=after.dwMP;
        const int is_loop=cast->loop?1:0;
        long long charge_action=0;
        if(!cast->loop) {
            const int channel=c.channel,map=after.wMapID;
            // Only the latest successful ACTION in this login can begin a new
            // charge. Rejected ACTIONs preserve it; skill=0 explicitly cancels.
            sql<<"SELECT x.action_id FROM app_world.skill_action_operations x WHERE x.world_id=:w AND x.char_id=:c AND x.session_key=:k "
                 "AND x.action_id=(SELECT max(action_id) FROM app_world.skill_action_operations WHERE world_id=:w AND char_id=:c AND session_key=:k AND result=0) "
                 "AND x.skill_id=:skill AND x.charge_ms>0 AND x.channel_id=:channel AND x.map_id=:map "
                 "AND NOT EXISTS(SELECT 1 FROM app_world.accepted_skill_casts a WHERE a.charge_action_id=x.action_id)",
                soci::use(world,"w"),soci::use(character,"c"),soci::use(session_key,"k"),soci::use(id,"skill"),
                soci::use(channel,"channel"),soci::use(map,"map"),soci::into(charge_action);
        }
        sql<<"INSERT INTO app_world.accepted_skill_casts(world_id,char_id,server_id,owner_token,connection_id,session_key,authority_epoch,previous_cast_id,state_contract,skill_id,skill_rank,is_loop,request,acknowledgement,before_hp,after_hp,before_mp,after_mp,before_skills,after_skills,before_effects,after_effects,before_graph_hash,after_graph_hash,core_fingerprint,charge_action_id) "
             "VALUES(:w,:c,:s,:t,:g,:k,:e,:prev,:contract,:skill,:rank,:loop=1,decode(:req,'hex'),decode(:ack,'hex'),:bhp,:ahp,:bmp,:amp,CAST(:bs AS jsonb),CAST(:as AS jsonb),CAST(:be AS jsonb),CAST(:ae AS jsonb),NULLIF(:gb,''),NULLIF(:ga,''),:f,NULLIF(:charge,0)) RETURNING cast_id",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(server,"s"),soci::use(m_config.owner_token,"t"),
            soci::use(generation,"g"),soci::use(session_key,"k"),soci::use(epoch,"e"),soci::use(previous,"prev"),
            soci::use(contract,"contract"),soci::use(id,"skill"),soci::use(rank,"rank"),soci::use(is_loop,"loop"),
            soci::use(request_hex,"req"),soci::use(ack_hex,"ack"),soci::use(bhp,"bhp"),soci::use(ahp,"ahp"),soci::use(bmp,"bmp"),soci::use(amp,"amp"),
            soci::use(bs,"bs"),soci::use(as,"as"),soci::use(be,"be"),soci::use(ae,"ae"),
            soci::use(graph_plan.before_hash,"gb"),soci::use(graph_plan.after_hash,"ga"),soci::use(cast_fingerprint,"f"),soci::use(charge_action,"charge"),soci::into(accepted_cast);
        p=std::make_shared<CharacterPayload>(*after.payload);p->last_cast_id=static_cast<std::uint64_t>(accepted_cast);after.payload=std::move(p);
    }
    const auto fingerprint=Fingerprint(c,after);
    WriteCore(sql,c,after,0);
    // This is an immediate gameplay receipt, not a periodic revision. The
    // runtime checkpoint lease prevents an older sweep overwriting this state.
    RecordCheckpoint(sql,c,receipt.revision,fingerprint,"active");StoreTransferCheckpoint(sql,c,after);
    const int server=m_config.server,unsigned_skill=skill,contract=graph?2:3,hit_count=kind!=ConsumptionKind::Reagent?hits:1;
    const long long generation=c.connection_id,epoch=c.authority_epoch;
    const std::string consumption_kind=kind!=ConsumptionKind::Reagent?"ammunition":"reagent";
    const std::string hit_mode=kind==ConsumptionKind::MultiAttackAmmunition?"expanded":"direct";
    // Reserve one ID from the existing granted receipt sequence to group the
    // whole cast. Sequence gaps on rollback are intentional; never retry a cast.
    long long cast_id=0;
    if(!debits.empty())sql<<"SELECT nextval('app_world.skill_item_consumptions_consumption_id_seq')",soci::into(cast_id);
    for(std::size_t i=0;i<debits.size();++i) {
        const auto& before=debits[i].before;const auto& after_hash=hashes[i];
        const int count=before.bCount,remaining=count-debits[i].count;
        const long long id=std::bit_cast<std::int64_t>(before.dlID);
        sql<<"INSERT INTO app_world.skill_item_consumptions(world_id,char_id,server_id,owner_token,connection_id,authority_epoch,skill_id,item_id,"
             "before_count,after_count,before_hash,after_hash,core_fingerprint,state_contract,before_graph_hash,after_graph_hash,consumption_kind,cast_id,hit_count,hit_mode,accepted_cast_id) "
             "VALUES(:w,:c,:s,:t,:g,:e,:skill,:id,:before,:after,:bh,NULLIF(:ah,''),:f,:contract,NULLIF(:gb,''),NULLIF(:ga,''),:kind,:cast,:hits,:mode,NULLIF(:accepted,0))",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(server,"s"),soci::use(m_config.owner_token,"t"),
            soci::use(generation,"g"),soci::use(epoch,"e"),soci::use(unsigned_skill,"skill"),soci::use(id,"id"),
            soci::use(count,"before"),soci::use(remaining,"after"),soci::use(before.durable_hash,"bh"),soci::use(after_hash,"ah"),soci::use(fingerprint,"f"),
            soci::use(contract,"contract"),soci::use(graph_plan.before_hash,"gb"),soci::use(graph_plan.after_hash,"ga"),soci::use(consumption_kind,"kind"),
            soci::use(cast_id,"cast"),soci::use(hit_count,"hits"),soci::use(hit_mode,"mode"),soci::use(accepted_cast,"accepted");
    }
    if(cast_result)cast_result->snapshot=std::make_shared<const CharSnapshot>(after);
    tx->commit();return hashes;
}
SecureCodeState PostgreSQLMapService::ReadSecurity(soci::session& sql,const MapSessionClaim& c) {
    try {
        const long long user=c.user_id,key=c.key;SecureCodeState s;s.hydrated=true;
        std::string hex;int disabled=0,tries=0,unlocked=0;long long deadline=0,tick=0,revision=0;
        sql<<"SELECT encode(code,'hex'),disabled::integer,tries,deadline_ms,source_tick,revision,CASE WHEN unlocked_key=:k THEN 1 ELSE 0 END "
             "FROM app_global.secure_codes WHERE user_id=:u",
            soci::use(key,"k"),soci::use(user,"u"),soci::into(hex),soci::into(disabled),soci::into(tries),
            soci::into(deadline),soci::into(tick),soci::into(revision),soci::into(unlocked);
        if(!sql.got_data())return s;
        s.created=true;s.disabled=disabled;s.tries=tries;s.deadline_ms=deadline;s.source_tick=tick;s.revision=revision;s.unlocked=unlocked;
        for(std::size_t i=0;i<hex.size();i+=2)s.code.push_back(static_cast<char>(std::stoul(hex.substr(i,2),nullptr,16)));
        return s;
    }catch(...){throw std::runtime_error("Native secure state could not be read");}
}
SecureCommit PostgreSQLMapService::UpdateSecureCode(const MapSessionClaim& c,std::uint16_t opcode,
    std::span<const std::byte> body,const CharSnapshot& before) {
    const auto request=DecodeSecureRequest(opcode,body);
    if(c.role!=MapSessionRole::Primary||!before.payload||before.dwCharID!=c.char_id||before.persistence_uncertain)
        throw std::runtime_error("Secure code requires authoritative primary");
    try {
        auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
        std::string phase;
        if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||phase!="ready")throw std::runtime_error("Secure claim not ready");
        const auto receipt=ReadReceipt(sql,c,m_config.server,m_config.owner_token);
        if(!receipt.found||!receipt.core_matches||receipt.outcome!="active")throw std::runtime_error("Secure receipt changed");
        const long long user=c.user_id,key=c.key;int unlocked=0;
        sql<<"SELECT 1 FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\"=:k AND \"bLocked\"=0",soci::use(key),soci::into(unlocked);
        if(!sql.got_data())throw std::runtime_error("Secure session revoked");
        ValidateInventoryState(sql,c,before,true);
        const auto current=ReadSecurity(sql,c);long long now=0;
        sql<<"SELECT floor(extract(epoch FROM clock_timestamp())*1000)::bigint",soci::into(now);
        const auto plan=PlanSecureCode(current,request,now,static_cast<std::uint32_t>(SkillClockMs()));
        auto after=before;
        if(plan.state==current){tx->commit();return {std::make_shared<const CharSnapshot>(after),plan.packet};}
        if(!plan.packet)throw std::runtime_error("Secure mutation missing response");
        const auto hex=[](auto bytes){constexpr char d[]="0123456789abcdef";std::string out;
            for(auto byte:bytes){const auto b=static_cast<unsigned char>(byte);out+=d[b>>4];out+=d[b&15];}return out;};
        const int world=c.group,character=c.char_id,server=m_config.server,request_id=opcode,response_id=plan.packet->first;
        const long long previous=current.revision,generation=c.connection_id,epoch=c.authority_epoch;
        const auto response=hex(plan.packet->second);long long id=0;
        sql<<"INSERT INTO app_world.secure_code_operations(user_id,previous_revision,world_id,char_id,session_key,server_id,owner_token,connection_id,authority_epoch,request_opcode,response_opcode,response) "
             "VALUES(:u,:prev,:w,:c,:k,:s,:t,:g,:e,:request,:response,decode(:bytes,'hex')) RETURNING operation_id",
            soci::use(user,"u"),soci::use(previous,"prev"),soci::use(world,"w"),soci::use(character,"c"),soci::use(key,"k"),
            soci::use(server,"s"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(epoch,"e"),
            soci::use(request_id,"request"),soci::use(response_id,"response"),soci::use(response,"bytes"),soci::into(id);
        auto payload=std::make_shared<CharacterPayload>(*before.payload);payload->security=plan.state;payload->security.revision=id;
        const auto code=hex(payload->security.code);const int disabled=payload->security.disabled,tries=payload->security.tries;
        const long long deadline=payload->security.deadline_ms,tick=payload->security.source_tick,unlock_key=payload->security.unlocked?key:0;
        sql<<"INSERT INTO app_global.secure_codes(user_id,code,disabled,tries,deadline_ms,source_tick,unlocked_key,revision) "
             "VALUES(:u,decode(:code,'hex'),CAST(:d AS integer)<>0,:tries,:deadline,:tick,:key,:revision) "
             "ON CONFLICT(user_id) DO UPDATE SET code=EXCLUDED.code,disabled=EXCLUDED.disabled,tries=EXCLUDED.tries,"
             "deadline_ms=EXCLUDED.deadline_ms,source_tick=EXCLUDED.source_tick,unlocked_key=EXCLUDED.unlocked_key,revision=EXCLUDED.revision",
            soci::use(user,"u"),soci::use(code,"code"),soci::use(disabled,"d"),soci::use(tries,"tries"),soci::use(deadline,"deadline"),
            soci::use(tick,"tick"),soci::use(unlock_key,"key"),soci::use(id,"revision");
        after.payload=std::move(payload);
        WriteCore(sql,c,after,0);RecordCheckpoint(sql,c,receipt.revision,Fingerprint(c,after),"active");StoreTransferCheckpoint(sql,c,after);
        SecureCommit result{std::make_shared<const CharSnapshot>(std::move(after)),plan.packet};tx->commit();return result;
    }catch(...) {
        // SOCI diagnostics can include bound parameters. Do not let credential
        // bytes enter handler logs; the caller closes without retrying the write.
        throw std::runtime_error("Native secure transaction failed; outcome requires recovery");
    }
}

TitleChangeCommit PostgreSQLMapService::SelectTitle(const MapSessionClaim& c,std::span<const std::byte> body,
    const CharSnapshot& before) {
    const auto selected=DecodeTitleChange(body);
    if(c.role!=MapSessionRole::Primary||!before.payload||before.dwCharID!=c.char_id||before.persistence_uncertain)
        throw std::runtime_error("CHANGETITLE requires authoritative primary");
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;
    if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||phase!="ready")throw std::runtime_error("Title claim is not ready primary");
    CheckCatalogs(sql);
    const auto receipt=ReadReceipt(sql,c,m_config.server,m_config.owner_token);
    const bool graph=bool(before.payload->transfer_state);
    if(!receipt.found||!receipt.core_matches||receipt.outcome!="active"||(graph?receipt.contract!=2:(receipt.contract!=3&&receipt.contract!=4)))
        throw std::runtime_error("Title recovery receipt changed");
    const long long key=c.key;int unlocked=0;
    sql<<"SELECT 1 FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\"=:k AND \"bLocked\"=0",soci::use(key),soci::into(unlocked);
    if(!sql.got_data())throw std::runtime_error("Title session was revoked");
    ValidateInventoryState(sql,c,before,true);
    const int world=c.group,character=c.char_id;
    const auto old_titles=TitleJson(before.payload->titles);
    if(!graph) {
        std::vector<CharacterTitle> stored;
        soci::rowset<soci::row> rows=(sql.prepare<<"SELECT \"wTitleID\",\"bSelected\" FROM app_world.\"TTITLETABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c FOR UPDATE",soci::use(world,"w"),soci::use(character,"c"));
        for(const auto& r:rows)stored.push_back({static_cast<std::uint16_t>(r.get<int>(0)),r.get<int>(1)!=0});
        if(TitleJson(stored)!=old_titles)throw std::runtime_error("Title ownership changed outside native transaction");
    }
    if(selected==before.payload->selected_title||std::none_of(before.payload->titles.begin(),before.payload->titles.end(),[&](auto t){return t.id==selected;})) {
        tx->commit();return {std::make_shared<const CharSnapshot>(before),false};
    }
    auto after=before;auto payload=std::make_shared<CharacterPayload>(*before.payload);after.payload=payload;
    payload->selected_title=selected;
    for(auto& title:payload->titles)title.selected=title.id==selected;
    const int title=selected,previous_title=before.payload->selected_title,server=m_config.server;
    if(!graph) {
        const int signed_title=std::bit_cast<std::int16_t>(selected);
        sql<<"UPDATE app_world.\"TTITLETABLE\" SET \"bSelected\"=CASE WHEN \"wTitleID\"=:title THEN 1 ELSE 0 END WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c",
            soci::use(signed_title,"title"),soci::use(world,"w"),soci::use(character,"c");
    }
    const auto new_titles=TitleJson(payload->titles);
    const long long previous=payload->last_title_selection_id,generation=c.connection_id,epoch=c.authority_epoch;long long id=0;
    constexpr char digits[]="0123456789abcdef";std::string request;
    for(auto v:body){auto b=std::to_integer<unsigned>(v);request+=digits[b>>4];request+=digits[b&15];}
    sql<<"INSERT INTO app_world.title_selection_operations(world_id,char_id,previous_selection_id,session_key,server_id,owner_token,connection_id,authority_epoch,previous_title,selected_title,before_titles,after_titles,request) "
         "VALUES(:w,:c,:prev,:key,:s,:token,:g,:epoch,:old,:title,CAST(:before AS jsonb),CAST(:after AS jsonb),decode(:request,'hex')) RETURNING selection_id",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(previous,"prev"),soci::use(key,"key"),soci::use(server,"s"),
        soci::use(m_config.owner_token,"token"),soci::use(generation,"g"),soci::use(epoch,"epoch"),soci::use(previous_title,"old"),
        soci::use(title,"title"),soci::use(old_titles,"before"),soci::use(new_titles,"after"),soci::use(request,"request"),soci::into(id);
    payload->last_title_selection_id=static_cast<std::uint64_t>(id);
    WriteCore(sql,c,after,0);RecordCheckpoint(sql,c,receipt.revision,Fingerprint(c,after),"active");StoreTransferCheckpoint(sql,c,after);
    TitleChangeCommit committed{std::make_shared<const CharSnapshot>(std::move(after)),true};tx->commit();return committed;
}

QuestCommit PostgreSQLMapService::ChangeQuest(const MapSessionClaim& c,std::uint16_t opcode,
    std::span<const std::byte> body,const CharSnapshot& before,const IQuestChart& chart,const QuestMapFacts& facts) {
    using tnetlib::protocol::MessageId;
    const auto message=static_cast<MessageId>(opcode);
    const bool progress_tick=opcode==0; // internal timer only; no dispatcher registration
    const bool during_entry=opcode==1; // internal EnterMAP only; no wire change
    const bool mail_poll=opcode==2; // internal idle-primary mailbox timer only
    const bool use_item=message==MessageId::CS_ITEMUSE_REQ;
    const bool sell_item=message==MessageId::CS_ITEMSELL_REQ;
    const bool buy_item=message==MessageId::CS_ITEMBUY_REQ||message==MessageId::CS_MAGICITEMBUY_REQ;
    const bool repair_items=message==MessageId::CS_DURATIONREP_REQ;
    const bool refine_item=message==MessageId::CS_REFINE_REQ;
    const bool reclaim_refine=message==MessageId::CS_WASTEREFINE_REQ;
    const bool gamble=message==MessageId::CS_GAMBLECHECK_REQ||message==MessageId::CS_GAMBLEOPEN_REQ;
    const bool upgrade_item=message==MessageId::CS_ITEMUPGRADE_REQ;
    const bool protected_change=message==MessageId::CS_PROTECTEDADD_REQ||message==MessageId::CS_PROTECTEDERASE_REQ||message==MessageId::CS_PROTECTEDOPTION_REQ;
    const bool mail_send=message==MessageId::CS_POSTSEND_REQ;
    const bool mail_get=message==MessageId::CS_POSTGETITEM_REQ;
    const bool mail=mail_get||message==MessageId::CS_POSTLIST_REQ||message==MessageId::CS_POSTVIEW_REQ||message==MessageId::CS_POSTDEL_REQ||message==MessageId::CS_POSTRETURN_REQ;
    const bool lottery=message==MessageId::CS_SOULLOTTERY_REQ||message==MessageId::CS_SOULLOTTERY_TRIGGER_REQ;
    wire::Reader reader(body.data(),body.size());std::uint32_t quest=0,reward_id=0;std::uint16_t npc=0;std::uint8_t reward_type=0;
    if(progress_tick||during_entry||mail_poll) {
        if(!body.empty()||!facts.sample_ms)throw std::runtime_error("Invalid internal progression tick");
    } else if(mail_send||protected_change) {
        if(message==MessageId::CS_PROTECTEDOPTION_REQ) {
            if(body.size()!=5)throw std::runtime_error("Invalid PROTECTEDOPTION request length");
        }else if(body.size()<(mail_send?27U:4U)||body.size()>65527)throw std::runtime_error("Invalid variable gameplay request length");
    } else if(mail) {
        if(body.size()!=(message==MessageId::CS_POSTLIST_REQ?2U:message==MessageId::CS_POSTRETURN_REQ?5U:4U))throw std::runtime_error("Invalid native post request length");
    } else if(buy_item) {
        if(body.size()!=(message==MessageId::CS_ITEMBUY_REQ?11U:9U))throw std::runtime_error("Invalid NPC purchase request length");
    } else if(upgrade_item) {
        if(body.size()!=10)throw std::runtime_error("Invalid ITEMUPGRADE request length");
    } else if(gamble) {
        if(body.size()!=(message==MessageId::CS_GAMBLECHECK_REQ?4U:6U))throw std::runtime_error("Invalid GAMBLE request length");
    } else if(reclaim_refine) {
        if(body.size()!=2)throw std::runtime_error("Invalid WASTEREFINE request length");
    } else if(refine_item) {
        if(body.size()<8||body.size()!=8U+2U*std::to_integer<unsigned>(body[3]))throw std::runtime_error("Invalid REFINE request length");
    } else if(repair_items) {
        if(body.size()!=8)throw std::runtime_error("Invalid DURATIONREP request length");
    } else if(sell_item) {
        if(body.size()!=5)throw std::runtime_error("Invalid ITEMSELL request length");
    } else if(lottery) {
        if(!body.empty())throw std::runtime_error("Invalid soul lottery request length");
    } else if(use_item) {
        if(body.size()<7||body.size()!=7U+5U*std::to_integer<unsigned>(body[6]))
            throw std::runtime_error("Invalid ITEMUSE request length");
    } else if(message==MessageId::CS_NPCTALK_REQ||message==MessageId::CS_NPCITEMLIST_REQ) {
        if(!reader.Read(npc)||!reader.Eof())throw std::runtime_error("Invalid NPCTALK request");
    } else if(message==MessageId::CS_QUESTDROP_REQ||message==MessageId::CS_QUESTENDTIMER_REQ) {
        if(!reader.Read(quest)||!reader.Eof())throw std::runtime_error("Invalid quest progress request");
    } else if(message==MessageId::CS_QUESTEXEC_REQ) {
        if(!reader.Read(quest)||!reader.Read(reward_type)||!reader.Read(reward_id)||!reader.Eof())
            throw std::runtime_error("Invalid QUESTEXEC request");
    } else throw std::runtime_error("Quest operation contract unavailable");
    if(c.role!=MapSessionRole::Primary||before.dwCharID!=c.char_id||!before.payload||before.persistence_uncertain)
        throw std::runtime_error("Quest mutation requires authoritative primary");
    if(facts.map!=before.wMapID||facts.channel!=c.channel)throw std::runtime_error("Quest event Map context changed");
    auto lease=m_pool.Acquire();auto& sql=*lease;
    auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;
    if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||phase!="ready")throw std::runtime_error("Quest claim is not ready");
    CheckCatalogs(sql);
    const auto receipt=ReadReceipt(sql,c,m_config.server,m_config.owner_token);
    if(!receipt.found||!receipt.core_matches||receipt.outcome!="active")throw std::runtime_error("Quest checkpoint changed");
    const long long key=c.key;int current=0;
    sql << "SELECT 1 FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\"=:k AND \"bLocked\"=0",soci::use(key),soci::into(current);
    if(!sql.got_data())throw std::runtime_error("Quest session revoked");
    ValidateInventoryState(sql,c,before,true);
    QuestCommit committed;
    auto after=before;auto payload=std::make_shared<CharacterPayload>(*before.payload);after.payload=payload;
    payload->event_ms=facts.sample_ms;payload->event_time=std::time(nullptr);
    quest_runtime::HydrateLevels(*payload,chart);
    bool other_state_changed=false;
    const auto before_json=quest_runtime::Json(quest_runtime::Rows(*before.payload));
    if(mail_poll) {
        other_state_changed=PollNativeMail(sql,c,after,committed.packets);
        payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
    }else if(protected_change) {
        other_state_changed=ChangeNativeProtected(sql,c,after,opcode,body,committed.packets);
        payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
    }else if(mail_send) {
        other_state_changed=SendNativePost(sql,c,after,body,committed.packets);
        payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
    }else if(message==MessageId::CS_NPCITEMLIST_REQ) {
        ListNativeNpcItems(sql,after,npc,committed.packets);
    }else if(mail&&!mail_get) {
        other_state_changed=NativeMail(sql,c,after,opcode,body,{},committed.packets);
        payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
    }else if(upgrade_item) {
        other_state_changed=UpgradeNativeItem(sql,c,after,body,facts,committed.packets);
        payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
    }else if(reclaim_refine) {
        other_state_changed=ReclaimNativeRefine(sql,c,after,body,committed.packets);
        payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
    }else if(refine_item) {
        other_state_changed=RefineNativeItem(sql,c,after,body,facts,committed.packets);
        payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
    }else if(repair_items) {
        other_state_changed=RepairNativeItems(sql,c,after,body,committed.packets);
        payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
    }else if(sell_item) {
        std::uint16_t sold=0;
        other_state_changed=SellNativeItem(sql,c,after,body,sold,committed.packets);
        payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
        // Source calls CTPlayer::CheckQuest(count0), not module CheckQuest:
        // update running GETITEM terms without triggering new grant actions.
        if(other_state_changed)quest_runtime::UpdateWithoutTarget(after,*payload,chart,facts,sold,QTT_GETITEM,0,committed.packets);
    }else if(lottery) {
        other_state_changed=ChangeSoulLottery(sql,c,after,message==MessageId::CS_SOULLOTTERY_TRIGGER_REQ,committed.packets);
        payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
    }else if(message==MessageId::CS_NPCTALK_REQ) {
        const int id=std::bit_cast<std::int16_t>(npc);int country=0,local=0;
        sql << "SELECT \"bCountryID\",\"wLocalID\" FROM character_compat.\"TNPCCHART\" WHERE \"wID\"=:id",
            soci::use(id),soci::into(country),soci::into(local);
        if(!sql.got_data()){committed.snapshot=std::make_shared<const CharSnapshot>(before);tx->commit();return committed;}
        if(country==3&&local)throw std::runtime_error("NPC territory authority is not integrated");
        int disguise=0;
        if(!payload->effects)throw std::runtime_error("NPC interaction requires hydrated effects");
        for(const auto& effect:*payload->effects) {
            const int skill=std::bit_cast<std::int16_t>(effect.skill);
            sql << "SELECT \"bExec\" FROM character_compat.\"TSKILLDATA\" WHERE \"wSkillID\"=:s AND \"bType\"=3 "
                "AND \"bExec\" BETWEEN 4 AND 6 ORDER BY \"bAction\",\"bType\",\"bAttr\",\"bExec\" LIMIT 1",soci::use(skill),soci::into(disguise);
            if(sql.got_data())break;
            disguise=0;
        }
        const bool friendly=country==before.bCountry||country==payload->aid_country;
        if(country!=3&&(!friendly||(disguise&&disguise!=country+4))) {
            committed.snapshot=std::make_shared<const CharSnapshot>(before);tx->commit();return committed;
        }
        const auto selected=quest_runtime::Talk(*payload,chart,npc,committed.packets);
        std::vector<std::byte> ack;wire::WritePOD(ack,selected);wire::WritePOD(ack,npc);
        committed.packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_NPCTALK_ACK),std::move(ack));
    } else if(message==MessageId::CS_QUESTDROP_REQ)quest_runtime::Drop(*payload,chart,quest,committed.packets);
    else if(message==MessageId::CS_QUESTENDTIMER_REQ)quest_runtime::EndTimer(*payload,chart,quest,committed.packets);
    else if(during_entry) {
        if(!payload->post_counts)throw std::runtime_error("Native entry mailbox counters unavailable");
        std::vector<std::byte> post_info;wire::WritePOD(post_info,payload->post_counts->first);wire::WritePOD(post_info,payload->post_counts->second);
        committed.packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_POSTINFO_ACK),std::move(post_info));
        ListNativeProtected(*payload,committed.packets);
        EnterDuringItems(sql,c,after,committed.packets);
        payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
        other_state_changed=true;
    }
    else {
        const auto* requested=chart.Find(quest);
        if(requested&&requested->bTriggerType==TT_TALKNPC) {
            const int id=std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(requested->dwTriggerID));int map=0;
            sql << "SELECT \"wMapID\" FROM character_compat.\"TNPCCHART\" WHERE \"wID\"=:id",soci::use(id),soci::into(map);
            if(!sql.got_data()||static_cast<std::uint16_t>(map)!=after.wMapID)requested=nullptr;
        }
        quest_runtime::ConditionInputs inputs;
        inputs.probability=[] {
            // Retain Windows rand()%100's 15-bit distribution, with independent
            // secure draws instead of the source process-global PRNG state.
            unsigned char bytes[2];if(RAND_bytes(bytes,2)!=1)throw std::runtime_error("Quest probability unavailable");
            return static_cast<std::uint8_t>((((unsigned(bytes[0])<<8)|bytes[1])&32767U)%100U);
        };
        const auto ordered=chart.All();
        std::function<void(const QuestDef&,unsigned)> execute;
        const auto children=[&](const QuestDef& def,unsigned depth) {
            for(const auto* child:ordered)if(child->bTriggerType==TT_EXECQUEST&&child->dwTriggerID==def.dwQuestID) {
                std::uint8_t level=0;
                if(!quest_runtime::CanRun(after,*payload,*child,chart,inputs,level))execute(*child,depth+1);
            }
        };
        const auto refresh=[&]{
            RefreshEquipment(sql,after,true);
            payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
        };
        const auto give=[&](const QuestDef& def,const std::vector<ItemInstance>& items,unsigned depth) {
            if(items.empty())return false;
            if(!GrantQuestItems(sql,c,after,items,committed.packets)) {
                quest_runtime::Drop(*payload,chart,def.dwParentID,committed.packets,QR_INVENTORYFULL);
                return false;
            }
            payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
            other_state_changed=true;
            for(const auto& item:items) {
                quest_runtime::UpdateWithoutTarget(after,*payload,chart,facts,item.wItemID,QTT_GETITEM,item.bCount,committed.packets);
                // TMapSvr::CheckQuest advances existing terms before dispatching
                // trigger actions, each of which performs its own source gate.
                for(const auto* trigger:ordered)if(trigger->bTriggerType==TT_GETITEM&&trigger->dwTriggerID==item.wItemID) {
                    std::uint8_t level=0;
                    if(!quest_runtime::CanRun(after,*payload,*trigger,chart,inputs,level))execute(*trigger,depth+1);
                }
            }
            committed.packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_MONITEMTAKE_ACK),EncodeMonItemTakeAck(0));
            return true;
        };
        execute=[&](const QuestDef& def,unsigned depth) {
            if(depth>256)throw std::runtime_error("Quest trigger chain exceeds safe recursion depth");
            std::uint8_t level=0;
            switch(def.bType) {
            case QT_DEFTALK:return; // CQuestDefaultTalk::ExecQuest is intentionally empty.
            case QT_NPCTALK: {
                const auto selected=quest_runtime::CanRun(after,*payload,def,chart,inputs,level)?0U:def.dwQuestID;
                std::vector<std::byte> ack;wire::WritePOD(ack,selected);wire::WritePOD(ack,std::uint16_t{0});
                // CS_QUESTEXEC_REQ creates CQuest with m_dwMonID=0; the trigger
                // NPC is not substituted for this original response field.
                committed.packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_NPCTALK_ACK),std::move(ack));return;
            }
            case QT_DROPQUEST:
                for(const auto& t:def.terms)quest_runtime::Drop(*payload,chart,t.dwTermID,committed.packets);
                return;
            case QT_CHAPTERMSG: {
                std::vector<std::byte> ack;wire::WritePOD(ack,def.dwQuestID);
                committed.packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_CHAPTERMSG_ACK),std::move(ack));return;
            }
            case QT_ROUTING: {
                if(quest_runtime::CanRun(after,*payload,def,chart,inputs,level))return;
                std::vector<std::uint16_t> items;
                for(const auto& term:def.terms)if(term.bTermType==QTT_ITEMID)
                    items.push_back(static_cast<std::uint16_t>(term.dwTermID));
                if(items.size()>255)throw std::runtime_error("Quest NPC item list exceeds source count width");
                const auto npc_id=static_cast<std::uint16_t>(def.dwTriggerID);
                const int id=std::bit_cast<std::int16_t>(npc_id);
                int condition=0,rate=0,local=0;
                sql << "SELECT \"bCondition\",\"bDiscountRate\",\"wLocalID\" FROM character_compat.\"TNPCCHART\" WHERE \"wID\"=:id",
                    soci::use(id),soci::into(condition),soci::into(rate),soci::into(local);
                // CSSender's overload accepts a missing NPC: GetDiscountRate
                // returns zero, while the quest-specified NPC ID stays on wire.
                const auto discount=sql.got_data()?NpcDiscount(condition,rate,local):std::uint8_t{0};
                std::vector<std::byte> ack;wire::WritePOD(ack,npc_id);
                wire::WritePOD(ack,std::uint8_t{7}); // NetCode.h TNPC_BOX
                wire::WritePOD(ack,discount);wire::WritePOD(ack,static_cast<std::uint8_t>(items.size()));
                for(const auto item:items)wire::WritePOD(ack,item);
                committed.packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_NPCITEMLIST_ACK),std::move(ack));
                return; // CQuestRouting does not execute child quests.
            }
            case QT_GIVESKILL: {
                if(quest_runtime::CanRun(after,*payload,def,chart,inputs,level))return;
                std::uint16_t skill=0;std::uint8_t rank=0;
                for(const auto& t:def.terms)if(t.bTermType==QTT_SKILLID){skill=static_cast<std::uint16_t>(t.dwTermID);rank=t.bGoalCount;}
                const int id=std::bit_cast<std::int16_t>(skill);int classes=0;
                sql << "SELECT \"dwClassID\" FROM character_compat.\"TSKILLCHART\" WHERE \"wID\"=:id",soci::use(id),soci::into(classes);
                if(!sql.got_data())return;
                if(after.bClass>=32)throw std::runtime_error("Quest skill class shift out of range");
                if(!(static_cast<std::uint32_t>(classes)&(std::uint32_t{1}<<after.bClass)))return;
                if(std::any_of(payload->skills.begin(),payload->skills.end(),[&](const auto& s){return s.wSkillID==skill;}))return;
                rank=std::max<std::uint8_t>(rank,1);payload->skills.push_back({skill,rank,0});
                std::sort(payload->skills.begin(),payload->skills.end(),[](const auto& a,const auto& b){return a.wSkillID<b.wSkillID;});
                refresh();other_state_changed=true;
                if(!payload->transfer_state) {
                    const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id),learned=rank;
                    sql << "INSERT INTO app_world.\"TSKILLTABLE\"(\"bWorldID\",\"dwCharID\",\"wSkillID\",\"bLevel\",\"dwRemainTick\") VALUES(:w,:c,:s,:r,0)",
                        soci::use(world,"w"),soci::use(character,"c"),soci::use(id,"s"),soci::use(learned,"r");
                }
                LearningPlan learned;learned.after=after;learned.result=0;learned.rank=rank;
                committed.packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_SKILLBUY_ACK),EncodeSkillPurchaseAck({0,skill},learned));
                children(def,depth);return;
            }
            case QT_GIVEITEM: {
                if(quest_runtime::CanRun(after,*payload,def,chart,inputs,level))return;
                std::uint32_t completed=0;std::vector<ItemInstance> items;
                for(const auto& t:def.terms) {
                    if(t.bTermType==QTT_ITEMID) {
                        if(auto item=CreateQuestItem(sql,c,static_cast<std::uint16_t>(t.dwTermID),t.bGoalCount))items.push_back(std::move(*item));
                    } else if(t.bTermType==QTT_COMPQUEST) {
                        completed=t.dwTermID;
                        if(quest_runtime::Find(quest_runtime::Rows(*payload),completed))
                            if(const auto* current=chart.Find(completed))execute(*current,depth+1);
                    }
                }
                const bool granted=give(def,items,depth);
                if(completed||granted)children(def,depth);
                return;
            }
            case QT_COMPLETE:case QT_CRAFT: {
                if(quest_runtime::CanRun(after,*payload,def,chart,inputs,level))return;
                const bool craft=def.bType==QT_CRAFT;
                std::uint32_t target=craft?def.dwQuestID:0;
                if(!craft)for(const auto& term:def.terms)if(term.bTermType==QTT_COMPQUEST){target=term.dwTermID;break;}
                // A recipe checks a newly created CQuestCraft's private terms;
                // it neither requires a running mission nor increments its
                // persistent completion history. Repeated valid crafts remain valid.
                QuestProgressRow recipe{target,0,0,0,{}};
                auto* progress=craft?&recipe:quest_runtime::Find(quest_runtime::Rows(*payload),target);
                const auto* completed=craft?&def:chart.Find(target);
                if(!progress||!completed||(!craft&&!quest_runtime::Running(*progress)))return;
                for(const auto& term:completed->terms)if(quest_runtime::TermStatus(after,*progress,term)!=QTS_SUCCESS) {
                    if(!def.force_run)committed.packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_QUESTCOMPLETE_ACK),
                        EncodeQuestCompleteAck(QR_TERM,target,term.dwTermID,term.bTermType,0));
                    return;
                }
                int available=0;
                sql << "SELECT count(*) FROM game_compat.quest_reward_release r JOIN game_compat.catalog_release m "
                    "ON m.manifest_sha256=r.map_manifest AND m.status='verified' WHERE r.status='verified' "
                    "AND r.character_manifest=:c AND r.actor_manifest=:a",
                    soci::use(m_config.character_manifest,"c"),soci::use(m_config.actor_manifest,"a"),soci::into(available);
                if(available!=1)throw std::runtime_error("Quest rewards require compatible published historical content");
                const QuestEvent event=[&](std::uint32_t id,std::uint8_t type,std::uint8_t trigger_type,int count) {
                    payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
                    quest_runtime::UpdateWithoutTarget(after,*payload,chart,facts,id,type,count,committed.packets);
                    for(const auto* trigger:ordered)if(trigger->bTriggerType==trigger_type&&trigger->dwTriggerID==id) {
                        std::uint8_t gate_level=0;
                        if(!quest_runtime::CanRun(after,*payload,*trigger,chart,inputs,gate_level))execute(*trigger,depth+1);
                    }
                };
                // CreateQuest initializes child/CheckQuest reward selection to
                // zero. Only the actual network-requested action carries it.
                const auto result=CompleteQuestRewards(sql,c,after,*completed,depth?0:reward_type,depth?0:reward_id,
                    inputs.probability,event,committed.packets);
                payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
                committed.packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_QUESTCOMPLETE_ACK),
                    EncodeQuestCompleteAck(result,target,0,0,0));
                if(result!=QR_SUCCESS)return;
                other_state_changed=true;
                if(!craft) {
                    progress=quest_runtime::Find(quest_runtime::Rows(*payload),target);
                    if(!progress)throw std::runtime_error("Completed quest disappeared during reward triggers");
                    ++progress->bCompleteCount;progress->dwTick=0;progress->timer_active=false;progress->timer_expired=false;
                    if(completed->bType!=QT_GUILD)progress->save=1;
                    for(auto& term:progress->terms)term.bCount=0;
                }
                event(target,QTT_QUESTCOMPLETED,TT_COMPLETE,1);
                children(def,depth);return;
            }
            case QT_MISSION:case QT_GUILD: {
                if(quest_runtime::CanRun(after,*payload,def,chart,inputs,level))return;
                auto* q=quest_runtime::Find(quest_runtime::Rows(*payload),def.dwQuestID);
                if(!q) {quest_runtime::Rows(*payload).push_back({def.dwQuestID,0,0,0,{}});q=&quest_runtime::Rows(*payload).back();}
                if(const auto key=quest_runtime::LevelKey(def))payload->quest_levels->insert(key);
                std::vector<std::byte> ack;wire::WritePOD(ack,def.dwQuestID);wire::WritePOD(ack,def.bType);
                committed.packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_QUESTADD_ACK),std::move(ack));
                if(q->bTriggerCount==255){q->bTriggerCount=1;q->bCompleteCount=1;}
                ++q->bTriggerCount;if(def.bType!=QT_GUILD)q->save=1;
                for(const auto& t:def.terms)
                    quest_runtime::UpdateWithoutTarget(after,*payload,chart,facts,t.dwTermID,t.bTermType,0,committed.packets);
                // The Araz mission variant intentionally retains this flag
                // across GETITEM terms; owning an earlier mission item suppresses
                // later paired grants. Preserve that observable source rule.
                bool eligible=true;
                for(const auto& t:def.terms)if(t.bTermType==QTT_GETITEM)
                    for(const auto& paired:def.terms)if(paired.bTermType==QTT_ITEMID&&paired.dwTermID==t.dwTermID) {
                        if(quest_runtime::HasItem(*payload,t.dwTermID))eligible=false;
                        if(!eligible)continue;
                        std::vector<ItemInstance> items;
                        if(auto item=CreateQuestItem(sql,c,static_cast<std::uint16_t>(t.dwTermID),t.bGoalCount))items.push_back(std::move(*item));
                        give(def,items,depth);break;
                    }
                for(const auto& t:def.terms)if(t.bTermType==QTT_TIMER) {
                    q=quest_runtime::Find(quest_runtime::Rows(*payload),def.dwQuestID);
                    if(!q)throw std::runtime_error("Mission state disappeared during trigger execution");
                    q->dwTick=t.dwTermID;q->timer_active=true;q->timer_expired=false;
                    std::vector<std::byte> timer;wire::WritePOD(timer,def.dwQuestID);wire::WritePOD(timer,t.dwTermID);
                    committed.packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_QUESTSTARTTIMER_ACK),std::move(timer));
                }
                children(def,depth);return;
            }
            case QT_DELETEITEM: {
                if(quest_runtime::CanRun(after,*payload,def,chart,inputs,level))return;
                for(const auto& t:def.terms)if(t.bTermType==QTT_ITEMID) {
                    const auto item=static_cast<std::uint16_t>(t.dwTermID);const int id=std::bit_cast<std::int16_t>(item);int exists=0;
                    sql << "SELECT 1 FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",soci::use(id),soci::into(exists);
                    if(!sql.got_data())continue;
                    std::sort(payload->bags.begin(),payload->bags.end(),[](const auto& a,const auto& b){return a.bag.bInvenID<b.bag.bInvenID;});
                    for(auto& bag:payload->bags) {
                        std::sort(bag.items.begin(),bag.items.end(),[](const auto& a,const auto& b){return a.bItemID<b.bItemID;});
                        for(auto it=bag.items.begin();it!=bag.items.end();) {
                            if(it->wItemID!=item){++it;continue;}
                            if(!payload->transfer_state) {
                                const int world=c.group,owner=std::bit_cast<std::int32_t>(c.char_id);
                                const long long identity=std::bit_cast<long long>(it->dlID);long long erased=0;
                                sql << "DELETE FROM app_world.\"TITEMTABLE\" WHERE \"bWorldID\"=:w AND \"dwOwnerID\"=:c AND \"dlID\"=:id AND \"bOwnerType\"=0 AND \"bStorageType\"=0 AND app_world.item_fingerprint(\"TITEMTABLE\")=:hash RETURNING \"dlID\"",
                                    soci::use(world,"w"),soci::use(owner,"c"),soci::use(identity,"id"),soci::use(it->durable_hash,"hash"),soci::into(erased);
                                if(!sql.got_data()||erased!=identity)throw std::runtime_error("Quest item ownership changed");
                            }
                            std::vector<std::byte> ack;wire::WritePOD(ack,bag.bag.bInvenID);wire::WritePOD(ack,it->bItemID);
                            committed.packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_DELITEM_ACK),std::move(ack));
                            it=bag.items.erase(it);other_state_changed=true;
                        }
                    }
                }
                if(other_state_changed)refresh();
                return; // CQuestDeleteItem deliberately does not execute children.
            }
            default:
                if(def.bType>QT_DEFTALK&&def.bType<QT_COUNT)
                    throw std::runtime_error("Quest action requires remaining native integration: quest="+std::to_string(def.dwQuestID)+" type="+std::to_string(def.bType));
                children(def,depth);return; // CQuest default implementation.
            }
        };
        if(progress_tick||use_item||gamble||buy_item||mail_get) {
            const QuestEvent event=[&](std::uint32_t id,std::uint8_t type,std::uint8_t trigger_type,int count) {
                payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
                quest_runtime::UpdateWithoutTarget(after,*payload,chart,facts,id,type,count,committed.packets);
                for(const auto* trigger:ordered)if(trigger->bTriggerType==trigger_type&&trigger->dwTriggerID==id) {
                    std::uint8_t gate_level=0;
                    if(!quest_runtime::CanRun(after,*payload,*trigger,chart,inputs,gate_level))execute(*trigger,1);
                }
            };
            if(buy_item)other_state_changed=BuyNativeItem(sql,c,after,message==MessageId::CS_MAGICITEMBUY_REQ,body,chart,event,committed.packets);
            else if(gamble)other_state_changed=GambleNativeItem(sql,c,after,message==MessageId::CS_GAMBLECHECK_REQ,body,facts,event,committed.packets);
            else if(use_item)other_state_changed=UseNativeItem(sql,c,after,body,event,committed.packets);
            else if(mail_get)other_state_changed=NativeMail(sql,c,after,opcode,body,event,committed.packets);
            else {
                AdvanceProgressEffects(sql,c,after,facts.sample_ms,event,committed.packets);
                AdvanceDuringItems(sql,c,after,committed.packets);
                AdvanceItemExpirations(sql,c,after,committed.packets);
                PollNativeMail(sql,c,after,committed.packets);
                other_state_changed=true;
            }
            payload=std::make_shared<CharacterPayload>(*after.payload);after.payload=payload;
            // Timer changes share the quest ledger so an older checkpoint,
            // cast or reward cannot restore the previous effect/XP snapshot.
        } else if(requested)execute(*requested,0);
    }
    if(!facts.world_registered&&std::any_of(committed.packets.begin(),committed.packets.end(),
        [](const auto& packet){return packet.audience==QuestAudience::World;}))
        throw std::runtime_error("Quest progression requires a registered World connection");
    const auto after_json=quest_runtime::Json(quest_runtime::Rows(*payload));
    if(before_json==after_json&&!other_state_changed) {
        // Repeated capped talk and absent/nonrunning drop still have their
        // original responses. Equal request bytes are never a deduplication key.
        committed.snapshot=std::make_shared<const CharSnapshot>(std::move(after));tx->commit();return committed;
    }
    const auto hex=[](std::span<const std::byte> bytes){constexpr char d[]="0123456789abcdef";std::string out;
        for(auto v:bytes){const auto b=std::to_integer<unsigned>(v);out+=d[b>>4];out+=d[b&15];}return out;};
    std::string packets="[";
    for(const auto& packet:committed.packets){if(packets.size()>1)packets+=',';
        packets+='['+std::to_string(packet.opcode)+",\""+hex(packet.body)+"\","+std::to_string(static_cast<unsigned>(packet.audience))+']';}
    packets+=']';const auto request=hex(body);
    const auto events=facts.world_events?facts.world_events->Json():std::string{"null"};
    if(!before.payload->pvp||!payload->pvp)throw std::runtime_error("Quest operation lost PvP balance");
    const auto before_pvp=before.payload->pvp->Json(),after_pvp=payload->pvp->Json();
    const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id),server=m_config.server,op=opcode;
    const long long previous=payload->last_quest_id,generation=c.connection_id,epoch=c.authority_epoch;long long operation=0;
    sql << "INSERT INTO app_world.quest_operations(world_id,char_id,previous_operation_id,session_key,server_id,owner_token,connection_id,authority_epoch,opcode,request,before_state,after_state,responses,world_event_context,before_pvp,after_pvp) "
        "VALUES(:w,:c,:previous,:key,:server,:token,:generation,:epoch,:op,decode(:request,'hex'),CAST(:before AS jsonb),CAST(:after AS jsonb),CAST(:packets AS jsonb),NULLIF(CAST(:events AS jsonb),'null'::jsonb),CAST(:before_pvp AS jsonb),CAST(:after_pvp AS jsonb)) RETURNING operation_id",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(previous,"previous"),soci::use(key,"key"),soci::use(server,"server"),
        soci::use(m_config.owner_token,"token"),soci::use(generation,"generation"),soci::use(epoch,"epoch"),soci::use(op,"op"),
        soci::use(request,"request"),soci::use(before_json,"before"),soci::use(after_json,"after"),soci::use(packets,"packets"),soci::use(events,"events"),soci::use(before_pvp,"before_pvp"),soci::use(after_pvp,"after_pvp"),soci::into(operation);
    payload->last_quest_id=static_cast<std::uint64_t>(operation);
    WriteQuestProgress(sql,c,after);
    if(!after.payload->transfer_state)WriteMaintainedEffects(sql,c,after);
    WriteDuringItems(sql,c,after);
    // The request was checked against its old head before any mutation. New
    // delay groups are armed only here, in the same item/quest transaction.
    WriteItemCooldowns(sql,c,after);
    WriteCore(sql,c,after,0);RecordCheckpoint(sql,c,receipt.revision,Fingerprint(c,after),"active");StoreTransferCheckpoint(sql,c,after);
    committed.snapshot=std::make_shared<const CharSnapshot>(std::move(after));tx->commit();return committed;
}

SkillPurchaseCommit PostgreSQLMapService::PurchaseSkill(const MapSessionClaim& c,std::span<const std::byte> body,const CharSnapshot& before) {
    const auto req=DecodeSkillPurchase(body);
    if(c.role!=MapSessionRole::Primary||!before.payload||before.dwCharID!=c.char_id||before.persistence_uncertain)
        throw std::runtime_error("SKILLBUY requires authoritative primary");
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;
    if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||phase!="ready")throw std::runtime_error("Learning claim is not ready primary");
    CheckCatalogs(sql);
    const auto receipt=ReadReceipt(sql,c,m_config.server,m_config.owner_token);
    const bool graph=bool(before.payload->transfer_state);
    if(!receipt.found||!receipt.core_matches||receipt.outcome!="active"||(graph?receipt.contract!=2:(receipt.contract!=3&&receipt.contract!=4)))
        throw std::runtime_error("Learning recovery receipt changed");
    const long long key=c.key;int unlocked=0;
    sql<<"SELECT 1 FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\"=:k AND \"bLocked\"=0",soci::use(key),soci::into(unlocked);
    if(!sql.got_data())throw std::runtime_error("Learning session was revoked");
    std::string manifest;
    sql<<"SELECT manifest_sha256 FROM character_compat.trainer_release WHERE status='verified' AND character_manifest=:c AND actor_manifest=:a",
        soci::use(m_config.character_manifest,"c"),soci::use(m_config.actor_manifest,"a"),soci::into(manifest);
    if(!sql.got_data())throw std::runtime_error("Learning requires verified compatible trainer catalog");
    ValidateInventoryState(sql,c,before,true);
    if(!SupportedPostures(*before.payload))throw std::runtime_error("Learning disguise/effect semantics unavailable");
    if(before.payload->security.Blocks())throw std::runtime_error("Learning requires secure unlock");
    if(before.bStartAct==2||(before.bStartAct==1&&before.wMapID==2010))
        throw std::runtime_error("Tutorial does not permit learning");
    auto original=before;RefreshEquipment(sql,original,true);
    LearningCatalog catalog;
    const int npc=req.npc,skill=req.skill,world=c.group,character=c.char_id;
    {soci::rowset<soci::row> rows=(sql.prepare<<"SELECT \"bType\",\"bCountryID\",\"wLocalID\",\"bCondition\",\"bDiscountRate\" FROM character_compat.\"TNPCCHART\" WHERE (\"wID\"::integer & 65535)=:id",soci::use(npc));
     for(const auto& r:rows) {
        if(catalog.npc_exists)throw std::runtime_error("Duplicate source NPC");
        catalog.npc_exists=true;
        const auto country=r.get<int>(1),local=r.get<int>(2),condition=r.get<int>(3),discount=r.get<int>(4);
        if(country==3&&local)throw std::runtime_error("Trainer territory ownership is not integrated");
        catalog.can_talk=country==3||country==before.bCountry||country==before.payload->aid_country;
        if(!catalog.can_talk)continue;
        catalog.discount=NpcDiscount(condition,discount,local);
        if(r.get<int>(0)==1||r.get<int>(0)==19){int found=0;
            sql<<"SELECT 1 FROM character_compat.\"TNPCITEMCHART\" WHERE (\"wNpcID\"::integer & 65535)=:npc AND (\"dwItemID\" & 65535)=:skill LIMIT 1",
                soci::use(npc,"npc"),soci::use(skill,"skill"),soci::into(found);catalog.offered=sql.got_data();}
     }}
    {soci::rowset<soci::row> rows=(sql.prepare<<"SELECT \"wParentSkillID\",\"bLevel\",\"bNextLevel\",\"bMaxLevel\",\"bKind\",\"dwClassID\",\"fPrice\"::double precision FROM character_compat.\"TSKILLCHART\" WHERE (\"wID\"::integer & 65535)=:id",soci::use(skill));
     for(const auto& r:rows){if(catalog.skill)throw std::runtime_error("Duplicate source skill");
        catalog.skill=LearningSkill{req.skill,static_cast<std::uint16_t>(r.get<int>(0)),static_cast<std::uint8_t>(r.get<int>(1)),
            static_cast<std::uint8_t>(r.get<int>(2)),static_cast<std::uint8_t>(r.get<int>(3)),static_cast<std::uint8_t>(r.get<int>(4)),
            static_cast<std::uint32_t>(r.get<int>(5)),static_cast<float>(r.get<double>(6))};}}
    {soci::rowset<soci::row> rows=(sql.prepare<<"SELECT \"wID\",\"bLevel\",\"bSkillPoint\",\"bGroupPoint\",\"bPrevSkillLevel\" FROM actor_compat.\"TSKILLPOINTCHART\"");
     for(const auto& r:rows)catalog.points.emplace(std::make_pair(static_cast<std::uint16_t>(r.get<int>(0)),static_cast<std::uint8_t>(r.get<int>(1))),
         LearningPoint{static_cast<std::uint8_t>(r.get<int>(2)),static_cast<std::uint8_t>(r.get<int>(3)),static_cast<std::uint8_t>(r.get<int>(4))});}
    {soci::rowset<soci::row> rows=(sql.prepare<<"SELECT \"bLevel\",\"dwMoney\" FROM character_compat.\"TLEVELCHART\"");
     for(const auto& r:rows)catalog.money.emplace(static_cast<std::uint8_t>(r.get<int>(0)),static_cast<std::uint32_t>(r.get<int>(1)));}
    {soci::rowset<soci::row> rows=(sql.prepare<<"SELECT \"wTitleID\",\"dwRequirement\" FROM character_compat.\"TTITLECHART\" WHERE \"bKind\"=8");
     for(const auto& r:rows)catalog.gold_titles.emplace(static_cast<std::uint16_t>(r.get<int>(0)),static_cast<std::uint32_t>(r.get<int>(1)));}
    const auto title_json=[](const auto& input){auto titles=input;std::sort(titles.begin(),titles.end(),[](const auto& a,const auto& b){return a.id<b.id;});
        std::string out="[";for(const auto& t:titles){if(out.size()>1)out+=',';out+='['+std::to_string(t.id)+','+(t.selected?"1":"0")+']';}return out+']';};
    const auto before_titles=title_json(before.payload->titles);
    if(!graph) {
        std::vector<CharacterTitle> stored;
        {soci::rowset<soci::row> rows=(sql.prepare<<"SELECT \"wTitleID\",\"bSelected\" FROM app_world.\"TTITLETABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c FOR UPDATE",soci::use(world,"w"),soci::use(character,"c"));
         for(const auto& r:rows)stored.push_back({static_cast<std::uint16_t>(r.get<int>(0)),r.get<int>(1)!=0});}
        if(title_json(stored)!=before_titles)throw std::runtime_error("Title ownership changed outside learning");
    }
    auto plan=PlanSkillPurchase(req,original,catalog);auto& after=plan.after;
    // Rebuild passive/equipment permissions and the source per-kind point sums
    // from the new rank, before accepting any database mutation.
    if(!plan.result)RefreshEquipment(sql,after,true);
    const auto after_titles=title_json(after.payload->titles);
    if(!graph) {
        if(!plan.result) {
            const int signed_skill=std::bit_cast<std::int16_t>(req.skill),rank=plan.rank;
            sql<<"INSERT INTO app_world.\"TSKILLTABLE\"(\"bWorldID\",\"dwCharID\",\"wSkillID\",\"bLevel\",\"dwRemainTick\") VALUES(:w,:c,:id,:rank,0) "
                 "ON CONFLICT(\"bWorldID\",\"dwCharID\",\"wSkillID\") DO UPDATE SET \"bLevel\"=EXCLUDED.\"bLevel\"",
                soci::use(world,"w"),soci::use(character,"c"),soci::use(signed_skill,"id"),soci::use(rank,"rank");
        }
        if(before_titles!=after_titles) {
            sql<<"DELETE FROM app_world.\"TTITLETABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c",soci::use(world,"w"),soci::use(character,"c");
            for(const auto& t:after.payload->titles){const int id=std::bit_cast<std::int16_t>(t.id),selected=t.selected;
                sql<<"INSERT INTO app_world.\"TTITLETABLE\"(\"bWorldID\",\"dwCharID\",\"wTitleID\",\"bSelected\") VALUES(:w,:c,:id,:selected)",
                    soci::use(world,"w"),soci::use(character,"c"),soci::use(id,"id"),soci::use(selected,"selected");}
        }
    }
    SkillPurchaseCommit committed;using tnetlib::protocol::MessageId;
    const auto hex=[](auto bytes){constexpr char digits[]="0123456789abcdef";std::string out;for(auto v:bytes){const auto b=std::to_integer<unsigned>(v);out+=digits[b>>4];out+=digits[b&15];}return out;};
    std::string title_packets="[";
    for(auto& packet:plan.titles){if(title_packets.size()>1)title_packets+=',';title_packets+='"'+hex(packet)+'"';committed.packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_TITLEGAIN_ACK),std::move(packet));}
    title_packets+=']';
    auto ack=EncodeSkillPurchaseAck(req,plan);const auto ack_hex=hex(ack),request_hex=hex(body);
    committed.packets.emplace_back(static_cast<std::uint16_t>(MessageId::CS_SKILLBUY_ACK),std::move(ack));
    const int server=m_config.server,code=plan.result,rank=plan.rank;
    const long long generation=c.connection_id,epoch=c.authority_epoch,previous=before.payload->last_purchase_id,spent=plan.spent;long long id=0;
    sql<<"INSERT INTO app_world.skill_purchase_operations(world_id,char_id,previous_purchase_id,session_key,server_id,owner_token,connection_id,authority_epoch,trainer_manifest,npc_id,skill_id,result,learned_rank,spent,request,acknowledgement,title_packets,before_titles,after_titles) "
         "VALUES(:w,:c,:prev,:key,:s,:token,:g,:epoch,:manifest,:npc,:skill,:result,:rank,:spent,decode(:request,'hex'),decode(:ack,'hex'),CAST(:packets AS jsonb),CAST(:before AS jsonb),CAST(:after AS jsonb)) RETURNING purchase_id",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(previous,"prev"),soci::use(key,"key"),soci::use(server,"s"),soci::use(m_config.owner_token,"token"),
        soci::use(generation,"g"),soci::use(epoch,"epoch"),soci::use(manifest,"manifest"),soci::use(npc,"npc"),soci::use(skill,"skill"),soci::use(code,"result"),
        soci::use(rank,"rank"),soci::use(spent,"spent"),soci::use(request_hex,"request"),soci::use(ack_hex,"ack"),soci::use(title_packets,"packets"),soci::use(before_titles,"before"),soci::use(after_titles,"after"),soci::into(id);
    if (!plan.result) {
        // Default legacy Map builds send DM_LOGSKILL_REQ only after learning
        // succeeds. TLogSkill records the remaining purse, not the price.
        // Keep the source signed SQL representation of DWORD/WORD arguments.
        const int gold=std::bit_cast<std::int32_t>(after.dwGold);
        const int silver=std::bit_cast<std::int32_t>(after.dwSilver);
        const int copper=std::bit_cast<std::int32_t>(after.dwCooper);
        const int source_skill=std::bit_cast<std::int16_t>(req.skill);
        sql << "INSERT INTO app_world.\"TSKILLLOG\"(world_id,purchase_id,\"dwCharID\",\"dwGold\",\"dwSilver\",\"dwCooper\",\"wSkill\",\"bLevel\") "
               "VALUES(:w,:purchase,:c,:gold,:silver,:copper,:skill,:rank)",
            soci::use(world,"w"),soci::use(id,"purchase"),soci::use(character,"c"),
            soci::use(gold,"gold"),soci::use(silver,"silver"),soci::use(copper,"copper"),
            soci::use(source_skill,"skill"),soci::use(rank,"rank");
    }
    auto payload=std::make_shared<CharacterPayload>(*after.payload);payload->last_purchase_id=static_cast<std::uint64_t>(id);after.payload=std::move(payload);
    WriteCore(sql,c,after,0);RecordCheckpoint(sql,c,receipt.revision,Fingerprint(c,after),"active");StoreTransferCheckpoint(sql,c,after);
    committed.snapshot=std::make_shared<const CharSnapshot>(std::move(after));tx->commit();return committed;
}

SkillActionCommit PostgreSQLMapService::CommitSkillAction(const MapSessionClaim& c,std::span<const std::byte> body,
    const CharSnapshot& before) {
    const auto request=DecodeSkillAction(body);
    if(c.role!=MapSessionRole::Primary||!before.payload||before.dwCharID!=c.char_id||before.persistence_uncertain||
       request.object!=c.char_id||request.type!=1||request.channel!=c.channel||request.map!=before.wMapID)
        throw std::runtime_error("ACTION requires an authoritative own PC and route");
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;
    if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||phase!="ready")throw std::runtime_error("Action claim is not ready primary");
    CheckCatalogs(sql);
    const auto receipt=ReadReceipt(sql,c,m_config.server,m_config.owner_token);
    const bool graph=bool(before.payload->transfer_state);
    if(!receipt.found||!receipt.core_matches||receipt.outcome!="active"||(graph?receipt.contract!=2:(receipt.contract!=3&&receipt.contract!=4)))
        throw std::runtime_error("Action recovery receipt changed");
    const long long key=c.key;int unlocked=0;
    sql<<"SELECT 1 FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\"=:k AND \"bLocked\"=0",soci::use(key),soci::into(unlocked);
    if(!sql.got_data())throw std::runtime_error("Action session was revoked");
    ValidateInventoryState(sql,c,before,true);
    auto after=before;RefreshEquipment(sql,after,true);
    if(after.dwHP!=before.dwHP||after.dwMP!=before.dwMP)throw std::runtime_error("Action resources exceed source maximum");
    std::uint8_t result=SKILL_SUCCESS;std::uint32_t charge=0;
    if(request.skill) {
        const auto& p=*after.payload;
        const auto learned=std::find_if(p.skills.begin(),p.skills.end(),[&](const auto& s){return s.wSkillID==request.skill&&s.bLevel;});
        const auto definition=std::find_if(p.skill_templates.begin(),p.skill_templates.end(),[&](const auto& t){return t.wID==request.skill;});
        if(learned==p.skills.end()||definition==p.skill_templates.end())result=SKILL_NOTFOUND;
        else if(learned->dwRemainTick)result=SKILL_SPEEDYUSE;
        else if(after.dwMP<skill_engine::RequiredMP(*definition,p.pure_max_mp,learned->bLevel))result=SKILL_NEEDMP;
        // ACTION uses <, unlike ordinary SKILLUSE's <= HP gate.
        else if(after.dwHP<skill_engine::RequiredHP(*definition,p.pure_max_hp,learned->bLevel))result=SKILL_NEEDHP;
        else if(definition->wPrevActiveID)result=SKILL_NEEDPREVACT;
        else {
            // ReadPostures pins zero bEraseAct: EraseBuffByAttack retains these
            // stances. ACTION does not invoke the zero-MP CheckEternalBuff.
            if(!definition->action_supported||!SupportedPostures(p))
                throw std::runtime_error("ACTION effect/recall side effects are not implemented");
            charge=definition->action_time;
        }
    }
    SkillActionCommit committed;committed.acknowledgement=EncodeActionAck(result,c.char_id,1,request.action,request.act,request.animation,request.skill);
    const auto hex=[](auto bytes){constexpr char digits[]="0123456789abcdef";std::string out;
        for(auto v:bytes){const auto b=std::to_integer<unsigned>(v);out+=digits[b>>4];out+=digits[b&15];}return out;};
    const auto request_hex=hex(body),ack_hex=hex(committed.acknowledgement),fingerprint=Fingerprint(c,after);
    const int world=c.group,character=c.char_id,server=m_config.server,channel=c.channel,map=before.wMapID,skill=request.skill,code=result;
    const long long generation=c.connection_id,epoch=c.authority_epoch,previous=before.payload->last_action_id,duration=charge;long long action=0;
    sql<<"INSERT INTO app_world.skill_action_operations(world_id,char_id,previous_action_id,session_key,server_id,owner_token,connection_id,authority_epoch,channel_id,map_id,skill_id,result,charge_ms,request,acknowledgement) "
         "VALUES(:w,:c,:previous,:k,:s,:t,:g,:e,:channel,:map,:skill,:result,:duration,decode(:request,'hex'),decode(:ack,'hex')) RETURNING action_id",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(previous,"previous"),soci::use(key,"k"),soci::use(server,"s"),
        soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(epoch,"e"),soci::use(channel,"channel"),soci::use(map,"map"),
        soci::use(skill,"skill"),soci::use(code,"result"),soci::use(duration,"duration"),soci::use(request_hex,"request"),soci::use(ack_hex,"ack"),soci::into(action);
    auto payload=std::make_shared<CharacterPayload>(*after.payload);payload->last_action_id=static_cast<std::uint64_t>(action);after.payload=std::move(payload);
    WriteCore(sql,c,after,0);RecordCheckpoint(sql,c,receipt.revision,fingerprint,"active");StoreTransferCheckpoint(sql,c,after);
    committed.snapshot=std::make_shared<const CharSnapshot>(std::move(after));tx->commit();return committed;
}
FinishSkillCommit PostgreSQLMapService::FinishSkill(const MapSessionClaim& c,std::span<const std::byte> body,
    const CharSnapshot& before) {
    const auto request=DecodeFinishSkill(body);
    if(c.role!=MapSessionRole::Primary||!before.payload||before.dwCharID!=c.char_id||before.persistence_uncertain||
       request.host!=c.char_id||request.attacker!=c.char_id||request.type!=1||request.linked||request.fake||request.party||
       before.bDead||!before.dwHP||!SupportedPostures(*before.payload)||
       std::any_of(request.targets.begin(),request.targets.end(),[&](const auto& t){return t.id!=c.char_id||t.type!=1;}))
        throw std::runtime_error("Unsupported native FINISHSKILL scope");
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;
    if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||phase!="ready")throw std::runtime_error("Finish claim is not ready primary");
    CheckCatalogs(sql);
    const auto receipt=ReadReceipt(sql,c,m_config.server,m_config.owner_token);
    const bool graph=bool(before.payload->transfer_state);
    if(!receipt.found||!receipt.core_matches||receipt.outcome!="active"||(graph?receipt.contract!=2:(receipt.contract!=3&&receipt.contract!=4)))
        throw std::runtime_error("Finish recovery receipt changed");
    const long long key=c.key;int unlocked=0;
    sql<<"SELECT 1 FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\"=:k AND \"bLocked\"=0",soci::use(key),soci::into(unlocked);
    if(!sql.got_data())throw std::runtime_error("Finish session was revoked");
    ValidateInventoryState(sql,c,before,true);
    auto after=before;RefreshEquipment(sql,after,true);
    if(after.dwHP!=before.dwHP||after.dwMP!=before.dwMP)throw std::runtime_error("Finish resources exceed source maximum");
    const auto def=std::find_if(after.payload->skill_templates.begin(),after.payload->skill_templates.end(),
        [&](const auto& t){return t.wID==request.skill;});
    if(def==after.payload->skill_templates.end()||def->instant_cure.empty()||!def->attack_profile||
       def->attack_profile->critical==255||(def->wMapID!=0xffff&&def->wMapID!=after.wMapID))
        throw std::runtime_error("FINISHSKILL effect contract is not implemented");
    const auto definition=*def;const auto power=*definition.attack_profile;
    const int world=c.group,character=c.char_id,skill=request.skill;
    const auto unhex=[](const std::string& value){std::vector<std::byte> out;
        for(std::size_t i=0;i<value.size();i+=2)out.push_back(static_cast<std::byte>(std::stoul(value.substr(i,2),nullptr,16)));return out;};
    const auto hex=[](auto bytes){constexpr char digits[]="0123456789abcdef";std::string out;
        for(auto v:bytes){const auto b=std::to_integer<unsigned>(v);out+=digits[b>>4];out+=digits[b&15];}return out;};
    long long accepted=0;
    // Original protocol supplies no cast ID. Choose the oldest outstanding
    // compatible cast from this login session. No wall-clock expiry is guessed.
    // The account/claim lock serializes cast, finish and handoff transactions.
    {
        soci::rowset<soci::row> casts=(sql.prepare<<
            "SELECT cast_id,CASE WHEN is_loop THEN 1 ELSE 0 END,encode(request,'hex'),encode(acknowledgement,'hex'),"
            "CASE WHEN EXISTS(SELECT 1 FROM app_world.skill_action_operations x WHERE x.action_id=a.charge_action_id "
            "AND x.charge_ms>0 AND x.charge_ms<=EXTRACT(EPOCH FROM (clock_timestamp()-x.started_at))*1000) THEN 1 ELSE 0 END "
            "FROM app_world.accepted_skill_casts a WHERE world_id=:w AND char_id=:c AND session_key=:k AND skill_id=:skill "
            "AND NOT EXISTS(SELECT 1 FROM app_world.skill_finish_operations f WHERE f.accepted_cast_id=a.cast_id) ORDER BY cast_id",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(key,"k"),soci::use(skill,"skill"));
        for(const auto& row:casts) {
            if(definition.action_time&&row.get<int>(4)!=1)continue;
            const auto original=unhex(row.get<std::string>(2)),ack=unhex(row.get<std::string>(3));
            const bool loop=row.get<int>(1)!=0;const std::size_t fixed=loop?45:62;
            if(original.size()<10||ack.size()<fixed)throw std::runtime_error("Malformed accepted cast receipt");
            wire::Reader route(original.data()+5,3);std::uint8_t channel{};std::uint16_t map{};route.Read(channel);route.Read(map);
            if(channel!=c.channel||map!=before.wMapID)continue;
            wire::Reader ground(ack.data()+fixed-13,12);float x{},y{},z{};ground.Read(x);ground.Read(y);ground.Read(z);
            if(x!=request.x||y!=request.y||z!=request.z)continue;
            const auto count=std::to_integer<unsigned>(ack[fixed-1]);
            if(count>16||ack.size()!=fixed+count*5)throw std::runtime_error("Invalid accepted target list");
            std::vector<SkillTarget> targets;wire::Reader input(ack.data()+fixed,count*5);
            for(unsigned i=0;i<count;++i){SkillTarget target;input.Read(target.id);input.Read(target.type);targets.push_back(target);}
            if(!FinishTargetsMatch(targets,request.targets))continue;
            accepted=row.get<long long>(0);break;
        }
    }
    if(!accepted)throw std::runtime_error("FINISHSKILL has no unconsumed accepted cast");
    FinishSkillCommit result;
    const auto emit=[&](std::uint16_t id,std::vector<std::byte> bytes,FinishAudience audience){
        result.packets.emplace_back(id,std::move(bytes));result.audiences.push_back(audience);
    };
    using tnetlib::protocol::MessageId;
    const auto erase=[&](std::size_t index) {
        const auto max_hp=after.dwMaxHP,max_mp=after.dwMaxMP;
        auto p=std::make_shared<CharacterPayload>(*after.payload);
        const auto skill=p->effects->at(index).skill;p->effects->erase(p->effects->begin()+index);after.payload=p;
        RefreshEquipment(sql,after,true);
        emit(static_cast<std::uint16_t>(MessageId::CS_SKILLEND_ACK),EncodeSkillEnd(c.char_id,skill),FinishAudience::Neighbors);
        if(max_hp!=after.dwMaxHP||max_mp!=after.dwMaxMP)
            emit(static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK),EncodeHpMpAck(c.char_id,1,after.dwMaxHP,after.dwHP,after.dwMaxMP,after.dwMP),FinishAudience::Neighbors);
        if(!after.payload->statistics)throw std::runtime_error("Finished effect removal lacks source statistics");
        emit(static_cast<std::uint16_t>(MessageId::CS_CHARSTATINFO_ACK),EncodeCharacterStatistics(after,*after.payload->statistics),FinishAudience::Owner);
    };
    for(const auto& target:request.targets) {
        (void)target;const auto hp=after.dwHP,mp=after.dwMP;
        // PerformSkill rolls each ordered row separately, not their sum.
        for(const auto& row:definition.instant_cure) {
            const auto value=skill_effect::Calculate(row,definition,power.rank,row.bExec==SCT_HP?after.dwMaxHP:after.dwMaxMP);
            if(value<0)throw std::runtime_error("Cure magnitude outside supported positive source domain");
            unsigned char random{};if(RAND_bytes(&random,1)!=1)throw std::runtime_error("Cure randomness unavailable");
            const auto delta=static_cast<std::uint64_t>(value)+static_cast<std::uint64_t>(value)*(random&15)/100;
            auto& resource=row.bExec==SCT_HP?after.dwHP:after.dwMP;
            // DWORD source arithmetic, clamped after all rows by Defend.
            if(delta+resource>0xffffffffULL)throw std::runtime_error("Cure would overflow original DWORD");
            resource+=static_cast<std::uint32_t>(delta);
        }
        after.dwHP=std::min(after.dwHP,after.dwMaxHP);after.dwMP=std::min(after.dwMP,after.dwMaxMP);
        // Defend calls CheckEternalBuff after applying the effect, before its
        // DEFEND/HPMP reply. Use catalog bIsuse/dwDuration, including a timed
        // ForceMaintain override such as Soul Lottery820.
        if(!after.dwMP)for(std::size_t i=0;i<after.payload->effects->size();)
            if(ErasesAtZeroMp(*after.payload,after.payload->effects->at(i).skill))erase(i);else ++i;
        emit(static_cast<std::uint16_t>(MessageId::CS_DEFEND_ACK),EncodeCureDefend(after,request,power),FinishAudience::Nearby);
        if(after.dwHP!=hp||after.dwMP!=mp)emit(static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK),
            EncodeHpMpAck(c.char_id,1,after.dwMaxHP,after.dwHP,after.dwMaxMP,after.dwMP),FinishAudience::Nearby);
    }
    // FINISHSKILL ends with CheckEquipSkill even for an empty filtered target
    // list. IsEquipSkillItem deliberately includes broken weapons.
    for(std::size_t i=0;i<after.payload->effects->size();) {
        const auto& effect=after.payload->effects->at(i);
        if(!IsNativePosture(effect.skill)){++i;continue;}
        const auto& definition=after.payload->posture_templates.at(effect.skill-131);
        bool suitable=!definition.weapons;
        for(const auto& bag:after.payload->bags)if(bag.bag.bInvenID==254)for(const auto& item:bag.items)
            if(item.bKind&&item.bKind<=32&&(definition.weapons&(std::uint32_t{1}<<(item.bKind-1))))suitable=true;
        if(effect.attack_type==1&&effect.attack_id==c.char_id&&!suitable)erase(i);else ++i;
    }
    const auto before_effects=MaintainJson(before),after_effects=MaintainJson(after);
    if(!graph&&before_effects!=after_effects)WriteMaintainedEffects(sql,c,after);
    std::string responses="[";
    for(const auto& [id,packet]:result.packets){if(responses.size()>1)responses+=',';responses+='['+std::to_string(id)+",\""+hex(packet)+"\"]";}
    responses+=']';std::string audiences="[";
    for(auto audience:result.audiences){if(audiences.size()>1)audiences+=',';audiences+=std::to_string(static_cast<unsigned>(audience));}
    audiences+=']';const auto request_hex=hex(body),fingerprint=Fingerprint(c,after);
    const int server=m_config.server;const long long generation=c.connection_id,epoch=c.authority_epoch,previous=before.payload->last_finish_id;
    const long long bhp=before.dwHP,ahp=after.dwHP,bmp=before.dwMP,amp=after.dwMP;long long finish=0;
    sql<<"INSERT INTO app_world.skill_finish_operations(world_id,char_id,accepted_cast_id,previous_finish_id,server_id,owner_token,connection_id,session_key,authority_epoch,request,responses,before_hp,after_hp,before_mp,after_mp,core_fingerprint,before_effects,after_effects,response_audiences) "
         "VALUES(:w,:c,:cast,:previous,:s,:t,:g,:k,:e,decode(:request,'hex'),CAST(:responses AS jsonb),:bhp,:ahp,:bmp,:amp,:f,CAST(:be AS jsonb),CAST(:ae AS jsonb),CAST(:audiences AS jsonb)) RETURNING finish_id",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(accepted,"cast"),soci::use(previous,"previous"),soci::use(server,"s"),
        soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(key,"k"),soci::use(epoch,"e"),
        soci::use(request_hex,"request"),soci::use(responses,"responses"),soci::use(bhp,"bhp"),soci::use(ahp,"ahp"),
        soci::use(bmp,"bmp"),soci::use(amp,"amp"),soci::use(fingerprint,"f"),soci::use(before_effects,"be"),soci::use(after_effects,"ae"),soci::use(audiences,"audiences"),soci::into(finish);
    auto payload=std::make_shared<CharacterPayload>(*after.payload);payload->last_finish_id=static_cast<std::uint64_t>(finish);after.payload=std::move(payload);
    WriteCore(sql,c,after,0);RecordCheckpoint(sql,c,receipt.revision,fingerprint,"active");StoreTransferCheckpoint(sql,c,after);
    result.snapshot=std::make_shared<const CharSnapshot>(std::move(after));tx->commit();return result;
}
EffectEndCommit PostgreSQLMapService::EndMaintainedEffect(const MapSessionClaim& c,const EffectEndRequest& request,
    const CharSnapshot& before) {
    const bool graph=before.payload&&before.payload->transfer_state;
    if(c.role!=MapSessionRole::Primary||(!graph&&c.authority_epoch)||!before.payload||
       before.dwCharID!=c.char_id||before.persistence_uncertain||request.object_type!=1||request.object!=c.char_id)
        throw std::runtime_error("Unsupported native effect cancellation");
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;
    if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||phase!="ready")throw std::runtime_error("Effect claim is not ready primary");
    CheckCatalogs(sql);
    const auto receipt=ReadReceipt(sql,c,m_config.server,m_config.owner_token);
    if(!receipt.found||!receipt.core_matches||receipt.outcome!="active"||(graph?receipt.contract!=2:(receipt.contract!=3&&receipt.contract!=4)))
        throw std::runtime_error("Effect recovery receipt changed");
    const long long key=c.key;int unlocked=0;
    sql<<"SELECT 1 FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\"=:k AND \"bLocked\"=0",soci::use(key),soci::into(unlocked);
    if(!sql.got_data())throw std::runtime_error("Effect session was revoked");
    const auto before_graph=ValidateInventoryState(sql,c,before,true);
    auto after=before;
    const auto& effects=MaintainedEffects(*before.payload);
    const auto found=std::find_if(effects.begin(),effects.end(),[&](const auto& e){
        return e.attack_id==request.attacker&&e.attack_type==request.attack_type&&e.skill==request.skill;
    });
    const int removed=found!=effects.end()?1:0;
    if(removed) {
        // Original OnCS_SKILLEND_REQ ignores host/map/channel and bCanCancel
        // for one's own PC. Erase only the first attacker/type/skill match.
        // Other effects require their own hide/death/recall/stat side effects.
        if(!SupportedPostures(*before.payload))throw std::runtime_error("Effect erase semantics are not implemented for this maintained state");
        auto p=std::make_shared<CharacterPayload>(*before.payload);
        p->effects->erase(p->effects->begin()+std::distance(effects.begin(),found));after.payload=std::move(p);
        RefreshEquipment(sql,after,true);
        if(!graph)WriteMaintainedEffects(sql,c,after);
    }
    const auto fingerprint=Fingerprint(c,after),after_graph=graph?TransferFingerprint(c,after):std::string{};
    WriteCore(sql,c,after,0);RecordCheckpoint(sql,c,receipt.revision,fingerprint,"active");StoreTransferCheckpoint(sql,c,after);
    constexpr char digits[]="0123456789abcdef";std::string wire;
    const auto append=[&]<class T>(T v){for(std::size_t n=0;n<sizeof(T);++n){const auto b=v&255;wire+=digits[b>>4];wire+=digits[b&15];v>>=8;}};
    append(request.object);append(request.object_type);append(request.host);append(request.attacker);
    append(request.attack_type);append(request.skill);append(request.map);append(request.channel);
    const int world=c.group,character=c.char_id,server=m_config.server,contract=graph?2:3;
    const long long generation=c.connection_id,epoch=c.authority_epoch;
    const auto before_effects=MaintainJson(before),after_effects=MaintainJson(after);
    sql<<"INSERT INTO app_world.maintained_effect_operations(world_id,char_id,server_id,owner_token,connection_id,authority_epoch,state_contract,request,removed,before_effects,after_effects,before_graph_hash,after_graph_hash,core_fingerprint) "
         "VALUES(:w,:c,:s,:t,:g,:e,:contract,decode(:request,'hex'),:n,CAST(:be AS jsonb),CAST(:ae AS jsonb),NULLIF(:gb,''),NULLIF(:ga,''),:f)",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(server,"s"),soci::use(m_config.owner_token,"t"),
        soci::use(generation,"g"),soci::use(epoch,"e"),soci::use(contract,"contract"),soci::use(wire,"request"),soci::use(removed,"n"),
        soci::use(before_effects,"be"),soci::use(after_effects,"ae"),soci::use(before_graph,"gb"),soci::use(after_graph,"ga"),soci::use(fingerprint,"f");
    tx->commit();return {std::make_shared<const CharSnapshot>(std::move(after)),removed!=0};
}
InventoryMoveCommit PostgreSQLMapService::MoveInventoryItems(const MapSessionClaim& c,const InventoryMoveRequest& request,
    const CharSnapshot& before,const CharSnapshot& after) {
    const bool graph=before.payload&&before.payload->transfer_state;
    if(c.role!=MapSessionRole::Primary||(!graph&&c.authority_epoch)||!before.payload||!after.payload||
       before.dwCharID!=c.char_id||before.persistence_uncertain||before.bDead||!before.dwHP)
        throw std::runtime_error("Unsupported native inventory move");
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;
    if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||phase!="ready")throw std::runtime_error("Inventory claim is not ready primary");
    CheckCatalogs(sql);
    const auto receipt=ReadReceipt(sql,c,m_config.server,m_config.owner_token);
    if(!receipt.found||!receipt.core_matches||receipt.outcome!="active"||(graph?receipt.contract!=2:(receipt.contract!=3&&receipt.contract!=4)))
        throw std::runtime_error("Inventory recovery receipt changed");
    const long long key=c.key;int unlocked=0;
    sql<<"SELECT 1 FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\"=:k AND \"bLocked\"=0",soci::use(key),soci::into(unlocked);
    if(!sql.got_data())throw std::runtime_error("Inventory session was revoked");
    const auto plan=ValidateInventoryMove(sql,c,request,before,after);
    const auto fingerprint=Fingerprint(c,plan.after);
    WriteCore(sql,c,plan.after,0);RecordCheckpoint(sql,c,receipt.revision,fingerprint,"active");StoreTransferCheckpoint(sql,c,plan.after);
    long long operation=0;
    sql<<"SELECT nextval('app_world.inventory_movements_movement_id_seq')",soci::into(operation);
    const int world=c.group,character=c.char_id,server=m_config.server,contract=graph?2:3;
    const long long generation=c.connection_id,epoch=c.authority_epoch;
    if(plan.move.kind==InventoryMoveKind::Equipment) {
        constexpr char digits[]="0123456789abcdef";std::string wire;
        for(auto value:{request.source_bag,request.source_slot,request.destination_bag,request.destination_slot,request.count}){wire+=digits[value>>4];wire+=digits[value&15];}
        const int changed=static_cast<int>(plan.move.items.size());
        const auto before_effects=MaintainJson(before),after_effects=MaintainJson(plan.after);
        sql<<"INSERT INTO app_world.equipment_operations(operation_id,world_id,char_id,server_id,owner_token,connection_id,authority_epoch,state_contract,request,changed_items,before_graph_hash,after_graph_hash,core_fingerprint,before_effects,after_effects) "
             "VALUES(:op,:w,:c,:s,:t,:g,:e,:contract,decode(:request,'hex'),:n,NULLIF(:gb,''),NULLIF(:ga,''),:f,CAST(:be AS jsonb),CAST(:ae AS jsonb))",
            soci::use(operation,"op"),soci::use(world,"w"),soci::use(character,"c"),soci::use(server,"s"),soci::use(m_config.owner_token,"t"),
            soci::use(generation,"g"),soci::use(epoch,"e"),soci::use(contract,"contract"),soci::use(wire,"request"),soci::use(changed,"n"),
            soci::use(plan.before_graph,"gb"),soci::use(plan.after_graph,"ga"),soci::use(fingerprint,"f"),soci::use(before_effects,"be"),soci::use(after_effects,"ae");
    }
    for(std::size_t i=0;i<plan.move.items.size();++i) {
        const auto& move=plan.move.items[i];const long long id=std::bit_cast<std::int64_t>(move.before.dlID);
        const int source_bag=move.before.bInvenID,source_slot=move.before.bItemID,dest_bag=move.bag,dest_slot=move.slot,count=move.before.bCount;
        if(plan.move.kind==InventoryMoveKind::Equipment) {
            const long long changed_id=move.created?static_cast<long long>(plan.committed.created_id):id;
            const int ordinal=static_cast<int>(i),old_count=move.created?0:count,new_count=move.count;
            const std::string old_hash=move.created?"":move.before.durable_hash;
            sql<<"INSERT INTO app_world.equipment_item_changes(operation_id,ordinal,item_id,parent_id,source_bag,source_slot,destination_bag,destination_slot,before_count,after_count,before_hash,after_hash) "
                 "VALUES(:op,:ord,:id,:parent,:sb,:ss,:db,:ds,:bc,:ac,NULLIF(:bh,''),NULLIF(:ah,''))",
                soci::use(operation,"op"),soci::use(ordinal,"ord"),soci::use(changed_id,"id"),soci::use(id,"parent"),
                soci::use(source_bag,"sb"),soci::use(source_slot,"ss"),soci::use(dest_bag,"db"),soci::use(dest_slot,"ds"),
                soci::use(old_count,"bc"),soci::use(new_count,"ac"),soci::use(old_hash,"bh"),soci::use(plan.committed.hashes[i],"ah");
            continue;
        }
        if(plan.move.kind==InventoryMoveKind::Split||plan.move.kind==InventoryMoveKind::Merge) {
            const std::string kind=plan.move.kind==InventoryMoveKind::Split?"split":"merge";
            const long long changed_id=move.created?static_cast<long long>(plan.committed.created_id):id;
            const int ordinal=static_cast<int>(i),old_count=move.created?0:count,new_count=move.count;
            const std::string old_hash=move.created?"":move.before.durable_hash;
            sql<<"INSERT INTO app_world.inventory_stack_changes(operation_id,ordinal,kind,world_id,char_id,server_id,owner_token,connection_id,authority_epoch,state_contract,"
                 "item_id,parent_id,source_bag,source_slot,destination_bag,destination_slot,before_count,after_count,before_hash,after_hash,before_graph_hash,after_graph_hash,core_fingerprint) "
                 "VALUES(:op,:ord,:kind,:w,:c,:s,:t,:g,:e,:contract,:id,:parent,:sb,:ss,:db,:ds,:bc,:ac,NULLIF(:bh,''),NULLIF(:ah,''),NULLIF(:gb,''),NULLIF(:ga,''),:f)",
                soci::use(operation,"op"),soci::use(ordinal,"ord"),soci::use(kind,"kind"),soci::use(world,"w"),soci::use(character,"c"),
                soci::use(server,"s"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(epoch,"e"),
                soci::use(contract,"contract"),soci::use(changed_id,"id"),soci::use(id,"parent"),
                soci::use(source_bag,"sb"),soci::use(source_slot,"ss"),soci::use(dest_bag,"db"),soci::use(dest_slot,"ds"),
                soci::use(old_count,"bc"),soci::use(new_count,"ac"),soci::use(old_hash,"bh"),soci::use(plan.committed.hashes[i],"ah"),
                soci::use(plan.before_graph,"gb"),soci::use(plan.after_graph,"ga"),soci::use(fingerprint,"f");
            continue;
        }
        sql<<"INSERT INTO app_world.inventory_movements(operation_id,world_id,char_id,server_id,owner_token,connection_id,authority_epoch,state_contract,"
             "item_id,source_bag,source_slot,destination_bag,destination_slot,item_count,before_hash,after_hash,before_graph_hash,after_graph_hash,core_fingerprint) "
             "VALUES(:op,:w,:c,:s,:t,:g,:e,:contract,:id,:sb,:ss,:db,:ds,:n,:bh,:ah,NULLIF(:gb,''),NULLIF(:ga,''),:f)",
            soci::use(operation,"op"),soci::use(world,"w"),soci::use(character,"c"),soci::use(server,"s"),soci::use(m_config.owner_token,"t"),
            soci::use(generation,"g"),soci::use(epoch,"e"),soci::use(contract,"contract"),soci::use(id,"id"),
            soci::use(source_bag,"sb"),soci::use(source_slot,"ss"),soci::use(dest_bag,"db"),soci::use(dest_slot,"ds"),soci::use(count,"n"),
            soci::use(move.before.durable_hash,"bh"),soci::use(plan.committed.hashes[i],"ah"),soci::use(plan.before_graph,"gb"),soci::use(plan.after_graph,"ga"),soci::use(fingerprint,"f");
    }
    tx->commit();return plan.committed;
}
void PostgreSQLMapService::CheckCastHead(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) {
    CheckPvpBalance(sql,c,s);
    CheckQuestProgress(sql,c,s);
    CheckDuringItems(sql,c,s);
    CheckItemCooldowns(sql,c,s);
    if(!s.payload)throw std::runtime_error("Cast ledger requires native snapshot");
    const auto security=ReadSecurity(sql,c);
    auto expected=s.payload->security;expected.hydrated=true;
    if(expected!=security)throw std::runtime_error("Snapshot predates secure code state");
    const int world=c.group,character=c.char_id;long long head=0;
    sql<<"SELECT COALESCE(max(cast_id),0) FROM app_world.accepted_skill_casts WHERE world_id=:w AND char_id=:c",soci::use(world,"w"),soci::use(character,"c"),soci::into(head);
    if(static_cast<std::uint64_t>(head)!=s.payload->last_cast_id)throw std::runtime_error("Snapshot predates accepted cast");
    sql<<"SELECT COALESCE(max(finish_id),0) FROM app_world.skill_finish_operations WHERE world_id=:w AND char_id=:c",soci::use(world,"w"),soci::use(character,"c"),soci::into(head);
    if(static_cast<std::uint64_t>(head)!=s.payload->last_finish_id)throw std::runtime_error("Snapshot predates finished skill");
    sql<<"SELECT COALESCE(max(action_id),0) FROM app_world.skill_action_operations WHERE world_id=:w AND char_id=:c",soci::use(world,"w"),soci::use(character,"c"),soci::into(head);
    if(static_cast<std::uint64_t>(head)!=s.payload->last_action_id)throw std::runtime_error("Snapshot predates skill action");
    sql<<"SELECT COALESCE(max(purchase_id),0) FROM app_world.skill_purchase_operations WHERE world_id=:w AND char_id=:c",soci::use(world,"w"),soci::use(character,"c"),soci::into(head);
    if(static_cast<std::uint64_t>(head)!=s.payload->last_purchase_id)throw std::runtime_error("Snapshot predates skill purchase");
    sql<<"SELECT COALESCE(max(selection_id),0) FROM app_world.title_selection_operations WHERE world_id=:w AND char_id=:c",soci::use(world,"w"),soci::use(character,"c"),soci::into(head);
    if(static_cast<std::uint64_t>(head)!=s.payload->last_title_selection_id)throw std::runtime_error("Snapshot predates title selection");
}
void PostgreSQLMapService::WriteCore(soci::session& sql,const MapSessionClaim& claim,const CharSnapshot& s,int logout) {
    CheckCastHead(sql,claim,s);
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
