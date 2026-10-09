#include "postgresql_map_service.h"
#include "postgresql_map_owner.h"
#include <boost/asio/ip/address_v4.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <map>
#include <stdexcept>
namespace tmapsvr {
namespace {
int CellServer(soci::session& sql,const MapSessionClaim& claim,std::uint16_t map,int ux,int uz) {
    if(ux<0||uz<0||ux>255||uz>255)return 0;
    const int unit=std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(uz*256+ux));
    const int map_id=std::bit_cast<std::int16_t>(map),world=claim.group,channel=claim.channel;
    int count=0,server=0;
    sql<<"SELECT count(DISTINCT s.\"bServerID\"),COALESCE(min(s.\"bServerID\"),0) FROM app_world.routing_worlds w "
         "JOIN route_compat.\"TCHANNELCHART\" c ON c.\"bGroupID\"=w.source_group "
         "JOIN route_compat.\"TSVRCHART\" s ON s.\"bGroup\"=c.\"bGroupID\" AND s.\"bChannel\"=c.\"bPhyChannel\" "
         "AND s.\"wMapID\"=c.\"wMapID\" AND s.\"wUnitID\"=c.\"wUnitID\" "
         "WHERE w.group_id=:w AND c.\"bLogChannel\"=:ch AND c.\"wMapID\"=:m AND c.\"wUnitID\"=:u",
        soci::use(world,"w"),soci::use(channel,"ch"),soci::use(map_id,"m"),soci::use(unit,"u"),soci::into(count),soci::into(server);
    if(count>1)throw std::runtime_error("Ambiguous cell ownership");
    return count==1?server:0;
}
}
int PostgreSQLMapService::CellOwner(soci::session& sql,const MapSessionClaim& c,std::uint16_t map,float x,float z) const {
    if(!std::isfinite(x)||!std::isfinite(z)||x<0||z<0||x>=65536||z>=65536)return 0;
    return CellServer(sql,c,map,int(x/1024),int(z/1024));
}
bool PostgreSQLMapService::OwnsCell(const MapSessionClaim& claim,std::uint16_t map,float x,float z) {
    if(!std::isfinite(x)||!std::isfinite(z)||x<0||z<0||x>=262144||z>=262144)return false;
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token,true);
    std::string phase;if(!LockAccount(sql,claim))return false;
    if(claim.role==MapSessionRole::Replica){if(!LockReplica(sql,claim,phase)||phase!="ready")return false;}
    else if(!LockClaim(sql,claim,phase))return false;
    CheckCatalogs(sql);const int server=CellServer(sql,claim,map,int(x/1024),int(z/1024));
    const bool owns=server!=0&&server==m_config.server;
    tx->commit();return owns;
}
std::optional<std::vector<std::uint8_t>> PostgreSQLMapService::MovementServers(const MapSessionClaim& c,std::uint16_t map,float x,float z) {
    if(c.role!=MapSessionRole::Primary)return {};
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||phase!="ready")return {};
    CheckCatalogs(sql);const auto owner=CellOwner(sql,c,map,x,z);if(!owner)return {};
    std::vector<std::uint8_t> ids;
    if(owner==m_config.server){auto neighbors=NeighborServersLocked(sql,c,map,x,z);if(!neighbors)return {};
        ids=std::move(*neighbors);ids.push_back(m_config.server);}
    tx->commit();return ids;
}
std::optional<std::vector<std::uint8_t>> PostgreSQLMapService::NeighborServersLocked(soci::session& sql,const MapSessionClaim& claim,std::uint16_t map,float x,float z) const {
    if(!std::isfinite(x)||!std::isfinite(z)||x<0||z<0||x>=65536||z>=65536||
       CellServer(sql,claim,map,int(x/1024),int(z/1024))!=m_config.server)return {};
    const int ux=int(x)/1024,uz=int(z)/1024,cx=int(x)%1024/64,cz=int(z)%1024/64;
    std::vector<std::uint8_t> out;
    for(int dz=-1;dz<=1;++dz)for(int dx=-1;dx<=1;++dx){
        if((!dx&&!dz)||(dx<0&&cx!=0)||(dx>0&&cx!=15)||(dz<0&&cz!=0)||(dz>0&&cz!=15))continue;
        const int server=CellServer(sql,claim,map,ux+dx,uz+dz);
        if(server!=0&&server!=m_config.server)out.push_back(static_cast<std::uint8_t>(server));
    }
    std::sort(out.begin(),out.end());out.erase(std::unique(out.begin(),out.end()),out.end());return out;
}
std::optional<std::vector<std::uint8_t>> PostgreSQLMapService::NeighborServers(const MapSessionClaim& claim,std::uint16_t map,float x,float z){
    if(claim.role!=MapSessionRole::Primary)return {};
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token,true);
    std::string phase;if(!LockAccount(sql,claim)||!LockClaim(sql,claim,phase))return {};
    CheckCatalogs(sql);auto out=NeighborServersLocked(sql,claim,map,x,z);tx->commit();return out;
}
std::vector<ServerRoute> PostgreSQLMapService::Resolve(std::uint8_t,const std::vector<std::uint8_t>&){
    throw std::runtime_error("Native route lookup requires connection claim");
}
std::vector<ServerRoute> PostgreSQLMapService::ResolveAuthorized(const MapSessionClaim& claim,const std::vector<std::uint8_t>& ids){
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;if(!LockAccount(sql,claim)||!LockClaim(sql,claim,phase))return {};
    CheckCatalogs(sql);const int world=claim.group;
    struct Endpoint {int machine{},port{};};std::map<int,Endpoint> endpoints;
    for(int id:ids){int machine=0,port=0;sql<<"SELECT \"bMachineID\",\"wPort\" FROM app_global.\"TSERVER\" WHERE \"bGroupID\"=:w AND \"bServerID\"=:s AND \"bType\"=4",
        soci::use(world,"w"),soci::use(id,"s"),soci::into(machine),soci::into(port);
        if(sql.got_data())endpoints.emplace(id,Endpoint{machine,port});}
    // Lock all machines in stable order before any rotation.
    std::map<int,int> rotations;
    for(const auto& [id,e]:endpoints)rotations.emplace(e.machine,0);
    for(auto& [machine,rotation]:rotations){sql<<"SELECT \"bRouteID\" FROM app_global.\"TMACHINE\" WHERE \"bMachineID\"=:m FOR UPDATE",
        soci::use(machine,"m"),soci::into(rotation);if(!sql.got_data())throw std::runtime_error("Route machine missing");}
    std::vector<ServerRoute> out;
    for(const auto& [id,e]:endpoints){
        std::vector<std::string> ips;
        {soci::rowset<std::string> rows=(sql.prepare<<"SELECT \"szIPAddr\" FROM app_global.\"TIPADDR\" WHERE \"bMachineID\"=:m AND \"bActive\"=1 ORDER BY \"szIPAddr\"",soci::use(e.machine,"m"));for(const auto& ip:rows)ips.push_back(ip);}
        if(ips.empty()||ips.size()>255||e.port<1||e.port>65535)continue;
        const int next=rotations[e.machine]%static_cast<int>(ips.size())+1;
        boost::system::error_code ec;const auto address=boost::asio::ip::make_address_v4(ips[next-1],ec);if(ec)continue;
        const auto bytes=address.to_bytes();const std::uint32_t wire_ip=bytes[0]|(std::uint32_t(bytes[1])<<8)|(std::uint32_t(bytes[2])<<16)|(std::uint32_t(bytes[3])<<24);
        out.push_back({wire_ip,static_cast<std::uint16_t>(e.port),static_cast<std::uint8_t>(id)});rotations[e.machine]=next;
        sql<<"UPDATE app_global.\"TMACHINE\" SET \"bRouteID\"=:r WHERE \"bMachineID\"=:m",soci::use(next,"r"),soci::use(e.machine,"m");
    }
    tx->commit();return out;
}
}
