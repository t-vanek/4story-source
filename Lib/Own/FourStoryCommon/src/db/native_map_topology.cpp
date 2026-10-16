#include "fourstory/db/native_map_topology.h"
#include <soci/soci.h>
#include <bit>
#include <set>
#include <stdexcept>

namespace fourstory::db {
namespace {
std::uint8_t Byte(const soci::row& row,const char* column) {
    if(row.get_indicator(column)!=soci::i_ok)throw std::runtime_error("NULL source topology BYTE");
    const auto value=row.get<int>(column);
    if(value<0||value>255)throw std::runtime_error("Source topology BYTE range");
    return static_cast<std::uint8_t>(value);
}
std::uint16_t Word(const soci::row& row,const char* column) {
    if(row.get_indicator(column)!=soci::i_ok)throw std::runtime_error("NULL source topology WORD");
    const auto value=row.get<int>(column);
    if(value < -32768||value>32767)throw std::runtime_error("Source topology SMALLINT range");
    return std::bit_cast<std::uint16_t>(static_cast<std::int16_t>(value));
}
}
std::string NativeMapTopology::Json() const {
    std::string json="[[";bool first=true;
    for(auto physical:physical_channels) {
        if(!first)json+=',';first=false;json+=std::to_string(physical);
    }
    json+="],[";first=true;
    for(const auto& [key,physical]:logical_channels) {
        if(!first)json+=',';first=false;
        json+='['+std::to_string(key)+','+std::to_string(physical)+']';
    }
    return json+"]]";
}
NativeMapTopology ReadNativeMapTopology(soci::session& sql,std::uint8_t world,
    std::uint8_t server,const std::string& expected_manifest) {
    if(!server)throw std::runtime_error("Native Map server identity is zero");
    NativeMapTopology topology;const int wid=world,sid=server;int source_group=-1,endpoint=0;
    sql<<"SELECT manifest_sha256,source_group FROM route_compat.map_topology_release WHERE world_id=:w",
        soci::use(wid,"w"),soci::into(topology.manifest),soci::into(source_group);
    if(!sql.got_data()||topology.manifest.size()!=64||
       topology.manifest.find_first_not_of("0123456789abcdef")!=std::string::npos||source_group<0||source_group>255||
       (!expected_manifest.empty()&&topology.manifest!=expected_manifest))
        throw std::runtime_error("Complete verified native Map routing publication required");
    topology.source_group=static_cast<std::uint8_t>(source_group);
    sql<<"SELECT count(*) FROM app_global.\"TSERVER\" WHERE \"bGroupID\"=:w AND \"bServerID\"=:s AND \"bType\"=4",
        soci::use(wid,"w"),soci::use(sid,"s"),soci::into(endpoint);
    if(endpoint!=1)throw std::runtime_error("Native Map endpoint identity is unavailable or ambiguous");
    std::set<std::uint8_t> physical;
    // The original queries filter by server only. Do not silently turn that
    // into a group-filtered query: cross-group reuse needs explicit resolution.
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT \"bGroup\",\"bChannel\",\"wMapID\",\"wUnitID\" FROM route_compat.source_map_regions WHERE \"bServerID\"=:s",
        soci::use(sid,"s"));
     for(const auto& row:rows) {
        if(Byte(row,"bGroup")!=topology.source_group)throw std::runtime_error("Source Map server spans unmapped routing groups");
        physical.insert(Byte(row,"bChannel"));
        (void)Word(row,"wMapID");(void)Word(row,"wUnitID");
     }}
    topology.physical_channels.assign(physical.begin(),physical.end());
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT \"bGroupID\",\"wMapID\",\"bLogChannel\",\"bPhyChannel\" FROM route_compat.source_map_channels WHERE \"bServerID\"=:s",
        soci::use(sid,"s"));
     for(const auto& row:rows) {
        if(Byte(row,"bGroupID")!=topology.source_group)throw std::runtime_error("Source logical Map channel spans unmapped groups");
        const auto phy=Byte(row,"bPhyChannel"),logical=Byte(row,"bLogChannel");
        const auto map=Word(row,"wMapID");
        if(!physical.contains(phy))continue; // original m_mapTCHANNEL.find branch
        const std::uint32_t key=map|(std::uint32_t{logical}<<16);
        const auto [it,inserted]=topology.logical_channels.emplace(key,phy);
        if(!inserted&&it->second!=phy)
            throw std::runtime_error("Source logical Map key has order-dependent physical ownership");
        if(topology.logical_channels.size()>255)
            throw std::runtime_error("Source Map registration BYTE count overflow requires reconciliation");
     }}
    // SendMW_CONNECT_ACK casts the SIZE to BYTE but serializes every key.
    // Do not normalize/deduplicate BYTE(key) values or invent logical channels.
    // A >255-key profile needs explicit remainder/framing reconciliation.
    return topology;
}
}
