#pragma once
#include "domain/character.h"
#include "domain/session.h"
#include <cstdint>
#include <optional>

namespace tmapsvr {
// CTPlayer connection-local state. No DB/transfer fields are added. A missing
// move tick is an unobserved source member, not a fabricated tick0.
struct NativePlayerHostState {
    bool can_host{},hp_zero{true}; // initial CTPlayer/CTObjBase HP is really0
    std::uint8_t ghost{};
    std::optional<std::uint32_t> move_tick;
    float ghost_x{},ghost_z{};
};
struct NativePlayerMoveInput {
    float x{},y{},z{},speed{};
    std::uint16_t pitch{},direction{};
    std::uint8_t mouse{},key{},action{},ghost{};
};
// Owned preparation across route/World suspension; Apply checks the exact
// cell lifetime/revision before any mutation. Ghost coordinates stay separate.
struct NativePlayerMovePlan {
    MapSessionClaim claim;
    std::uint64_t revision{};
    std::uint32_t origin_cell{};
    std::uint16_t map{},direction{};
    NativePlayerHostState host;
    SourceMovementState movement;
    float x{},y{},z{};
    bool early_return{};
};
struct NativeCellPlayer;
struct NativeMonsterMap;
NativePlayerMovePlan PlanNativePlayerHostMove(const NativeCellPlayer&,const CharSnapshot&,
                                            const NativePlayerMoveInput&,std::uint32_t tick);
bool NativePlayerCanHost(const NativeCellPlayer&,float monster_x,float monster_z);
struct NativeMonsterHostChoice {
    std::optional<MapSessionClaim> player;
    bool activate_first{}; // source fallback's CanHost=TRUE, applied by SetHost/ChkHost command
    bool keep_current{}; // CHKHOST returns before neighbor selection when old host CanHost
};
NativeMonsterHostChoice SelectNativeMonsterHost(const NativeMonsterMap&,float x,float z,std::uint32_t tick);
std::optional<MapSessionClaim> FindNativeMonsterHost(const NativeMonsterMap&,float x,float z,std::uint32_t id);
NativeMonsterHostChoice SelectNativeMonsterReplacementHost(const NativeMonsterMap&,float x,float z,
                                                         const std::optional<MapSessionClaim>& previous);
} // namespace tmapsvr
