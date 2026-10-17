#pragma once
#include "domain/session.h"
#include "native_monster_presentation.h"
#include <memory>
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
struct NativeCellPlayer {
    MapSessionClaim claim;
    float x{},z{};
};
enum class NativeCellNoticeKind { Enter, Leave, Move, AddMonster, DeleteMonster };
struct NativeCellNotice {
    NativeCellNoticeKind kind;
    MapSessionClaim recipient,actor;
    bool flag{}; // Enter/AddMonster: new member; Leave/DeleteMonster: exit map
    std::uint32_t monster_id{};
    std::uint64_t monster_generation{};
    std::shared_ptr<const NativeMonsterAppearance> monster{};
};
using NativeCellNotices=std::vector<NativeCellNotice>;
bool SameNativeCellPlayer(const MapSessionClaim&,const MapSessionClaim&);
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
    std::map<std::uint32_t,NativeCellPlayer> players; // source DWORD order, owned connection identities
    bool IsMain(std::uint8_t channel) const;
    bool IsEnabled(std::uint8_t channel) const;
};
using NativeMapCells=std::map<std::uint32_t,NativeMapCell>;
std::vector<std::uint32_t> NativeNeighborCells(const NativeMapCells&,std::uint32_t position);
NativeMapCells BuildNativeMapCells(const NativeCellCatalog&,std::uint16_t map,
    const std::set<std::uint16_t>& units,std::uint8_t server,std::size_t maximum_cells);
// Explicit WORD(float)/CELL_SIZE, refusing undefined out-of-range conversions.
std::optional<std::uint32_t> NativeCellAt(float x,float z);
// CTMapSvrModule::IsMainCell instead divides FLOAT before converting WORD.
std::optional<std::uint32_t> NativeMainCellAt(float x,float z);
struct NativeMonsterMap;
// Ordinary PC-only cell lifecycle. The service's latched admission barrier
// excludes monster AI, recall/self/companion and instance-map populations.
NativeCellNotices EnterNativeCellPlayer(NativeMonsterMap&,const NativeCellPlayer&,bool new_member);
NativeCellNotices MoveNativeCellPlayer(NativeMonsterMap&,const MapSessionClaim&,float x,float z,
                                     const std::vector<std::uint8_t>& source_channels,bool broadcast_movement=true);
void RebindNativeCellPlayer(NativeMonsterMap&,const MapSessionClaim& previous,const MapSessionClaim& current);
NativeCellNotices LeaveNativeCellPlayer(NativeMonsterMap&,const MapSessionClaim&);
// Normal OT_MON cell insertion/removal and captured client notices. The full
// hosted actor path remains guarded at admission. AT_ENTER is emitted by the
// executor exactly once AFTER entry returns, not once per recipient.
NativeCellNotices EnterNativeMonster(NativeMonsterMap&,std::uint32_t,const NativeMonsterCatalog&,
                                   const NativeMonsterAiCatalog&,std::uint32_t tick);
NativeCellNotices LeaveNativeMonster(NativeMonsterMap&,std::uint32_t,bool exit_map);
} // namespace tmapsvr
