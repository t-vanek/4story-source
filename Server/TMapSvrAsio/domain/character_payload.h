#pragma once
#include "domain/inventory.h"
#include "domain/skill.h"
#include "domain/character_statistics.h"
#include <array>
#include <memory>
#include <optional>
#include <cstdint>
#include <string>
#include <vector>

namespace tmapsvr {
namespace transfer {struct State;}
struct CharacterBag {
    InventoryRow bag;
    std::vector<ItemInstance> items;
    std::uint8_t slot_count{}; // pinned TITEMCHART capacity; server-only, never on wire
};
struct CharacterHotkeys { std::uint8_t inventory{}; std::array<std::pair<std::uint8_t,std::uint16_t>,12> keys{}; };
struct CharacterTitle { std::uint16_t id{}; bool selected{}; };
struct CharacterCabinet { std::uint8_t id{},use{}; };
struct CharacterPost {
    std::uint32_t id{},sender_id{},gold{},silver{},cooper{};
    std::uint8_t type{},read{};
    std::int64_t received_at{};
    std::string recipient,sender,title,message;
};
struct CharacterRecall {
    std::uint32_t id{},attribute{},hp{},mp{},time{};
    std::uint16_t monster{},pet{},x{},y{},z{};
    std::uint8_t level{},skill_level{},effect{};
};
struct CharacterPet { std::uint16_t id{}; std::string name; std::int64_t used_at{}; std::uint8_t effect{}; };
// Immutable load graph shared by snapshot copies. Mutations of durable children
// belong to their fenced repositories; core movement/save never replaces mail,
// inventory or skills with empty/unloaded collections.
struct CharacterPayload {
    // Full typed source graph retained across primary tenures. Its nested
    // character has no payload pointer, avoiding a shared ownership cycle.
    std::shared_ptr<const transfer::State> transfer_state;
    std::uint64_t transfer_received_ms{};
    std::vector<CharacterBag> bags;
    std::vector<SkillRow> skills;
    // Static definitions for learned skills, hydrated from the pinned character
    // catalog on both fresh load and transfer. Never serialized as player data.
    std::vector<SkillTemplate> skill_templates;
    // Source-derived physical/long/magic timing (TAD 1..3). Buff-bearing
    // transfers leave this absent until active-effect timing is implemented.
    std::optional<std::array<SkillAttackTiming,3>> skill_attack_timing;
    // Absent for active-effect/companion/guild states whose stat semantics are
    // not yet implemented. Never synthesize a successful zero-filled sheet.
    std::optional<CharacterStatistics> statistics;
    std::vector<CharacterHotkeys> hotkeys;
    std::vector<CharacterTitle> titles;
    std::vector<CharacterCabinet> cabinets;
    std::vector<CharacterPost> posts;
    std::vector<CharacterRecall> recalls;
    std::vector<CharacterPet> pets;
    std::array<std::uint16_t,4> skill_points{};
    std::uint32_t prev_exp{},next_exp{},rank_point{};
    std::uint8_t lucky_number{},aid_country{3}; // original absent TAIDTABLE -> TCONTRY_N
    std::uint16_t selected_title{};
};
}
