#include "postgresql_map_service.h"
#include "native_skill_state.h"
#include "postgresql_map_owner.h"
#include "native_wire_text.h"
#include <soci/soci.h>
#include <bit>
#include <stdexcept>
#include <cmath>

namespace tmapsvr {
namespace {
void ControlOwner(soci::session& sql,const PostgreSQLMapConfig& config) {
    const int world=config.world,server=config.server,node=(world<<8)|server,ns=0x344d4150;
    int live=0;
    sql<<"SELECT 1 FROM app_world.map_runtime_owner o WHERE o.world_id=:w AND o.server_id=:s AND o.owner_token=:t "
         "AND EXISTS(SELECT 1 FROM pg_locks l WHERE l.pid=o.backend_pid AND l.locktype='advisory' "
         "AND l.classid=CAST(:ns AS oid) AND l.objid=CAST(:node AS oid) AND l.objsubid=2 AND l.granted)",
        soci::use(world,"w"),soci::use(server,"s"),soci::use(config.owner_token,"t"),soci::use(ns,"ns"),soci::use(node,"node"),soci::into(live);
    if(!sql.got_data()||live!=1)throw std::runtime_error("Native Control requires the live Map process lease");
}
}
void PostgreSQLMapService::CheckControlOwner() {
    auto lease=m_pool.Acquire();auto tx=BeginMapTransaction(*lease,m_config.world,m_config.server,m_config.owner_token);
    ControlOwner(*lease,m_config);
    tx->commit();
}
bool PostgreSQLMapService::AuthorizeControlKick(const MapSessionClaim& c,const std::string& name) {
    auto lease=m_pool.Acquire();auto& sql=*lease;
    auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    ControlOwner(sql,m_config);
    std::string phase;if(!LockAccount(sql,c))return false;
    if(c.role==MapSessionRole::Primary){if(!LockClaim(sql,c,phase)||phase!="ready")return false;}
    else if(!LockReplica(sql,c,phase)||phase!="ready")return false;
    const auto decoded=native_wire_text::Decode(sql,name,m_config.client_text_encoding);
    const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id),user=std::bit_cast<std::int32_t>(c.user_id),key=std::bit_cast<std::int32_t>(c.key);
    int found=0;
    sql<<"SELECT 1 FROM app_world.\"TCHARTABLE\" c JOIN app_global.\"TCURRENTUSER\" u "
         "ON u.\"dwCharID\"=c.\"dwCharID\" AND u.\"dwUserID\"=c.\"dwUserID\" AND u.\"bGroupID\"=c.\"bWorldID\" "
         "WHERE c.\"bWorldID\"=:w AND c.\"dwCharID\"=:c AND c.\"dwUserID\"=:u AND c.\"bDelete\"=0 "
         "AND c.\"szNAME\"=:name AND u.\"dwKEY\"=:k AND u.\"bLocked\"=0 FOR SHARE OF c",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(user,"u"),soci::use(decoded,"name"),soci::use(key,"k"),soci::into(found);
    const bool allowed=sql.got_data()&&found==1;tx->commit();return allowed;
}
bool PostgreSQLMapService::ReadNativeOperator(soci::session& sql,std::uint8_t world_id,std::uint32_t character_id) {
    const int world=world_id,character=std::bit_cast<std::int32_t>(character_id);
    long long published=0,expected=0,operators=0;
    sql<<"SELECT count(*),COALESCE(max(operator_rows),0) FROM world_compat.operator_release WHERE world_id=:w AND status='verified'",
        soci::use(world),soci::into(published),soci::into(expected);
    sql<<"SELECT count(*) FROM app_world.\"TOPERATORTABLE\" WHERE \"bWorldID\"=:w",soci::use(world),soci::into(operators);
    if(published!=1||expected!=operators)throw std::runtime_error("Native operation requires the exact published source operator catalog");
    int op=0;
    sql<<"SELECT count(*) FROM app_world.\"TOPERATORTABLE\" WHERE \"bWorldID\"=:w AND \"dwOperatorID\"=:c",
        soci::use(world,"w"),soci::use(character,"c"),soci::into(op);
    return op==1;
}
bool PostgreSQLMapService::AuthorizeNativeMovementSpeed(const MapSessionClaim& c,float speed) {
    if(!std::isfinite(speed))return false;
    if(speed<=3.40f)return true; // source tests strict positive greater-than, not abs(speed)
    auto lease=m_pool.Acquire();auto& sql=*lease;
    auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;
    if(!LockAccount(sql,c))return false;
    if(c.role==MapSessionRole::Primary){if(!LockClaim(sql,c,phase)||phase!="ready")return false;}
    else if(!LockReplica(sql,c,phase)||phase!="ready")return false;
    const bool allowed=ReadNativeOperator(sql,c.group,c.char_id);
    tx->commit();return allowed;
}
std::optional<CommunicationAuthority> PostgreSQLMapService::AuthorizeCommunication(const MapSessionClaim& c,bool chat,std::uint32_t sender,std::uint8_t option) {
    // Only recipient chat delivery can use a partial replica. Its client may
    // never originate a TMS/chat request or receive a primary-only MW fanout.
    if(c.role!=MapSessionRole::Primary&&(chat||option!=1))return {};
    auto lease=m_pool.Acquire();auto& sql=*lease;
    auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;
    if(!LockAccount(sql,c))return {};
    if(c.role==MapSessionRole::Primary){if(!LockClaim(sql,c,phase)||phase!="ready")return {};}
    else if(!LockReplica(sql,c,phase)||phase!="ready")return {};
    const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id),key=std::bit_cast<std::int32_t>(c.key);
    soci::row row;
    sql<<"SELECT c.\"szNAME\" FROM app_world.\"TCHARTABLE\" c "
         "JOIN app_global.\"TCURRENTUSER\" u ON u.\"dwCharID\"=c.\"dwCharID\" AND u.\"dwUserID\"=c.\"dwUserID\" "
         "AND u.\"bGroupID\"=c.\"bWorldID\" WHERE c.\"bWorldID\"=:w AND c.\"dwCharID\"=:c "
         "AND u.\"dwKEY\"=:k AND u.\"bLocked\"=0 AND c.\"bDelete\"=0 "
         "AND EXISTS(SELECT 1 FROM app_world.map_sessions m JOIN app_world.map_runtime_owner o "
         "ON o.world_id=m.world_id AND o.server_id=m.server_id AND o.owner_token=m.owner_token "
         "WHERE m.world_id=:w AND m.char_id=:c AND m.session_key=:k AND m.phase='ready' "
         "AND EXISTS(SELECT 1 FROM pg_locks l WHERE l.pid=o.backend_pid AND l.locktype='advisory' "
         "AND l.classid=CAST(877478224 AS oid) AND l.objid=CAST((m.world_id::integer<<8)|m.server_id::integer AS oid) "
         "AND l.objsubid=2 AND l.granted)) FOR SHARE OF c",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(key,"k"),soci::into(row);
    if(!sql.got_data())return {};
    CommunicationAuthority out;
    if(option) {
        const int source=std::bit_cast<std::int32_t>(sender),mask=option;int blocked=0;
        sql<<"SELECT count(*) FROM app_world.\"TPROTECTEDTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c "
             "AND \"dwProtected\"=:source AND (\"bOption\"::integer & :mask)<>0",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(source,"source"),soci::use(mask,"mask"),soci::into(blocked);
        out.protected_sender=blocked!=0;
    }
    if(chat) {
        out.is_operator=ReadNativeOperator(sql,c.group,c.char_id);
        const auto name=native_wire_text::Hex(native_wire_text::Encode(sql,row.get<std::string>(0),m_config.client_text_encoding));
        sql<<"SELECT COALESCE((SELECT ends_at FROM app_world.chat_bans WHERE world_id=:w AND name_bytes=decode(:name,'hex')),0)",
            soci::use(world,"w"),soci::use(name,"name"),soci::into(out.chat_ban_time);
    }
    tx->commit();return out;
}
std::optional<PartyAuthority> PostgreSQLMapService::AuthorizeParty(const MapSessionClaim& c,const CharSnapshot& s,
    std::uint32_t inviter,bool invitation) {
    if(c.char_id!=s.dwCharID||s.persistence_uncertain||!s.cluster.hydrated||
       (invitation&&c.role!=MapSessionRole::Primary))return {};
    auto lease=m_pool.Acquire();auto& sql=*lease;
    auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token,false,true);
    ControlOwner(sql,m_config);
    CheckCatalogs(sql);
    std::string phase;
    if(!LockAccount(sql,c))return {};
    if(c.role==MapSessionRole::Primary) {
        if(!LockClaim(sql,c,phase)||(phase!="ready"&&(invitation||phase!="loaded")))return {};
    }else if(!LockReplica(sql,c,phase)||phase!="ready")return {};
    const int world=c.group,character=std::bit_cast<std::int32_t>(c.char_id),
        user=std::bit_cast<std::int32_t>(c.user_id),key=std::bit_cast<std::int32_t>(c.key);
    std::string name;
    sql<<"SELECT c.\"szNAME\" FROM app_world.\"TCHARTABLE\" c JOIN app_global.\"TCURRENTUSER\" u "
         "ON u.\"dwCharID\"=c.\"dwCharID\" AND u.\"dwUserID\"=c.\"dwUserID\" AND u.\"bGroupID\"=c.\"bWorldID\" "
         "WHERE c.\"bWorldID\"=:w AND c.\"dwCharID\"=:c AND c.\"dwUserID\"=:u AND c.\"bDelete\"=0 "
         "AND u.\"dwKEY\"=:k AND u.\"bLocked\"=0 "
         "AND EXISTS(SELECT 1 FROM app_world.map_sessions m JOIN app_world.map_runtime_owner p "
         "ON p.world_id=m.world_id AND p.server_id=m.server_id AND p.owner_token=m.owner_token "
         "WHERE m.world_id=:w AND m.char_id=:c AND m.session_key=:k AND m.phase IN ('loaded','ready') "
         "AND EXISTS(SELECT 1 FROM pg_locks l WHERE l.pid=p.backend_pid AND l.locktype='advisory' "
         "AND l.classid=CAST(877478224 AS oid) AND l.objid=CAST((m.world_id::integer<<8)|m.server_id::integer AS oid) "
         "AND l.objsubid=2 AND l.granted)) FOR SHARE OF c",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(user,"u"),soci::use(key,"k"),soci::into(name);
    if(!sql.got_data())return {};
    PartyAuthority out;out.name=native_wire_text::Encode(sql,name,m_config.client_text_encoding);
    if(out.name!=s.szNAME)return {};
    if(invitation) {
        if(!s.payload||!s.payload->effects)throw std::runtime_error("Party invitation requires hydrated maintained effects");
        out.action_blocked=s.bDead!=0;
        // CTObjBase::IsActionBlock examines every maintained skill's template,
        // including entries awaiting the normal simulation expiry producer.
        for(const auto& effect:*s.payload->effects) {
            for(const auto& d:NativePlayerSkillData(*s.payload,effect.skill))
                if(d.bType==SDT_TRANS||(d.bType==SDT_STATUS&&(d.bExec==SDT_STATUS_BLOCK||d.bExec==SDT_STATUS_HOLD)))
                    out.action_blocked=true;
        }
        const int source=std::bit_cast<std::int32_t>(inviter);int blocked=0;
        sql<<"SELECT count(*) FROM app_world.\"TPROTECTEDTABLE\" WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c "
             "AND \"dwProtected\"=:source AND (\"bOption\"::integer & 2)<>0",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(source,"source"),soci::into(blocked);
        out.protected_inviter=blocked!=0;
    }
    tx->commit();return out;
}
bool PostgreSQLMapService::ConfirmChatBan(const MapSessionClaim& c,const std::string& name,std::int64_t end,bool issuer) {
    if(c.role!=MapSessionRole::Primary||end<0)return false;
    auto lease=m_pool.Acquire();auto& sql=*lease;
    auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token,false,issuer);
    std::string phase;
    if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||(phase!="loaded"&&phase!="ready"))return false;
    const int world=c.group,key=std::bit_cast<std::int32_t>(c.key);const auto bytes=native_wire_text::Hex(name);
    const auto decoded=native_wire_text::Decode(sql,name,m_config.client_text_encoding);
    int confirmed=0;
    sql<<"SELECT 1 FROM app_world.\"TCHARTABLE\" c WHERE c.\"bWorldID\"=:w AND c.\"bDelete\"=0 "
         "AND c.\"szNAME\"=:decoded AND EXISTS(SELECT 1 FROM app_global.\"TCURRENTUSER\" "
         "WHERE \"dwKEY\"=:key AND \"bLocked\"=0) AND COALESCE((SELECT ends_at FROM app_world.chat_bans "
         "WHERE world_id=:w AND name_bytes=decode(:name,'hex')),0)=:ends",
        soci::use(world,"w"),soci::use(key,"key"),soci::use(decoded,"decoded"),soci::use(bytes,"name"),soci::use(end,"ends"),soci::into(confirmed);
    bool matches=sql.got_data()&&confirmed==1;
    if(matches&&issuer) {
        if(phase!="ready")return false;
        const int character=std::bit_cast<std::int32_t>(c.char_id),server=m_config.server;
        const long long generation=c.connection_id,epoch=c.authority_epoch;int receipt=0;
        sql<<"SELECT 1 WHERE EXISTS(SELECT 1 FROM app_world.chat_ban_mutations b "
             "JOIN app_world.world_runtime_owner o ON o.world_id=b.world_id AND o.owner_token=b.owner_token "
             "WHERE b.world_id=:w AND b.issuer_id=:c AND b.session_key=:key AND b.source_server=:server "
             "AND b.source_map_token=:token AND b.source_connection_id=:generation AND b.source_authority_epoch=:epoch "
             "AND b.name_bytes=decode(:name,'hex') AND b.ends_at=:ends) "
             "AND EXISTS(SELECT 1 FROM world_compat.operator_release WHERE world_id=:w AND status='verified') "
             "AND EXISTS(SELECT 1 FROM app_world.\"TOPERATORTABLE\" WHERE \"bWorldID\"=:w AND \"dwOperatorID\"=:c)",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(key,"key"),soci::use(server,"server"),
            soci::use(m_config.owner_token,"token"),soci::use(generation,"generation"),soci::use(epoch,"epoch"),
            soci::use(bytes,"name"),soci::use(end,"ends"),soci::into(receipt);
        matches=sql.got_data()&&receipt==1;
    }
    tx->commit();return matches;
}
std::optional<std::string> PostgreSQLMapService::NoticeOperatorName(const MapSessionClaim& c) {
    if(c.role!=MapSessionRole::Primary)return {};
    auto lease=m_pool.Acquire();auto& sql=*lease;
    auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;
    if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||phase!="ready")return {};
    int compatible=0;
    sql<<"SELECT count(*) FROM game_compat.server_message_release r JOIN game_compat.catalog_release m "
         "ON m.manifest_sha256=r.map_manifest AND m.status='verified' "
         "JOIN character_compat.catalog_release ch ON ch.manifest_sha256=r.character_manifest AND ch.status='verified' "
         "JOIN actor_compat.catalog_release a ON a.manifest_sha256=r.actor_manifest AND a.status='verified' WHERE r.status='verified'",
        soci::into(compatible);
    if(compatible!=1)throw std::runtime_error("Notice requires the original published server messages");
    std::string raw;
    sql<<"SELECT encode(message_bytes,'hex') FROM game_compat.server_messages WHERE \"dwID\"=6",soci::into(raw);
    if(!sql.got_data())throw std::runtime_error("Original operator display name is absent");
    auto name=native_wire_text::Unhex(raw);
    if(name.size()>256)throw std::runtime_error("Original operator display name exceeds its source width");
    // Validate the explicitly configured lossless client profile, but keep the
    // exact published059 bytes in the source CS_CHAT_ACK caption.
    (void)native_wire_text::Decode(sql,name,m_config.client_text_encoding);
    tx->commit();return name;
}
}
