#include "soci_map_server_locator.h"
#include "postgresql_login_owner.h"
#include "fourstory/db/session_pool.h"
#include <boost/asio/ip/address_v4.hpp>
#include <soci/soci.h>
#ifdef FOURSTORY_HAS_POSTGRESQL
#include <soci/postgresql/soci-postgresql.h>
#endif
#include <spdlog/spdlog.h>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace tloginsvr::services {
namespace {
bool Retryable(const soci::soci_error& error) {
#ifdef FOURSTORY_HAS_POSTGRESQL
    auto* pg=dynamic_cast<const soci::postgresql_soci_error*>(&error);
    return pg && (pg->sqlstate()=="40001" || pg->sqlstate()=="40P01");
#else
    (void)error; return false;
#endif
}
std::optional<int> Unit(double x,double z) {
    if (!std::isfinite(x) || !std::isfinite(z)) return {};
    // SQL Server CAST(real AS smallint) truncates; PostgreSQL rounds.
    const auto ux=std::trunc(x/1024.0), uz=std::trunc(z/1024.0);
    if (ux < -32768 || ux > 32767 || uz < -32768 || uz > 32767) return {};
    const auto unit=uz*256+ux;
    if (unit < -32768 || unit > 32767) return {};
    return static_cast<int>(unit);
}
std::optional<int> Route(soci::session& sql,int group,int channel,int map,double x,double z) {
    const auto unit=Unit(x,z); if (!unit) return {};
    int count=0,server=0;
    sql << "SELECT count(DISTINCT s.\"bServerID\"),coalesce(min(s.\"bServerID\"),0) "
           "FROM route_compat.\"TSVRCHART\" s JOIN route_compat.\"TCHANNELCHART\" c "
           "ON s.\"bGroup\"=c.\"bGroupID\" AND s.\"bChannel\"=c.\"bPhyChannel\" "
           "AND s.\"wMapID\"=c.\"wMapID\" AND s.\"wUnitID\"=c.\"wUnitID\" "
           "WHERE c.\"bGroupID\"=:g AND c.\"bLogChannel\"=:ch AND c.\"wMapID\"=:m AND c.\"wUnitID\"=:u",
        soci::use(group,"g"),soci::use(channel,"ch"),soci::use(map,"m"),soci::use(*unit,"u"),soci::into(count),soci::into(server);
    if (count>1) throw std::runtime_error("Ambiguous routing ownership");
    return count==1 ? std::optional<int>(server) : std::nullopt;
}
std::optional<MapEndpoint> Endpoint(const std::string& ip,int port,int server) {
    boost::system::error_code ec;
    const auto address=boost::asio::ip::make_address_v4(ip,ec);
    if (ec || port<1 || port>65535 || server<0 || server>255) return {};
    return MapEndpoint{address.to_bytes(),static_cast<std::uint16_t>(port),static_cast<std::uint8_t>(server)};
}
}

StartResponse SociMapServerLocator::StartAuthorized(const StartRequest& request) {
    if (m_pool.GetBackend()!=fourstory::db::Backend::PostgreSQL)
        return IMapServerLocator::StartAuthorized(request);
    if (m_owner_token.empty() || m_routing_manifest.size()!=64 || m_routing_manifest.find_first_not_of("0123456789abcdef")!=std::string::npos)
        return {StartStatus::Internal,{}};
    if (request.user_id<=0 || request.char_id<=0 || !request.session_key)
        return {StartStatus::NoServer,{}};
    for (int attempt=0;attempt<3;++attempt) {
        try { return StartNative(request); }
        catch (const soci::soci_error& ex) { if (attempt<2 && Retryable(ex)) continue; }
        catch (...) {}
        spdlog::error("native map handoff transaction failed");
        break;
    }
    return {StartStatus::Internal,{}};
}

