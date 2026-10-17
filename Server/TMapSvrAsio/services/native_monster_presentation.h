#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace tmapsvr {
// Owned CTSkill state for source monster presentation. These are original
// DWORD start/duration clocks, not the player's persisted remaining-ms model.
struct NativeMonsterMaintain {
    std::uint16_t skill{},attack_level{1};
    std::uint8_t level{1},attack_type{1},host_type{1},hit{1},attacker_level{1},can_select{1},attack_country{3};
    std::uint32_t start_tick{},duration{},attack_id{},host_id{};
    std::array<std::uint32_t,4> powers{};
    std::array<float,3> position{};
};
struct NativeMonsterObserver {
    std::uint32_t character{},party{};
    std::uint8_t war_country{};
};
// GetColor needs no vitals/effect projection. Capture only its actual source
// inputs before ResetHost clears state, independently of ADDMON readiness.
struct NativeMonsterColorState {
    std::uint32_t keeper{};
    std::uint8_t mode{},country{3},aid_country{3},keeper_type{};
    std::optional<std::uint8_t> own_ai_type;
};
struct NativeMonsterMotion {
    float x{},y{},z{};
    std::uint16_t pitch{},direction{};
    std::uint8_t mouse{4},key{4},action{};
};
struct NativeMonsterAttack {
    std::uint32_t target{};
    std::uint8_t target_type{};
    std::uint16_t skill{};
};
struct NativeMonsterAction {
    std::uint8_t action{};
    float x{},y{},z{};
    std::uint32_t target{};
    std::uint8_t target_type{};
};
struct NativeMonsterMoveRecord {
    std::uint32_t id{};
    std::uint64_t generation{}; // lifetime fence only, never sent
    NativeMonsterMotion motion;
};
// Immutable wire projection, captured before the following source AI event.
// It survives slot deletion/reuse; only the original BYTE-sized effect prefix
// is retained. It is not another monster registry or authoritative actor.
struct NativeMonsterAppearance {
    std::uint64_t generation{};
    std::uint32_t id{},max_hp{},hp{},max_mp{},mp{},region{},keeper{},tick{};
    std::uint16_t monster{},pitch{},direction{};
    std::uint8_t template_level{},mouse{4},key{4},action{},mode{},country{3},aid_country{3},keeper_type{};
    std::optional<std::uint8_t> own_ai_type; // actual template pointer, never DEFAULT_AI fallback
    float x{},y{},z{};
    std::vector<NativeMonsterMaintain> maintained;
    std::size_t WireSize() const {return 51+51*maintained.size();}
};
struct NativeMonsterSlot;
struct NativeMonsterCatalog;
struct NativeMonsterAiCatalog;
NativeMonsterColorState ProjectNativeMonsterColor(const NativeMonsterSlot&,const NativeMonsterCatalog&,
                                                  const NativeMonsterAiCatalog&);
std::uint8_t NativeMonsterColor(const NativeMonsterColorState&,const NativeMonsterObserver&);
NativeMonsterAppearance ProjectNativeMonsterAppearance(const NativeMonsterSlot&,const NativeMonsterCatalog&,
                                                       const NativeMonsterAiCatalog&,std::uint32_t tick);
std::uint8_t NativeMonsterColor(const NativeMonsterAppearance&,const NativeMonsterObserver&);
std::vector<std::byte> EncodeNativeAddMonAck(const NativeMonsterAppearance&,const NativeMonsterObserver&,bool new_member);
std::vector<std::byte> EncodeNativeMonAttackAck(std::uint32_t monster,const NativeMonsterAttack&);
std::vector<std::byte> EncodeNativeMonActionAck(std::uint32_t monster,const NativeMonsterAction&);
std::vector<std::byte> EncodeNativeMonMoveAck(const std::vector<NativeMonsterMoveRecord>&);
} // namespace tmapsvr
