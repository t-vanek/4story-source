#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace tmapsvr {
// Server/TMapSvr/TMapType.h, not the registration packet's BYTE limit.
inline constexpr std::size_t NativeCellChannels=51;
struct NativeCellCatalog {
    std::vector<std::uint8_t> channels; // actual CTBLChannelList query order, duplicates retained
    // TGetServerID has no group filter. Keep all joined owners until a query
    // is needed; different owners for that query require reconciliation.
    std::map<std::tuple<std::uint16_t,std::uint16_t,std::uint8_t>,std::set<std::uint8_t>> owners;
    std::string order_sha256;
    std::optional<std::uint8_t> Server(std::uint16_t map,std::uint16_t unit,std::uint8_t logical) const;
};
struct NativeMapCell {
    std::array<bool,NativeCellChannels> enabled{},extended{};
    std::array<std::array<std::uint8_t,NativeCellChannels>,8> servers{};
    std::set<std::uint32_t> monsters; // source OT_MON only; no borrowed pointers
    bool IsMain(std::uint8_t channel) const;
    bool IsEnabled(std::uint8_t channel) const;
};
using NativeMapCells=std::map<std::uint32_t,NativeMapCell>;
NativeMapCells BuildNativeMapCells(const NativeCellCatalog&,std::uint16_t map,
    const std::set<std::uint16_t>& units,std::uint8_t server,std::size_t maximum_cells);
// Explicit WORD(float)/CELL_SIZE, refusing undefined out-of-range conversions.
std::optional<std::uint32_t> NativeCellAt(float x,float z);
// CTMapSvrModule::IsMainCell instead divides FLOAT before converting WORD.
std::optional<std::uint32_t> NativeMainCellAt(float x,float z);
struct NativeMonsterMap;
// These consumers require a proven actor-free population, during LoadData or
// the native AI loop while all player-admission paths remain blocked.
// AT_ENTER remains the bootstrap executor's responsibility, exactly once.
void EnterNativeBootstrapMonster(NativeMonsterMap&,std::uint32_t);
void LeaveNativeBootstrapMonster(NativeMonsterMap&,std::uint32_t);
} // namespace tmapsvr
