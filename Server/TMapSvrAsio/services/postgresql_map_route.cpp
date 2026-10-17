#include "postgresql_map_service.h"
#include "postgresql_map_owner.h"
#include <boost/asio/ip/address_v4.hpp>
#include <algorithm>
#include <bit>
#include <cmath>
#include <map>
#include <stdexcept>
namespace tmapsvr {
int PostgreSQLMapService::CellOwner(soci::session& sql,const MapSessionClaim& c,std::uint16_t map,float x,float z) const {
    if(c.group!=m_config.world||x<0||z<0)return 0;
    CheckNativeCellRelease(sql);
    const auto cell=NativeCellAt(x,z);if(!cell)return 0;
    const auto unit=static_cast<std::uint16_t>(((*cell&65535)/16)|(((*cell>>16)/16)<<8));
    return m_cells.Server(map,unit,c.channel).value_or(0);
}
bool PostgreSQLMapService::OwnsCell(const MapSessionClaim& claim,std::uint16_t map,float x,float z) {
    if(x<0||z<0)return false; // preserve native client coordinate admission
    const auto key=NativeMainCellAt(x,z);if(!key)return false;
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token,true);
    std::string phase;if(!LockAccount(sql,claim))return false;
    if(claim.role==MapSessionRole::Replica){if(!LockReplica(sql,claim,phase)||phase!="ready")return false;}
    else if(!LockClaim(sql,claim,phase))return false;
    CheckCatalogs(sql);CheckNativeCellRelease(sql);
    const auto* cell=FindNativeCell(claim.channel,map,*key,true);
    const bool owns=cell&&cell->IsMain(claim.channel);
    tx->commit();return owns;
}
std::optional<std::vector<std::uint8_t>> PostgreSQLMapService::MovementServers(const MapSessionClaim& c,std::uint16_t map,float x,float z) {
    if(c.role!=MapSessionRole::Primary||c.channel>=NativeCellChannels||x<0||z<0)return {};
    const auto key=NativeCellAt(x,z);if(!key)return {};
    auto lease=m_pool.Acquire();auto& sql=*lease;auto tx=BeginMapTransaction(sql,m_config.world,m_config.server,m_config.owner_token);
    std::string phase;if(!LockAccount(sql,c)||!LockClaim(sql,c,phase)||phase!="ready")return {};
    CheckCatalogs(sql);CheckNativeCellRelease(sql);
    const auto* cell=FindNativeCell(c.channel,map,*key,false);
    std::vector<std::uint8_t> ids;
    // OnMove sends a real empty CHECKCONNECT set when the new cell is absent,
    // disabled or extended. That is distinct from an invalid/unauthorized call.
    if(cell&&cell->IsEnabled(c.channel)&&cell->IsMain(c.channel)) {
        for(const auto& direction:cell->servers)if(direction[c.channel])ids.push_back(direction[c.channel]);
        ids.push_back(m_config.server);
        std::sort(ids.begin(),ids.end());ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
    }
    tx->commit();return ids;
}
std::optional<std::vector<std::uint8_t>> PostgreSQLMapService::NeighborServersLocked(soci::session& sql,const MapSessionClaim& claim,std::uint16_t map,float x,float z) const {
    CheckNativeCellRelease(sql);
    if(x<0||z<0)return {};
    const auto key=NativeCellAt(x,z);if(!key)return {};
    const auto* cell=FindNativeCell(claim.channel,map,*key,false);
    if(!cell||!cell->IsEnabled(claim.channel)||!cell->IsMain(claim.channel))return {};
    std::vector<std::uint8_t> out;
    for(const auto& direction:cell->servers)if(direction[claim.channel])out.push_back(direction[claim.channel]);
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
