#pragma once
#include "domain/inventory.h"
#include "domain/secure_code.h"
#include "domain/quest.h"
#include "domain/skill.h"
#include "domain/character_statistics.h"
#include "domain/maintained_effect.h"
#include "domain/during_item.h"
#include "domain/item_cooldown.h"
#include "domain/pvp_balance.h"
#include "domain/communication.h"
#include <array>
#include <memory>
#include <optional>
#include <set>
#include <cstdint>
#include <string>
#include <vector>
#include <utility>

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
    std::uint8_t contains{}; // native derived attachment/funds availability
};
struct CharacterProtected { std::uint32_t id{}; std::string name; std::uint8_t option{}; };
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
    ChatFloodState chat_log;
    SecureCodeState security;
    // Full typed source graph retained across primary tenures. Its nested
    // character has no payload pointer, avoiding a shared ownership cycle.
    std::shared_ptr<const transfer::State> transfer_state;
    std::uint64_t transfer_received_ms{};
    std::uint64_t last_title_selection_id{}; // durable title selection fence, never on wire
    std::uint64_t last_purchase_id{}; // fences money/rank/title mutations, never on wire
    std::uint64_t last_cast_id{}; // native ledger head; rehydrated, never sent to the client
    std::uint64_t last_action_id{}; // action/charge ledger fence, never a wire field
    std::uint64_t last_finish_id{}; // fences stale snapshots after target resolution
    std::uint64_t last_quest_id{}; // durable quest mutation fence, never on wire
    // Mail already represented in admission/list counters. Persist separately
    // from the original transfer bytes; a handoff must not count it twice.
    std::uint64_t mail_notification_baseline{},mail_observed_notification{};
    std::optional<std::vector<QuestProgressRow>> quests;
    std::uint64_t quest_sample_ms{}; // server-only timer observation
    // CTPlayer::m_mapLevelQuest is a live-session cache, rebuilt from loaded
    // quest definitions on entry and explicitly erased on DropQuest.
    std::optional<std::set<std::uint32_t>> quest_levels;
    std::vector<CharacterBag> bags;
    std::vector<SkillRow> skills;
    // Optional only for legacy/test projections which have not hydrated effects.
    // Native fresh and transferred primaries always load the complete collection.
    std::optional<std::vector<MaintainedEffect>> effects;
    std::optional<MaintainedEffectSample> effect_sample;
    std::array<PostureTemplate,2> posture_templates{};
    std::vector<ProgressEffectTemplate> progress_effect_templates;
    std::optional<DuringItemState> during;
    std::optional<ItemCooldownState> item_cooldowns;
    std::optional<PvpBalance> pvp; // fresh normalized rows or the authoritative transfer graph
    // One observation for all nested actions in a native transaction. It is
    // never serialized; a newly forced effect must not start after its own tick.
    std::uint64_t event_ms{};
    // CTPlayer::Defend updates m_dwLastAtkTick even for an item self-buff.
    // Process-local observation; the connection's DWORD timers live outside
    // this full payload so replica promotion/demotion can retain them.
    std::uint64_t last_attack_ms{};
    std::int64_t event_time{};
    // Static definitions for learned skills, hydrated from the pinned character
    // catalog on both fresh load and transfer. Never serialized as player data.
    std::vector<SkillTemplate> skill_templates;
    std::array<bool,256> equipment_kinds{}; // learned SDT_EQUIP entries, independent of rank
    // Source-derived physical/long/magic timing (TAD 1..3). Buff-bearing
    // transfers leave this absent until active-effect timing is implemented.
    std::optional<std::array<SkillAttackTiming,3>> skill_attack_timing;
    // Absent for active-effect/companion/guild states whose stat semantics are
    // not yet implemented. Never synthesize a successful zero-filled sheet.
    std::optional<CharacterStatistics> statistics;
    // Actual GetHPR/GetMPR under the same supported graph as the stat sheet.
    // Absence is unsupported; zero is a real derived source rate.
    std::optional<std::array<std::uint32_t,2>> recovery_rates;
    // Source SetAftermath rebases this DWORD clock on real load/transfer.
    // Only bAftermath is persisted/encoded; checkpoints preserve this clock.
    std::uint32_t aftermath_tick{},aftermath_item_bonus{};
    bool revival_effect_exists{},revival_effect_supported{};
    bool avoid_effect_exists{},avoid_effect_supported{};
    std::uint32_t pure_max_hp{},pure_max_mp{}; // original GetPureMaxHP/MP cost bases
    std::vector<CharacterHotkeys> hotkeys;
    std::vector<CharacterTitle> titles;
    std::vector<CharacterCabinet> cabinets;
    std::vector<CharacterPost> posts;
    std::vector<CharacterProtected> protected_characters; // normalized SQL authority, including transfer

    // Source m_wPostRead counts UNREAD posts (TGetPostInfo bRead=0). Live
    // counters survive primary transfers independently of the mailbox DTO.
    std::optional<std::pair<std::uint16_t,std::uint16_t>> post_counts;
    std::uint32_t viewed_post{}; // CTPlayer::m_pPost selection, process-local
    std::vector<CharacterRecall> recalls;
    std::vector<CharacterPet> pets;
    std::array<std::uint16_t,4> skill_points{};
    std::uint32_t prev_exp{},next_exp{},rank_point{};
    // CTPlayer constructor state: not serialized by LOADCHAR/RELEASEMAIN.
    // Keep it for this player object's lifetime; new admission restores zero.
    std::uint8_t previous_level{};
    std::uint32_t soul_exp{};
    bool soul_ready{};
    std::array<std::uint32_t,4> lottery_numbers{};
    std::uint32_t lottery_money{}; // also player-object state, not a saved claim
    // CTPlayer live interaction gates. Their shop/trade/tournament producers
    // still need native integration; they start closed/inactive on admission.
    bool personal_store{},tournament_player{};
    // CTPlayer constructor initializes this to zero. Native arena assignment,
    // arena queues and ArenaLeave are still independent unported producers.
    std::uint16_t arena_id{};
    std::uint8_t deal_status{}; // DEAL_READY=0, WAIT=1, START=2
    std::uint8_t lucky_number{},aid_country{3}; // original absent TAIDTABLE -> TCONTRY_N
    std::uint16_t selected_title{};
};
}
