#pragma once

// Complete source RELEASEMAIN/ENTERSVR transfer body, independent of the
// client-facing CHARINFO projection. Source: TMapSvr/SSSender.cpp:1999–2638,
// TItem.cpp:468 and SSHandler.cpp:4424 (non-login LOADCHAR branch).
#include "domain/character.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace tmapsvr::transfer {
struct Magic { std::uint8_t id{}; std::uint16_t value{}; };
struct Item {
    std::uint8_t storage{},owner_type{};
    std::uint32_t storage_id{},owner_id{};
    std::uint64_t id{};
    std::uint8_t slot{},level{},gem{},count{},grade{},refine{},grade_effect{};
    std::uint16_t item{},appearance{};
    std::uint32_t companion{},durability_max{},durability{},eld{},wrap{},color{},guild{},texture{};
    std::int64_t expires{};
    std::vector<Magic> magic; // raw server values, not derived client values
};
using Buff=MaintainedEffect;
struct Quest {
    std::uint32_t id{},remaining{};
    std::uint8_t completed{},triggered{},save{};
};
struct QuestTerm { std::uint32_t quest{},id{}; std::uint8_t type{},count{}; };
struct Hotkeys { CharacterHotkeys row; std::uint8_t save{}; };
struct ItemCooldown { std::uint16_t group{}; std::uint32_t remaining{}; };
struct Saddle { std::uint32_t item{}; std::int64_t expires{}; std::uint8_t type{}; };
struct DuringItem { std::uint16_t item{}; std::uint8_t type{}; std::uint32_t remaining{}; std::int64_t expires{}; };
struct RecallBuff { std::uint32_t recall{}; Buff buff; };
struct Protected { std::uint32_t id{}; std::string name; std::uint8_t option{},changed{}; };
struct Record {
    std::string name;
    std::uint8_t klass{},level{},win{};
    std::uint32_t points{};
    std::int64_t time{};
};
struct Companion {
    std::uint8_t slot{},skill_points{},level{},effect{},bonus{};
    std::uint32_t monster{},experience{},next_experience{};
    std::uint16_t life{};
    std::string name;
    std::array<std::uint16_t,6> attributes{};
};
struct CompanionItems {
    std::uint8_t slot{};
    std::uint32_t tick{};
    std::array<std::uint16_t,2> items{};
    std::array<std::int64_t,2> expires{};
};
struct State {
    std::uint8_t db_load{},result{},login{},new_security{},security_tries{},security_unlocked{};
    std::uint32_t key{},save_age{},security_tick{};
    std::uint16_t local_id{};
    CharSnapshot character; // only source core fields occur in this wire body
    std::string security_code;
    std::uint8_t aid_country{},pc_bang{},pc_bang_items{},lucky{};
    std::int64_t aid_date{};
    std::uint32_t pc_bang_time{};
    std::uint16_t post_total{},post_read{};
    std::vector<InventoryRow> bags;
    std::vector<CharacterCabinet> cabinets;
    std::vector<Item> items;
    std::vector<SkillRow> skills;
    std::vector<Buff> buffs;
    std::vector<Quest> quests;
    std::vector<QuestTerm> quest_terms;
    std::vector<Hotkeys> hotkeys;
    std::vector<ItemCooldown> item_cooldowns;
    Saddle saddle;
    std::vector<CharacterPet> pets;
    std::vector<DuringItem> during_items;
    std::vector<CharacterRecall> recalls;
    std::vector<RecallBuff> recall_buffs;
    std::vector<Protected> protected_characters;
    std::uint32_t pvp_available{},pvp_total{},pvp_rank{};
    std::uint8_t pvp_percent{};
    std::array<std::array<std::uint32_t,2>,6> pvp_records{};
    std::vector<Record> pvp_recent,duel_records;
    std::uint16_t duel_sets{1}; // source always emits one six-class score set
    std::array<std::array<std::uint32_t,2>,6> duel_scores{};
    std::vector<std::uint32_t> auction_bids,auction_interests,auction_registrations;
    std::uint32_t month_points{},month_rank{};
    std::uint16_t month_wins{},month_losses{};
    std::uint8_t month_percent{};
    std::vector<CharacterTitle> titles;
    std::vector<Companion> companions;
    std::vector<CompanionItems> companion_items;
    std::uint8_t companion_slots{};
    std::uint32_t medals{},rank_points{},play_time{};
};
}
