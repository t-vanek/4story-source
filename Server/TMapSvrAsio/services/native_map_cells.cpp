#include "native_map_cells.h"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace tmapsvr {
namespace {
constexpr std::array<std::array<int,2>,8> Directions{{
    {-1,-1},{0,-1},{1,-1},{-1,0},{1,0},{-1,1},{0,1},{1,1}}};
constexpr std::uint32_t Cell(std::uint16_t x,std::uint16_t z) {
    return std::uint32_t{x}|(std::uint32_t{z}<<16);
}
}
std::optional<std::uint8_t> NativeCellCatalog::Server(std::uint16_t map,std::uint16_t unit,std::uint8_t logical) const {
    const auto found=owners.find({map,unit,logical});
    if(found==owners.end())return {}; // procedure RETURN1, not server0
    if(found->second.size()!=1)throw std::runtime_error("Ambiguous source TGetServerID TOP1; map="+
        std::to_string(map)+" unit="+std::to_string(unit)+" logical_channel="+std::to_string(logical));
    return *found->second.begin();
}
bool NativeMapCell::IsMain(std::uint8_t channel) const {
    return channel==NativeCellChannels || (channel<NativeCellChannels&&enabled[channel]&&!extended[channel]);
}
bool NativeMapCell::IsEnabled(std::uint8_t channel) const {
    return channel==NativeCellChannels || (channel<NativeCellChannels&&enabled[channel]);
}
std::vector<std::uint32_t> NativeNeighborCells(const NativeMapCells& cells,std::uint32_t position) {
    const int x=position&65535,z=position>>16;
    std::vector<std::uint32_t> out;
    for(int nz=z-1;nz<=z+1;++nz)for(int nx=x-1;nx<=x+1;++nx)if(nx>=0&&nz>=0) {
        const auto key=static_cast<std::uint32_t>(nx)|(static_cast<std::uint32_t>(nz)<<16);
        if(cells.contains(key))out.push_back(key);
    }
    return out;
}
NativeMapCells BuildNativeMapCells(const NativeCellCatalog& catalog,std::uint16_t map,
    const std::set<std::uint16_t>& units,std::uint8_t server,std::size_t maximum_cells) {
    for(auto channel:catalog.channels)if(channel>=NativeCellChannels)
        throw std::runtime_error("Source CTBLChannelList indexes outside MAX_CHANNEL51");
    if(units.size()>maximum_cells/256)throw std::runtime_error("Native cell resource limit");
    NativeMapCells cells;
    // InitMap: all primary units in WORD order, then all extension units.
    for(auto unit:units) {
        const int ux=unit&255,uz=unit>>8;
        std::array<bool,NativeCellChannels> enabled{};
        std::array<std::array<std::uint8_t,NativeCellChannels>,8> neighbors{};
        for(auto& direction:neighbors)direction.fill(server);
        for(std::size_t direction=0;direction<Directions.size();++direction) {
            const int nx=ux+Directions[direction][0],nz=uz+Directions[direction][1];
            // The source uses own unit if EITHER component is negative;
            // positive overflow wraps each BYTE, including255+1.
            const auto adjacent=nx<0||nz<0?unit:static_cast<std::uint16_t>(
                std::uint16_t{static_cast<std::uint8_t>(nx)}|
                (std::uint16_t{static_cast<std::uint8_t>(nz)}<<8));
            for(auto channel:catalog.channels)if(auto owner=catalog.Server(map,adjacent,channel))
                neighbors[direction][channel]=*owner;
        }
        for(auto channel:catalog.channels)if(auto owner=catalog.Server(map,unit,channel))
            enabled[channel]=*owner==server;
        for(int z=0;z<16;++z)for(int x=0;x<16;++x) {
            NativeMapCell cell;cell.enabled=enabled;
            const std::array<bool,8> edge{{z==0&&x==0,z==0,z==0&&x==15,x==0,x==15,
                                         z==15&&x==0,z==15,z==15&&x==15}};
            for(std::size_t direction=0;direction<Directions.size();++direction)
                for(std::size_t channel=0;channel<NativeCellChannels;++channel)
                    cell.servers[direction][channel]=edge[direction]&&neighbors[direction][channel]!=server?
                        neighbors[direction][channel]:0;
            cells.emplace(Cell(static_cast<std::uint16_t>(ux*16+x),static_cast<std::uint16_t>(uz*16+z)),std::move(cell));
        }
    }
    for(auto unit:units)for(int z=0;z<16;++z)for(int x=0;x<16;++x) {
        const int cx=(unit&255)*16+x,cz=(unit>>8)*16+z;
        const auto origin=cells.find(Cell(static_cast<std::uint16_t>(cx),static_cast<std::uint16_t>(cz)));
        if(origin==cells.end())continue;
        for(std::size_t direction=0;direction<Directions.size();++direction) {
            const auto adjacent=Cell(static_cast<std::uint16_t>(cx+Directions[direction][0]),
                                     static_cast<std::uint16_t>(cz+Directions[direction][1]));
            for(auto channel:catalog.channels)if(origin->second.enabled[channel]&&origin->second.servers[direction][channel]) {
                if(!cells.contains(adjacent)&&cells.size()>=maximum_cells)
                    throw std::runtime_error("Native extended-cell resource limit");
                auto& extension=cells[adjacent];
                extension.extended[channel]=true;extension.enabled[channel]=true;
            }
        }
    }
    return cells;
}
std::optional<std::uint32_t> NativeCellAt(float x,float z) {
    if(!std::isfinite(x)||!std::isfinite(z)||std::trunc(x)<0||std::trunc(z)<0||
       std::trunc(x)>65535||std::trunc(z)>65535)return {};
    return Cell(static_cast<std::uint16_t>(x)/64,static_cast<std::uint16_t>(z)/64);
}
std::optional<std::uint32_t> NativeMainCellAt(float x,float z) {
    const float cx=x/64.0f,cz=z/64.0f;
    if(!std::isfinite(cx)||!std::isfinite(cz)||std::trunc(cx)<0||std::trunc(cz)<0||
       std::trunc(cx)>65535||std::trunc(cz)>65535)return {};
    return Cell(static_cast<std::uint16_t>(cx),static_cast<std::uint16_t>(cz));
}
} // namespace tmapsvr
