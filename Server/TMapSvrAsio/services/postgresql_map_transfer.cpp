#include "postgresql_map_service.h"
#include "postgresql_map_owner.h"
#include "main_transfer_codec.h"
#include "main_transfer_runtime.h"
#include <openssl/sha.h>
#include <limits>
#include <stdexcept>

namespace tmapsvr {
namespace {
std::string Hex(std::span<const std::byte> bytes) {
    constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(bytes.size()*2);
    for(auto b:bytes){const auto n=std::to_integer<unsigned char>(b);out+=digits[n>>4];out+=digits[n&15];}return out;
}
std::string Hash(std::span<const std::byte> bytes) {
    unsigned char digest[SHA256_DIGEST_LENGTH];SHA256(reinterpret_cast<const unsigned char*>(bytes.data()),bytes.size(),digest);
    return Hex({reinterpret_cast<const std::byte*>(digest),sizeof(digest)});
}
}
std::string PostgreSQLMapService::TransferFingerprint(const MapSessionClaim& c,const CharSnapshot& s) const {
    if(!s.payload||!s.payload->transfer_state)return {};
    SkillCooldownTracker timers;timers.Restore(c.char_id,s.payload->skills,0);
    return Hash(transfer::Encode(transfer::Capture(s,c.key,timers,0)));
}
void PostgreSQLMapService::StoreTransferCheckpoint(soci::session& sql,const MapSessionClaim& c,const CharSnapshot& s) const {
    const int world=c.group,character=c.char_id;
    if(!s.payload||!s.payload->transfer_state) {
        if(c.authority_epoch)throw std::runtime_error("Transferred primary has no complete graph checkpoint");
        StoreSkillCheckpoint(sql,c,s);return;
    }
    SkillCooldownTracker timers;timers.Restore(c.char_id,s.payload->skills,0);
    const auto body=transfer::Encode(transfer::Capture(s,c.key,timers,0));const auto hex=Hex(body),hash=Hash(body);
    sql<<"UPDATE app_world.map_checkpoints SET recovery_contract=2,skill_state=NULL,maintain_state=NULL,transfer_body=decode(:body,'hex'),transfer_hash=:hash,character_manifest=:cm,routing_manifest=:rm,actor_manifest=:am "
         "WHERE world_id=:w AND char_id=:c",
        soci::use(hex,"body"),soci::use(hash,"hash"),soci::use(m_config.character_manifest,"cm"),soci::use(m_config.routing_manifest,"rm"),
        soci::use(m_config.actor_manifest,"am"),soci::use(world,"w"),soci::use(character,"c");
}
std::optional<CharSnapshot> PostgreSQLMapService::RestoreTransferCheckpoint(soci::session& sql,const MapSessionClaim& c) const {
    const int world=c.group,character=c.char_id,user=c.user_id;std::string hex,cm,rm,am,fingerprint;
    int matches=0;
    sql<<"SELECT encode(transfer_body,'hex'),character_manifest,routing_manifest,actor_manifest,fingerprint,"
         "CASE WHEN app_world.map_checkpoint_matches(map_checkpoints) THEN 1 ELSE 0 END "
         "FROM app_world.map_checkpoints WHERE world_id=:w AND char_id=:c AND user_id=:u AND transfer_body IS NOT NULL AND outcome IN ('logout','recovered') FOR UPDATE",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(user,"u"),soci::into(hex),soci::into(cm),soci::into(rm),soci::into(am),soci::into(fingerprint),soci::into(matches);
    if(!sql.got_data())return {};
    bool actor_matches=am==m_config.actor_manifest;
    if(!actor_matches) {
        // Only an explicit owner-published, directional proof can bridge the
        // additive actor catalog extension. Never relax character/routing hashes,
        // mutate the old receipt here, or traverse compatibility chains.
        int certified=0;
        sql<<"SELECT count(*) FROM actor_compat.transfer_catalog_compatibility "
             "WHERE source_manifest_sha256=:old AND target_manifest_sha256=:current",
            soci::use(am,"old"),soci::use(m_config.actor_manifest,"current"),soci::into(certified);
        actor_matches=certified==1;
    }
    if(!matches||cm!=m_config.character_manifest||rm!=m_config.routing_manifest||!actor_matches)
        throw std::runtime_error("Recovered transfer graph core or catalogs changed");
    std::vector<std::byte> body;body.reserve(hex.size()/2);
    for(std::size_t i=0;i<hex.size();i+=2)body.push_back(static_cast<std::byte>(std::stoul(hex.substr(i,2),nullptr,16)));
    auto state=transfer::Decode(body);
    if(!state||state->db_load||state->character.dwCharID!=c.char_id)throw std::runtime_error("Recovered transfer graph is invalid");
    state->key=c.key;auto snap=HydrateTransfer(sql,*state);
    if(CoreFingerprint(c,snap)!=fingerprint)throw std::runtime_error("Recovered transfer graph disagrees with core receipt");
    return snap;
}
bool PostgreSQLMapService::PrepareTransfer(const MapSessionClaim& c,const CharSnapshot& snapshot,std::span<const std::byte> body) {
    auto state=transfer::Decode(body);
    if(c.role!=MapSessionRole::Primary||!state||state->db_load||state->character.dwCharID!=c.char_id||state->key!=c.key||
       c.authority_epoch>=static_cast<std::uint64_t>(std::numeric_limits<long long>::max())||!c.endpoint_ip||!c.endpoint_port)return false;
    auto wire_core=state->character;wire_core.payload=snapshot.payload;
    const auto fingerprint=CoreFingerprint(c,snapshot);
    if(CoreFingerprint(c,wire_core)!=fingerprint)return false;
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||(phase!="ready"&&phase!="transferring"))return false;
    CheckCastHead(sql,c,snapshot);
    CheckCatalogs(sql);
    const auto& s=state->character;const int target=CellOwner(sql,c,s.wMapID,s.fPosX,s.fPosZ);
    if(!target||target==m_config.server)return false;
    const int world=c.group,character=c.char_id,user=c.user_id,key=c.key,channel=c.channel,source=m_config.server,port=c.endpoint_port;
    const long long generation=c.connection_id,epoch=c.authority_epoch,ip=c.endpoint_ip;
    const auto hex=Hex(body),hash=Hash(body);int found=0;
    if(phase=="transferring") {
        // Exact retry can confirm a lost prepare response; never extend expiry.
        sql<<"SELECT 1 FROM app_world.map_transfers WHERE world_id=:w AND char_id=:c AND source_token=:t AND source_connection=:g "
             "AND source_epoch=:e AND target_server=:target AND body=decode(:body,'hex') AND phase='prepared' AND expires_at>clock_timestamp()",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(epoch,"e"),
            soci::use(target,"target"),soci::use(hex,"body"),soci::into(found);
        if(!sql.got_data())return false;tx->commit();return true;
    }
    sql<<"SELECT 1 FROM app_world.map_checkpoints WHERE world_id=:w AND char_id=:c AND user_id=:u AND session_key=:k "
         "AND server_id=:s AND owner_token=:t AND connection_id=:g AND authority_epoch=:e AND outcome='active' AND recovery_contract IN (1,2,3,4) "
         "AND app_world.map_checkpoint_matches(map_checkpoints) FOR UPDATE",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(user,"u"),soci::use(key,"k"),soci::use(source,"s"),
        soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(epoch,"e"),soci::into(found);
    if(!sql.got_data())return false;
    std::string target_token;long long target_connection=0;
    // No target-owner row lock after the account lock. Replacement may invalidate
    // this prepared token; target consumption independently fences its own owner.
    sql<<"SELECT r.target_token,r.connection_id FROM app_world.map_replicas r JOIN app_world.map_runtime_owner o "
         "ON o.world_id=r.world_id AND o.server_id=r.target_server AND o.owner_token=r.target_token "
         "WHERE r.world_id=:w AND r.char_id=:c AND r.primary_server=:s AND r.primary_token=:t AND r.primary_connection=:g "
         "AND r.primary_epoch=:e AND r.target_server=:target AND r.phase='ready' "
         "AND r.routing_manifest=:rm AND r.character_manifest=:cm AND r.actor_manifest=:am FOR UPDATE OF r",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(source,"s"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),
        soci::use(epoch,"e"),soci::use(target,"target"),soci::use(m_config.routing_manifest,"rm"),soci::use(m_config.character_manifest,"cm"),
        soci::use(m_config.actor_manifest,"am"),soci::into(target_token),soci::into(target_connection);
    if(!sql.got_data())return false;
    sql<<R"SQL(INSERT INTO app_world.map_transfers(world_id,char_id,user_id,session_key,channel,
      source_server,source_token,source_connection,source_epoch,source_ip,source_port,
      target_server,target_token,target_connection,target_epoch,routing_manifest,character_manifest,actor_manifest,
      body,body_sha256,core_fingerprint,phase)
      VALUES(:w,:c,:u,:k,:ch,:s,:t,:g,:e,:ip,:port,:target,:tt,:tg,:e+1,:rm,:cm,:am,decode(:body,'hex'),:hash,:fp,'prepared'))SQL",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(user,"u"),soci::use(key,"k"),soci::use(channel,"ch"),soci::use(source,"s"),
        soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(epoch,"e"),soci::use(ip,"ip"),soci::use(port,"port"),
        soci::use(target,"target"),soci::use(target_token,"tt"),soci::use(target_connection,"tg"),soci::use(m_config.routing_manifest,"rm"),
        soci::use(m_config.character_manifest,"cm"),soci::use(m_config.actor_manifest,"am"),soci::use(hex,"body"),soci::use(hash,"hash"),soci::use(fingerprint,"fp");
    sql<<"UPDATE app_world.map_sessions SET phase='transferring',updated_at=clock_timestamp() WHERE world_id=:w AND char_id=:c",
        soci::use(world,"w"),soci::use(character,"c");
    tx->commit();return true;
}
std::optional<TransferredCharacter> PostgreSQLMapService::AcceptTransfer(const MapSessionClaim& c,std::span<const std::byte> body) {
    auto state=transfer::Decode(body);
    if(c.role!=MapSessionRole::Replica||!state||state->db_load||state->character.dwCharID!=c.char_id||state->key!=c.key)return {};
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    if(!LockAccount(sql,c))return {};CheckCatalogs(sql);
    const int world=c.group,character=c.char_id,user=c.user_id,key=c.key,channel=c.channel,target=m_config.server;
    const long long generation=c.connection_id;const auto hex=Hex(body);
    long long id=0,source_connection=0,source_epoch=0,target_epoch=0,source_ip=0;
    int source=0,source_port=0,unexpired=0;std::string source_token,phase,fingerprint;
    sql<<R"SQL(SELECT transfer_id,source_server,source_token,source_connection,source_epoch,target_epoch,source_ip,source_port,
      phase,core_fingerprint,CASE WHEN expires_at>clock_timestamp() THEN 1 ELSE 0 END FROM app_world.map_transfers
      WHERE world_id=:w AND char_id=:c AND user_id=:u AND session_key=:k AND channel=:ch
      AND target_server=:s AND target_token=:t AND target_connection=:g AND body=decode(:body,'hex')
      AND routing_manifest=:rm AND character_manifest=:cm AND actor_manifest=:am AND phase IN ('prepared','consumed')
      ORDER BY transfer_id DESC LIMIT 1 FOR UPDATE)SQL",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(user,"u"),soci::use(key,"k"),soci::use(channel,"ch"),soci::use(target,"s"),
        soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(hex,"body"),soci::use(m_config.routing_manifest,"rm"),
        soci::use(m_config.character_manifest,"cm"),soci::use(m_config.actor_manifest,"am"),soci::into(id),soci::into(source),soci::into(source_token),
        soci::into(source_connection),soci::into(source_epoch),soci::into(target_epoch),soci::into(source_ip),soci::into(source_port),
        soci::into(phase),soci::into(fingerprint),soci::into(unexpired);
    if(!sql.got_data()||(phase=="prepared"&&!unexpired))return {};
    MapSessionClaim promoted=c;promoted.role=MapSessionRole::Primary;promoted.authority_epoch=target_epoch;
    if(phase=="consumed") {
        // Confirmation only while the committed load has not entered gameplay.
        std::string current;
        if(!LockClaim(sql,promoted,current)||current!="loaded")return {};
        auto s=HydrateTransfer(sql,*state);
        int found=0;
        sql<<"SELECT 1 FROM app_world.map_checkpoints WHERE world_id=:w AND char_id=:c AND owner_token=:t "
             "AND connection_id=:g AND authority_epoch=:e AND revision=0 AND fingerprint=:f AND outcome='active' "
             "AND app_world.map_checkpoint_matches(map_checkpoints)",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),
            soci::use(target_epoch,"e"),soci::use(fingerprint,"f"),soci::into(found);
        if(!sql.got_data())return {};tx->commit();return TransferredCharacter{std::move(s),static_cast<std::uint64_t>(target_epoch)};
    }
    int found=0;
    sql<<R"SQL(SELECT 1 FROM app_world.map_sessions m
      JOIN app_global."TCURRENTUSER" u ON u."dwKEY"=m.session_key AND u."dwUserID"=m.user_id AND u."dwCharID"=m.char_id
        AND u."bGroupID"=m.world_id AND u."bChannel"=m.channel AND u."bLocked"=0
      JOIN app_world.map_replicas r ON r.world_id=m.world_id AND r.char_id=m.char_id AND r.primary_server=m.server_id
        AND r.primary_token=m.owner_token AND r.primary_connection=m.connection_id AND r.primary_epoch=m.authority_epoch
      JOIN app_world.map_checkpoints p ON p.world_id=m.world_id AND p.char_id=m.char_id AND p.user_id=m.user_id
        AND p.session_key=m.session_key AND p.server_id=m.server_id AND p.owner_token=m.owner_token
        AND p.connection_id=m.connection_id AND p.authority_epoch=m.authority_epoch AND p.outcome='active'
        AND app_world.map_checkpoint_matches(p)
      WHERE m.world_id=:w AND m.char_id=:c AND m.user_id=:u AND m.session_key=:k AND m.channel=:ch
      AND m.server_id=:s AND m.owner_token=:t AND m.connection_id=:g AND m.authority_epoch=:e AND m.phase='transferring'
      AND r.target_server=:target AND r.target_token=:tt AND r.connection_id=:tg AND r.phase='ready'
      FOR UPDATE OF m,u,r,p)SQL",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(user,"u"),soci::use(key,"k"),soci::use(channel,"ch"),soci::use(source,"s"),
        soci::use(source_token,"t"),soci::use(source_connection,"g"),soci::use(source_epoch,"e"),soci::use(target,"target"),
        soci::use(m_config.owner_token,"tt"),soci::use(generation,"tg"),soci::into(found);
    if(!sql.got_data())return {};
    if(CellOwner(sql,c,state->character.wMapID,state->character.fPosX,state->character.fPosZ)!=target)return {};
    auto s=HydrateTransfer(sql,*state);
    if(CoreFingerprint(promoted,s)!=fingerprint)return {};
    sql<<"DELETE FROM app_world.map_replicas WHERE world_id=:w AND char_id=:c AND target_server=:s",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(target,"s");
    sql<<"UPDATE app_world.map_replicas SET primary_server=:s,primary_token=:t,primary_connection=:g,primary_epoch=:e,updated_at=clock_timestamp() "
         "WHERE world_id=:w AND char_id=:c",
        soci::use(target,"s"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(target_epoch,"e"),soci::use(world,"w"),soci::use(character,"c");
    const int map=s.wMapID;const double x=s.fPosX,z=s.fPosZ;
    sql<<R"SQL(INSERT INTO app_world.map_replicas(world_id,char_id,primary_server,primary_token,primary_connection,primary_epoch,
      target_server,target_token,connection_id,endpoint_ip,endpoint_port,map_id,position_x,position_z,routing_manifest,character_manifest,actor_manifest,phase)
      VALUES(:w,:c,:s,:t,:g,:e,:source,:st,:sg,:ip,:port,:map,:x,:z,:rm,:cm,:am,'ready'))SQL",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(target,"s"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),
        soci::use(target_epoch,"e"),soci::use(source,"source"),soci::use(source_token,"st"),soci::use(source_connection,"sg"),
        soci::use(source_ip,"ip"),soci::use(source_port,"port"),soci::use(map,"map"),soci::use(x,"x"),soci::use(z,"z"),
        soci::use(m_config.routing_manifest,"rm"),soci::use(m_config.character_manifest,"cm"),soci::use(m_config.actor_manifest,"am");
    sql<<"UPDATE app_world.map_sessions SET server_id=:s,owner_token=:t,connection_id=:g,authority_epoch=:e,phase='loaded',updated_at=clock_timestamp() "
         "WHERE world_id=:w AND char_id=:c",
        soci::use(target,"s"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(target_epoch,"e"),soci::use(world,"w"),soci::use(character,"c");
    WriteCore(sql,promoted,s,0);
    sql<<"UPDATE app_world.map_checkpoints SET server_id=:s,owner_token=:t,connection_id=:g,authority_epoch=:e,revision=0,fingerprint=:f,"
         "core_state=app_world.map_core_state(world_id,char_id),saved_at=clock_timestamp() WHERE world_id=:w AND char_id=:c",
        soci::use(target,"s"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(target_epoch,"e"),
        soci::use(fingerprint,"f"),soci::use(world,"w"),soci::use(character,"c");
    sql<<"UPDATE app_world.map_transfers SET phase='consumed',settled_at=clock_timestamp() WHERE transfer_id=:id",soci::use(id);
    StoreTransferCheckpoint(sql,promoted,s);
    tx->commit();return TransferredCharacter{std::move(s),static_cast<std::uint64_t>(target_epoch)};
}
bool PostgreSQLMapService::OutgoingTransferCommitted(const MapSessionClaim& c) {
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    if(!LockAccount(sql,c))return false;
    const int world=c.group,character=c.char_id,user=c.user_id,key=c.key;
    const long long generation=c.connection_id,epoch=c.authority_epoch;int found=0;
    sql<<"SELECT 1 FROM app_world.map_transfers t JOIN app_world.map_sessions m ON m.world_id=t.world_id AND m.char_id=t.char_id "
         "AND m.server_id=t.target_server AND m.owner_token=t.target_token AND m.connection_id=t.target_connection AND m.authority_epoch=t.target_epoch "
         "WHERE t.world_id=:w AND t.char_id=:c AND t.user_id=:u AND t.session_key=:k AND t.source_token=:t "
         "AND t.source_connection=:g AND t.source_epoch=:e AND t.phase='consumed' AND m.phase IN ('loaded','ready')",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(user,"u"),soci::use(key,"k"),soci::use(m_config.owner_token,"t"),
        soci::use(generation,"g"),soci::use(epoch,"e"),soci::into(found);
    const bool result=sql.got_data();tx->commit();return result;
}

int RecoverPreparedMapTransfers(soci::session& sql,int world,int server,const std::string& token) {
    // Called inside owner replacement, after its process row has been locked.
    // Account serialization makes consumption and source recovery exclusive.
    std::vector<int> users;
    {soci::rowset<int> rows=(sql.prepare<<"SELECT user_id FROM app_world.map_sessions WHERE world_id=:w AND server_id=:s "
        "AND owner_token<>:t AND phase='transferring' ORDER BY user_id",soci::use(world,"w"),soci::use(server,"s"),soci::use(token,"t"));
     for(int user:rows)users.push_back(user);}
    int recovered=0;
    for(int user:users) {
        int locked=0;sql<<"SELECT \"dwUserID\" FROM app_global.\"TACCOUNT_PW\" WHERE \"dwUserID\"=:u FOR UPDATE",soci::use(user),soci::into(locked);
        long long transfer_id=0,generation=0,epoch=0;int character=0,key=0,channel=0;std::string hex;
        sql<<R"SQL(SELECT t.transfer_id,t.char_id,t.session_key,t.channel,t.source_connection,t.source_epoch,encode(t.body,'hex')
          FROM app_world.map_sessions m JOIN app_world.map_transfers t ON t.world_id=m.world_id AND t.char_id=m.char_id
            AND t.user_id=m.user_id AND t.session_key=m.session_key AND t.channel=m.channel AND t.source_server=m.server_id
            AND t.source_token=m.owner_token AND t.source_connection=m.connection_id AND t.source_epoch=m.authority_epoch
          JOIN app_world.map_checkpoints p ON p.world_id=m.world_id AND p.char_id=m.char_id AND p.user_id=m.user_id
            AND p.session_key=m.session_key AND p.server_id=m.server_id AND p.owner_token=m.owner_token
            AND p.connection_id=m.connection_id AND p.authority_epoch=m.authority_epoch
          JOIN app_global."TCURRENTUSER" u ON u."dwKEY"=m.session_key AND u."dwUserID"=m.user_id AND u."dwCharID"=m.char_id
            AND u."bGroupID"=m.world_id AND u."bChannel"=m.channel
          WHERE m.world_id=:w AND m.server_id=:s AND m.user_id=:u AND m.owner_token<>:t AND m.phase='transferring'
            AND t.phase='prepared' AND p.outcome='active' AND p.recovery_contract IN (1,2,3,4) AND p.revision<9223372036854775807
            AND app_world.map_checkpoint_matches(p) FOR UPDATE OF m,t,p,u)SQL",
            soci::use(world,"w"),soci::use(server,"s"),soci::use(user,"u"),soci::use(token,"t"),soci::into(transfer_id),soci::into(character),
            soci::into(key),soci::into(channel),soci::into(generation),soci::into(epoch),soci::into(hex);
        if(!sql.got_data())continue;
        std::vector<std::byte> body;body.reserve(hex.size()/2);
        for(std::size_t i=0;i<hex.size();i+=2)body.push_back(static_cast<std::byte>(std::stoul(hex.substr(i,2),nullptr,16)));
        auto graph=transfer::Decode(body);
        if(!graph||graph->db_load||graph->character.dwCharID!=static_cast<std::uint32_t>(character)||graph->key!=static_cast<std::uint32_t>(key))continue;
        MapSessionClaim claim{static_cast<std::uint32_t>(user),static_cast<std::uint32_t>(key),static_cast<std::uint32_t>(character),
            static_cast<std::uint8_t>(world),static_cast<std::uint8_t>(channel),static_cast<std::uint64_t>(generation)};
        claim.authority_epoch=epoch;
        // This is the locked durable prepared graph, not a caller-supplied
        // live snapshot. No cast can commit while this claim is transferring.
        // Rehydrate server-only metadata absent from the original SS packet.
        long long head=0;
        sql<<"SELECT COALESCE(max(cast_id),0) FROM app_world.accepted_skill_casts WHERE world_id=:w AND char_id=:c",
            soci::use(world,"w"),soci::use(character,"c"),soci::into(head);
        auto payload=std::make_shared<CharacterPayload>();payload->last_cast_id=static_cast<std::uint64_t>(head);
        graph->character.payload=std::move(payload);
        PostgreSQLMapService::WriteCore(sql,claim,graph->character,1);
        sql<<R"SQL(UPDATE app_world.map_checkpoints p SET recovery_contract=2,skill_state=NULL,maintain_state=NULL,revision=p.revision+1,fingerprint=t.core_fingerprint,
          core_state=app_world.map_core_state(p.world_id,p.char_id),transfer_body=t.body,transfer_hash=t.body_sha256,
          character_manifest=t.character_manifest,routing_manifest=t.routing_manifest,actor_manifest=t.actor_manifest,
          outcome='recovered',recovered_at=clock_timestamp(),saved_at=clock_timestamp()
          FROM app_world.map_transfers t WHERE t.transfer_id=:id AND p.world_id=t.world_id AND p.char_id=t.char_id)SQL",soci::use(transfer_id);
        sql<<"UPDATE app_world.map_transfers SET phase='cancelled',settled_at=clock_timestamp() WHERE transfer_id=:id",soci::use(transfer_id);
        sql<<"UPDATE app_global.\"TLOG\" SET \"timeLOGOUT\"=CURRENT_TIMESTAMP WHERE \"dwKEY\"=:k",soci::use(key);
        sql<<"DELETE FROM app_world.map_sessions WHERE world_id=:w AND char_id=:c",soci::use(world,"w"),soci::use(character,"c");
        sql<<"DELETE FROM app_global.\"TCURRENTUSER\" WHERE \"dwKEY\"=:k AND \"dwUserID\"=:u",soci::use(key,"k"),soci::use(user,"u");
        ++recovered;
    }
    return recovered;
}
}
