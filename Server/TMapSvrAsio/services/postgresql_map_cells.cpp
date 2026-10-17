#include "postgresql_map_service.h"
#include "fourstory/db/native_map_topology.h"
#include <soci/soci.h>
#include <bit>
#include <utility>

namespace tmapsvr {
namespace {
std::uint8_t Byte(const soci::row& row,const char* column) {
    if(row.get_indicator(column)!=soci::i_ok)throw std::runtime_error("NULL source cell BYTE");
    const auto n=row.get<int>(column);
    if(n<0||n>255)throw std::runtime_error("Source cell BYTE range");
    return static_cast<std::uint8_t>(n);
}
std::uint16_t Word(const soci::row& row,const char* column) {
    if(row.get_indicator(column)!=soci::i_ok)throw std::runtime_error("NULL source cell WORD");
    const auto n=row.get<int>(column);
    if(n < -32768||n>32767)throw std::runtime_error("Source cell SMALLINT range");
    return std::bit_cast<std::uint16_t>(static_cast<std::int16_t>(n));
}
}
void PostgreSQLMapService::CheckNativeCellRelease(soci::session& sql) const {
    const int world=m_config.world,server=m_config.server;int count=0;
    sql<<"SELECT count(*) FROM route_compat.cell_catalog_release WHERE world_id=:w AND server_id=:s "
         "AND run_id=:run AND manifest_sha256=:m AND routing_manifest=:r AND allocation_order_sha256=:a AND order_sha256=:o",
        soci::use(world,"w"),soci::use(server,"s"),soci::use(m_monster_catalog.run_id,"run"),
        soci::use(m_monster_catalog.manifest,"m"),soci::use(m_config.routing_manifest,"r"),
        soci::use(m_monster_catalog.order_sha256,"a"),soci::use(m_cells.order_sha256,"o"),soci::into(count);
    if(count!=1)throw std::runtime_error("Native source cell publication changed or unavailable");
}
void PostgreSQLMapService::LoadNativeCellCatalog(soci::session& sql) {
    const int world=m_config.world,server=m_config.server;long long expected=0;
    sql<<"SELECT order_sha256,channel_rows FROM route_compat.cell_catalog_release WHERE world_id=:w AND server_id=:s "
         "AND run_id=:run AND manifest_sha256=:m AND routing_manifest=:r AND allocation_order_sha256=:a",
        soci::use(world,"w"),soci::use(server,"s"),soci::use(m_monster_catalog.run_id,"run"),
        soci::use(m_monster_catalog.manifest,"m"),soci::use(m_config.routing_manifest,"r"),
        soci::use(m_monster_catalog.order_sha256,"a"),soci::into(m_cells.order_sha256),soci::into(expected);
    if(!sql.got_data()||expected<0||m_cells.order_sha256.size()!=64||
       m_cells.order_sha256.find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::runtime_error("Native Map requires explicit source cell publication081");
    long long order=0;
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT query_order,\"bChannel\" FROM route_compat.source_cell_channels WHERE world_id=:w AND server_id=:s ORDER BY query_order",
        soci::use(world,"w"),soci::use(server,"s"));
     for(const auto& row:rows) {
        const auto channel=Byte(row,"bChannel");
        if(channel>=NativeCellChannels||row.get<long long>("query_order")!=++order)
            throw std::runtime_error("Source CTBLChannelList order/MAX_CHANNEL51");
        m_cells.channels.push_back(channel);
     }}
    if(order!=expected)throw std::runtime_error("Incomplete source CTBLChannelList query");
    // This is the procedure's global join, not the group-filtered route helper.
    // Ambiguity is resolved only by refusing an actually needed query in Server.
    {soci::rowset<soci::row> rows=(sql.prepare<<
        "SELECT \"wMapID\",\"wUnitID\",\"bLogChannel\",\"bServerID\" FROM route_compat.source_cell_owners");
     std::size_t count=0;
     for(const auto& row:rows) {
        if(++count>1'000'000)throw std::runtime_error("Native cell ownership resource limit");
        m_cells.owners[{Word(row,"wMapID"),Word(row,"wUnitID"),Byte(row,"bLogChannel")}].insert(Byte(row,"bServerID"));
     }}
    CheckNativeCellRelease(sql);
    m_cell_channels=fourstory::db::ReadNativeMapTopology(sql,m_config.world,m_config.server,m_config.routing_manifest).logical_channels;
}
const NativeMapCell* PostgreSQLMapService::FindNativeCell(std::uint8_t logical,std::uint16_t map,std::uint32_t cell,bool allow_template) const {
    // MAX_CHANNEL is an internal IsMainCell sentinel, not a safe index for
    // CheckPlayer's neighbor-server array. Do not admit such a client route.
    if(logical>=NativeCellChannels)return nullptr;
    const auto channel=m_cell_channels.find(std::uint32_t{map}|(std::uint32_t{logical}<<16));
    if(channel==m_cell_channels.end())return nullptr;
    const auto key=std::uint32_t{map}|(std::uint32_t{channel->second}<<16);
    const auto environment=m_battles.maps.find(key);
    // FindTMap(party0,map) cannot return an instance template. Instance
    // creation and party-specific lifetime remain a separate missing contract.
    // Module IsMainCell queries the template map directly; actual player
    // movement must resolve a live party map instead. That producer is missing.
    if(environment==m_battles.maps.end())return nullptr;
    if(!allow_template&&environment->second.instance_template)
        throw std::runtime_error("Native Map cell lookup requires live party-instance map lifetime");
    const auto found=m_monster_maps.find(key);
    if(found==m_monster_maps.end())return nullptr;
    const auto position=found->second.cells.find(cell);
    return position==found->second.cells.end()?nullptr:&position->second;
}
void PostgreSQLMapService::InitializeNativeCells() {
    constexpr std::size_t maximum_cells=262144; // bounded source graph, no silent truncation
    std::size_t count=0;
    // Match InitChannel's physical-channel then WORD-map iteration order.
    std::map<std::uint8_t,std::set<std::uint16_t>> channels;
    for(const auto& [key,environment]:m_battles.maps)
        channels[static_cast<std::uint8_t>(key>>16)].insert(static_cast<std::uint16_t>(key));
    for(const auto& [channel,maps]:channels)for(auto map:maps) {
        const auto key=std::uint32_t{map}|(std::uint32_t{channel}<<16);
        auto cells=BuildNativeMapCells(m_cells,map,m_battles.maps.at(key).units,m_config.server,maximum_cells-count);
        count+=cells.size();
        auto& state=m_monster_maps[key];state.map_id=map;state.physical_channel=channel;
        state.cells=std::move(cells);
    }
}
} // namespace tmapsvr
