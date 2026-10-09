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
};
struct AbilityEffect {int target{},increase{},value{};};
struct PostureTemplate {
    std::uint16_t skill{};
    std::uint32_t weapons{};
    std::vector<AbilityEffect> abilities;
};
}