StartResponse SociMapServerLocator::StartNative(const StartRequest& r) {
    auto lease=m_pool.Acquire(); auto& sql=*lease;
    auto tx=BeginLoginTransaction(m_pool,sql,m_owner_token,true);
    const int user=r.user_id,world=r.group_id,channel=r.channel,cid=r.char_id;
    const long long key=r.session_key;
    int n=0;
    // Lock order: owner fence -> account -> current session -> character -> machine.
    // Touching the account forces retries for an older repeatable-read snapshot.
    sql << "UPDATE app_global.\"TACCOUNT_PW\" SET \"dLastLogin\"=\"dLastLogin\" WHERE \"dwUserID\"=:u RETURNING 1",
        soci::use(user,"u"),soci::into(n);
    if (!sql.got_data()) return {StartStatus::NoServer,{}};
    int current_char=0,current_world=0,current_channel=0;
    sql << "SELECT s.\"dwCharID\",s.\"bGroupID\",s.\"bChannel\" FROM app_global.\"TCURRENTUSER\" s JOIN app_global.\"TUSERINFOTABLE\" i USING (\"dwUserID\") "
           "WHERE s.\"dwUserID\"=:u AND s.\"dwKEY\"=:k AND s.\"bLocked\"=0 AND i.\"bAgreement\"=1 FOR UPDATE OF s",
        soci::use(user,"u"),soci::use(key,"k"),soci::into(current_char),soci::into(current_world),soci::into(current_channel);
    if (!sql.got_data()) return {StartStatus::NoServer,{}};
    // A retry after an uncertain ACK reuses exactly the committed reservation.
    // It never rotates the address again or extends the acceptance deadline.
    if (current_char!=0) {
        if (current_char!=cid || current_world!=world || current_channel!=channel) return {StartStatus::NoServer,{}};
        std::string ip; int port=0,server=0;
        sql << "SELECT host(server_ip),server_port,server_id FROM app_global.map_handoff "
               "WHERE session_key=:k AND user_id=:u AND world_id=:w AND char_id=:c AND channel=:ch "
               "AND expires_at>clock_timestamp()",
            soci::use(key,"k"),soci::use(user,"u"),soci::use(world,"w"),soci::use(cid,"c"),soci::use(channel,"ch"),
            soci::into(ip),soci::into(port),soci::into(server);
        if (!sql.got_data()) return {StartStatus::NoServer,{}};
        auto ep=Endpoint(ip,port,server); if (!ep) return {StartStatus::Internal,{}};
        tx->commit();return {StartStatus::Success,ep};
    }
    int source_group=0;
    sql << "SELECT r.source_group FROM app_world.routing_worlds r "
           "JOIN app_global.\"TGROUP\" g ON g.\"bGroupID\"=r.group_id "
           "JOIN app_global.\"TCHANNEL\" c ON c.\"bGroupID\"=r.group_id "
           "WHERE r.group_id=:w AND c.\"bChannel\"=:ch AND g.\"bType\"=0",
        soci::use(world,"w"),soci::use(channel,"ch"),soci::into(source_group);
    if (!sql.got_data()) return {StartStatus::NoGroup,{}};
    sql << "SELECT count(*) FROM route_compat.catalog_release WHERE manifest_sha256=:m AND status='verified'",
        soci::use(m_routing_manifest,"m"),soci::into(n);
    if (n!=1) throw std::runtime_error("Routing release unavailable");
    int map=0,spawn=0; double x=0,z=0;
    sql << "SELECT c.\"wMapID\",c.\"wSpawnID\",c.\"fPosX\",c.\"fPosZ\" FROM app_world.\"TCHARTABLE\" c "
           "JOIN app_global.\"TALLCHARTABLE\" d ON d.\"bWorldID\"=c.\"bWorldID\" AND d.\"dwCharID\"=c.\"dwCharID\" "
           "WHERE c.\"bWorldID\"=:w AND c.\"dwCharID\"=:c AND c.\"dwUserID\"=:u AND d.\"dwUserID\"=:u "
           "AND d.\"bDelete\"=0 FOR UPDATE OF c,d",
        soci::use(world,"w"),soci::use(cid,"c"),soci::use(user,"u"),soci::into(map),soci::into(spawn),soci::into(x),soci::into(z);
    if (!sql.got_data()) return {StartStatus::NoServer,{}};
    auto server=Route(sql,source_group,channel,map,x,z); bool fallback=false;
    if (!server) {
        sql << "SELECT \"wMapID\",\"fPosX\",\"fPosZ\" FROM route_compat.\"TSPAWNPOSCHART\" WHERE \"wID\"=:s",
            soci::use(spawn,"s"),soci::into(map),soci::into(x),soci::into(z);
        if (!sql.got_data()) return {StartStatus::NoGroup,{}};
        server=Route(sql,source_group,channel,map,x,z);fallback=true;
    }
    if (!server) return {StartStatus::NoGroup,{}};
    int machine=0,port=0;
    sql << "SELECT \"bMachineID\",\"wPort\" FROM app_global.\"TSERVER\" "
           "WHERE \"bGroupID\"=:w AND \"bServerID\"=:s AND \"bType\"=4",
        soci::use(world,"w"),soci::use(*server,"s"),soci::into(machine),soci::into(port);
    if (!sql.got_data()) return {StartStatus::NoServer,{}};
    int route=0;
    sql << "SELECT \"bRouteID\" FROM app_global.\"TMACHINE\" WHERE \"bMachineID\"=:m FOR UPDATE",
        soci::use(machine,"m"),soci::into(route);
    if (!sql.got_data()) return {StartStatus::NoServer,{}};
    std::vector<std::string> ips;
    { soci::rowset<std::string> rows=(sql.prepare << "SELECT \"szIPAddr\" FROM app_global.\"TIPADDR\" "
          "WHERE \"bMachineID\"=:m AND \"bActive\"=1 ORDER BY \"szIPAddr\"",soci::use(machine,"m"));
      for (const auto& ip:rows) ips.push_back(ip); }
    if (ips.empty() || ips.size()>255 || route<0) return {StartStatus::NoServer,{}};
    const int next=route%static_cast<int>(ips.size())+1; const auto& ip=ips[next-1];
    auto ep=Endpoint(ip,port,*server);if (!ep) return {StartStatus::NoServer,{}};
    sql << "UPDATE app_global.\"TMACHINE\" SET \"bRouteID\"=:r WHERE \"bMachineID\"=:m",soci::use(next,"r"),soci::use(machine,"m");
    if (fallback)
        sql << "UPDATE app_world.\"TCHARTABLE\" SET \"wMapID\"=:m,\"fPosX\"=:x,\"fPosY\"=0,\"fPosZ\"=:z "
               "WHERE \"bWorldID\"=:w AND \"dwCharID\"=:c",
            soci::use(map,"m"),soci::use(x,"x"),soci::use(z,"z"),soci::use(world,"w"),soci::use(cid,"c");
    sql << "UPDATE app_global.\"TCURRENTUSER\" SET \"dwCharID\"=:c,\"bGroupID\"=:w,\"bChannel\"=:ch,"
           "\"dEnterDate\"=CURRENT_TIMESTAMP,\"bLuckyNumber\"=floor(random()*100)::smallint WHERE \"dwKEY\"=:k AND \"dwUserID\"=:u",
        soci::use(cid,"c"),soci::use(world,"w"),soci::use(channel,"ch"),soci::use(key,"k"),soci::use(user,"u");
    sql << "INSERT INTO app_global.map_handoff(session_key,user_id,world_id,char_id,channel,server_id,server_ip,server_port,routing_manifest) "
           "VALUES(:k,:u,:w,:c,:ch,:s,CAST(:ip AS inet),:p,:m)",
        soci::use(key,"k"),soci::use(user,"u"),soci::use(world,"w"),soci::use(cid,"c"),soci::use(channel,"ch"),soci::use(*server,"s"),
        soci::use(ip,"ip"),soci::use(port,"p"),soci::use(m_routing_manifest,"m");
    sql << "UPDATE app_global.\"TLOG\" SET \"dwCharID\"=:c,\"bGroupID\"=:w,\"bChannel\"=:ch WHERE \"dwKEY\"=:k AND \"dwUserID\"=:u",
        soci::use(cid,"c"),soci::use(world,"w"),soci::use(channel,"ch"),soci::use(key,"k"),soci::use(user,"u");
    sql << "UPDATE app_global.\"TUSERINFOTABLE\" SET \"dwLastCharID\"=:c WHERE \"dwUserID\"=:u",soci::use(cid,"c"),soci::use(user,"u");
    tx->commit();return {StartStatus::Success,ep};
}
} // namespace tloginsvr::services
