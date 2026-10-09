#include "postgresql_map_service.h"
#include "postgresql_map_owner.h"
#include <boost/asio/ip/address_v4.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace tmapsvr {
bool PostgreSQLMapService::AuthorizeReplicas(const MapSessionClaim& c,std::uint16_t map,float x,float z,const std::vector<ServerRoute>& routes) {
    if(c.role!=MapSessionRole::Primary||routes.empty()||routes.size()>8)return false;
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;
    if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||(phase!="loaded"&&phase!="ready"))return false;
    CheckCatalogs(sql);
    auto expected=NeighborServersLocked(sql,c,map,x,z);if(!expected)return false;
    std::vector<std::uint8_t> ids;for(const auto& r:routes)ids.push_back(r.server_id);
    std::sort(ids.begin(),ids.end());if(std::adjacent_find(ids.begin(),ids.end())!=ids.end()||!std::includes(expected->begin(),expected->end(),ids.begin(),ids.end()))return false;
    const int world=c.group,character=c.char_id,primary=m_config.server,map_id=map;
    const long long generation=c.connection_id,epoch=c.authority_epoch;const double px=x,pz=z;
    for(const auto& r:routes) {
        if(!r.server_id||!r.ip_addr||!r.port)return false;
        const int target=r.server_id,port=r.port;const long long ip=r.ip_addr;
        const auto ip_text=boost::asio::ip::address_v4({static_cast<unsigned char>(r.ip_addr),static_cast<unsigned char>(r.ip_addr>>8),static_cast<unsigned char>(r.ip_addr>>16),static_cast<unsigned char>(r.ip_addr>>24)}).to_string();
        // Do not take another owner's row lock after the account lock: target
        // replacement owns that row before it cleans its sessions. A changed
        // token instead invalidates this grant at consumption.
        std::string target_token;
        sql<<"SELECT o.owner_token FROM app_world.map_runtime_owner o JOIN app_global.\"TSERVER\" s "
             "ON s.\"bGroupID\"=o.world_id AND s.\"bServerID\"=o.server_id AND s.\"bType\"=4 "
             "JOIN app_global.\"TIPADDR\" a ON a.\"bMachineID\"=s.\"bMachineID\" "
             "WHERE o.world_id=:w AND o.server_id=:s AND s.\"wPort\"=:p AND a.\"szIPAddr\"=:ip AND a.\"bActive\"=1",
            soci::use(world,"w"),soci::use(target,"s"),soci::use(port,"p"),soci::use(ip_text,"ip"),soci::into(target_token);
        if(!sql.got_data())return false;
        sql<<R"SQL(INSERT INTO app_world.map_replicas(world_id,char_id,primary_server,primary_token,primary_connection,primary_epoch,
          target_server,target_token,endpoint_ip,endpoint_port,map_id,position_x,position_z,routing_manifest,character_manifest,actor_manifest,phase)
          VALUES(:w,:c,:s,:t,:g,:epoch,:target,:token,:ip,:port,:map,:x,:z,:r,:cm,:a,'granted')
          ON CONFLICT(world_id,char_id,target_server) DO UPDATE SET
          primary_server=EXCLUDED.primary_server,primary_token=EXCLUDED.primary_token,primary_connection=EXCLUDED.primary_connection,primary_epoch=EXCLUDED.primary_epoch,
          target_token=EXCLUDED.target_token,endpoint_ip=EXCLUDED.endpoint_ip,endpoint_port=EXCLUDED.endpoint_port,
          map_id=EXCLUDED.map_id,position_x=EXCLUDED.position_x,position_z=EXCLUDED.position_z,
          routing_manifest=EXCLUDED.routing_manifest,character_manifest=EXCLUDED.character_manifest,actor_manifest=EXCLUDED.actor_manifest,
          phase='granted',connection_id=NULL,granted_at=clock_timestamp(),expires_at=clock_timestamp()+interval '60 seconds',updated_at=clock_timestamp()
          WHERE map_replicas.phase='retired' OR (map_replicas.phase='granted' AND map_replicas.expires_at<=clock_timestamp()))SQL",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(primary,"s"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(epoch,"epoch"),
            soci::use(target,"target"),soci::use(target_token,"token"),soci::use(ip,"ip"),soci::use(port,"port"),soci::use(map_id,"map"),
            soci::use(px,"x"),soci::use(pz,"z"),soci::use(m_config.routing_manifest,"r"),soci::use(m_config.character_manifest,"cm"),soci::use(m_config.actor_manifest,"a");
        int n=0;
        sql<<"SELECT 1 FROM app_world.map_replicas WHERE world_id=:w AND char_id=:c AND target_server=:target "
             "AND primary_server=:s AND primary_token=:t AND primary_connection=:g AND primary_epoch=:epoch AND target_token=:token "
             "AND endpoint_ip=:ip AND endpoint_port=:port AND map_id=:map AND position_x=:x AND position_z=:z "
             "AND routing_manifest=:r AND character_manifest=:cm AND actor_manifest=:a AND phase='granted' AND expires_at>clock_timestamp()",
            soci::use(world,"w"),soci::use(character,"c"),soci::use(primary,"s"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(epoch,"epoch"),
            soci::use(target,"target"),soci::use(target_token,"token"),soci::use(ip,"ip"),soci::use(port,"port"),soci::use(map_id,"map"),
            soci::use(px,"x"),soci::use(pz,"z"),soci::use(m_config.routing_manifest,"r"),soci::use(m_config.character_manifest,"cm"),soci::use(m_config.actor_manifest,"a"),soci::into(n);
        if(!sql.got_data())return false;
    }
    tx->commit();return true;
}
std::optional<MapSessionInfo> PostgreSQLMapService::ClaimReplica(soci::session& sql,const MapSessionClaim& c) const {
    const int world=c.group,server=m_config.server,channel=c.channel,port=c.endpoint_port;
    const long long user=c.user_id,key=c.key,character=c.char_id,generation=c.connection_id,ip=c.endpoint_ip;
    int n=0;
    sql<<R"SQL(UPDATE app_world.map_replicas r SET phase='claimed',connection_id=:g,updated_at=clock_timestamp()
      FROM app_world.map_sessions m,app_world.map_runtime_owner p
      WHERE r.world_id=:w AND r.char_id=:c AND r.target_server=:s AND r.target_token=:t
      AND r.phase='granted' AND r.expires_at>clock_timestamp() AND r.endpoint_ip=:ip AND r.endpoint_port=:port
      AND r.routing_manifest=:rm AND r.character_manifest=:cm AND r.actor_manifest=:am
      AND m.world_id=r.world_id AND m.char_id=r.char_id AND m.server_id=r.primary_server
      AND m.owner_token=r.primary_token AND m.connection_id=r.primary_connection AND m.authority_epoch=r.primary_epoch AND m.phase IN ('loaded','ready')
      AND m.user_id=:u AND m.session_key=:k AND m.channel=:ch AND m.routing_manifest=r.routing_manifest AND m.character_manifest=r.character_manifest
      AND p.world_id=m.world_id AND p.server_id=m.server_id AND p.owner_token=m.owner_token RETURNING 1)SQL",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(server,"s"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),
        soci::use(ip,"ip"),soci::use(port,"port"),soci::use(m_config.routing_manifest,"rm"),soci::use(m_config.character_manifest,"cm"),soci::use(m_config.actor_manifest,"am"),
        soci::use(user,"u"),soci::use(key,"k"),soci::use(channel,"ch"),soci::into(n);
    if(!sql.got_data())return {};
    MapSessionInfo out;out.dwUserID=c.user_id;out.dwKEY=c.key;out.dwCharID=c.char_id;out.bGroupID=c.group;out.bChannel=c.channel;out.role=MapSessionRole::Replica;return out;
}
bool PostgreSQLMapService::LockReplica(soci::session& sql,const MapSessionClaim& c,std::string& phase) const {
    if(c.role!=MapSessionRole::Replica)return false;
    const int world=c.group,server=m_config.server,channel=c.channel;
    const long long user=c.user_id,key=c.key,character=c.char_id,generation=c.connection_id;
    sql<<R"SQL(SELECT r.phase FROM app_world.map_replicas r JOIN app_world.map_sessions m ON m.world_id=r.world_id AND m.char_id=r.char_id
      JOIN app_world.map_runtime_owner p ON p.world_id=m.world_id AND p.server_id=m.server_id AND p.owner_token=m.owner_token
      JOIN app_global."TCURRENTUSER" u ON u."dwUserID"=m.user_id AND u."dwKEY"=m.session_key AND u."dwCharID"=m.char_id
        AND u."bGroupID"=m.world_id AND u."bChannel"=m.channel AND u."bLocked"=0
      WHERE r.world_id=:w AND r.char_id=:c AND r.target_server=:s AND r.target_token=:t AND r.connection_id=:g
      AND r.routing_manifest=:rm AND r.character_manifest=:cm AND r.actor_manifest=:am
      AND m.server_id=r.primary_server AND m.owner_token=r.primary_token AND m.connection_id=r.primary_connection AND m.authority_epoch=r.primary_epoch
      AND m.user_id=:u AND m.session_key=:k AND m.channel=:ch AND m.phase IN ('loaded','ready')
      AND r.phase IN ('claimed','loaded','ready') FOR UPDATE OF r)SQL",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(server,"s"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),
        soci::use(m_config.routing_manifest,"rm"),soci::use(m_config.character_manifest,"cm"),soci::use(m_config.actor_manifest,"am"),
        soci::use(user,"u"),soci::use(key,"k"),soci::use(channel,"ch"),soci::into(phase);
    return sql.got_data();
}
bool PostgreSQLMapService::LoadReplica(const MapSessionClaim& c,std::uint16_t map,float x,float z) {
    if(!std::isfinite(x)||!std::isfinite(z))return false;
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;if(!LockAccount(sql,c)||!LockReplica(sql,c,phase)||phase!="claimed")return false;
    CheckCatalogs(sql);const int world=c.group,character=c.char_id,server=m_config.server,map_id=map;
    const double px=x,pz=z;int n=0;
    sql<<"UPDATE app_world.map_replicas SET phase='loaded',updated_at=clock_timestamp() "
         "WHERE world_id=:w AND char_id=:c AND target_server=:s AND map_id=:m AND position_x=:x AND position_z=:z RETURNING 1",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(server,"s"),soci::use(map_id,"m"),soci::use(px,"x"),soci::use(pz,"z"),soci::into(n);
    if(!sql.got_data())return false;tx->commit();return true;
}
void PostgreSQLMapService::MarkReplicaReady(const MapSessionClaim& c) {
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;if(!LockAccount(sql,c)||!LockReplica(sql,c,phase)||phase!="loaded")throw std::runtime_error("Replica state not loaded");
    CheckCatalogs(sql);const int world=c.group,character=c.char_id,server=m_config.server;
    sql<<"UPDATE app_world.map_replicas SET phase='ready',updated_at=clock_timestamp() WHERE world_id=:w AND char_id=:c AND target_server=:s",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(server,"s");tx->commit();
}
void PostgreSQLMapService::ReleaseReplica(soci::session& sql,const MapSessionClaim& c) const {
    // Also used when claim commit raced client close before local role assignment.
    // No current-user, primary, checkpoint or core write is permitted here.
    const int world=c.group,server=m_config.server;const long long character=c.char_id,generation=c.connection_id,user=c.user_id,key=c.key;
    sql<<"UPDATE app_world.map_replicas r SET phase='retired',updated_at=clock_timestamp() FROM app_world.map_sessions m "
         "WHERE r.world_id=:w AND r.char_id=:c AND r.target_server=:s AND r.target_token=:t AND r.connection_id=:g "
         "AND m.world_id=r.world_id AND m.char_id=r.char_id AND m.user_id=:u AND m.session_key=:k",
        soci::use(world,"w"),soci::use(character,"c"),soci::use(server,"s"),soci::use(m_config.owner_token,"t"),soci::use(generation,"g"),soci::use(user,"u"),soci::use(key,"k");
}
}
