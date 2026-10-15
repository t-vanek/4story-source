#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <memory>

namespace tmapsvr {
struct CharSnapshot;
struct EffectEndRequest {
    std::uint32_t object{},host{},attacker{};
    std::uint8_t object_type{},attack_type{},channel{};
    std::uint16_t skill{},map{};
};
struct EffectEndCommit {
    std::shared_ptr<const CharSnapshot> snapshot;
    bool removed{};
};
// First eight fields are the original CTBLSkillMaintain/RELEASEMAIN contract.
// Presentation fields are not persisted by the original server: CTSkill's
// constructor restores them on LOADCHAR, including after a primary transfer.
struct MaintainedEffect {
    std::uint8_t level{},attack_type{},host_type{},attack_country{};
    std::uint16_t skill{};
    std::uint32_t remaining{},attack_id{},host_id{};
    std::uint8_t hit=1,attacker_level=1,can_select=1;
    std::uint16_t attack_level=1;
    std::array<std::uint32_t,4> powers{};
    std::array<float,3> position{};
    // Process-local observations. The original eight-field database/wire
    // representation stores remaining time, never another process's tick.
    std::uint64_t sampled_ms{},charge_ms{};
};
// A save/transfer projection proves which process-local timer observations
// produced its durable remaining times. It is never encoded or persisted.
struct MaintainedEffectSample {
    std::vector<MaintainedEffect> before;
    std::uint64_t at_ms{};
};
struct AbilityEffect {int target{},increase{},value{};};
struct PostureTemplate {
    std::uint16_t skill{};
    std::uint32_t weapons{};
    std::vector<AbilityEffect> abilities;
};
struct ProgressEffectTemplate {
    std::uint16_t skill{},value{};
    std::uint8_t status{},priority{},attribute{};
    std::uint32_t duration{};
    bool erase_at_zero_mp{}; // original bIsuse && !dwDuration, even for a forced timed effect
    std::uint8_t erase_act{}; // validated BEA_ATTACK / BEA_DEFEND, forced markers800/802
    bool is_static{true}; // source PushMaintainSkill refuses nonstatic skills on a corpse
};
}
