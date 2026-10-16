#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace tmapsvr {

// Source TMapType.h::LOCAL_TYPE, distinct from World's BATTLE_TYPE.
enum class LocalType : std::uint8_t { Occupation=1, Castle=2, Mission=3, SkyGarden=4 };

struct BattleZone {
    std::uint16_t id{},map{},castle{},boss{},left_keeper{},right_keeper{},center_keeper{};
    std::uint16_t valorian_defend{},valorian_attack{},derion_defend{},derion_attack{};
    std::uint16_t middle_spawn{},right_spawn{},left_spawn{},skill1{},skill2{};
    std::uint32_t left_switch{},right_switch{},center_switch{};
    std::uint8_t line{},item_level{};
    std::string name;
    // Missing item templates are absent, even if the requested ID was zero.
    std::optional<std::uint16_t> normal_item,chief_item;
    std::vector<std::uint16_t> spawns;
};
struct OccupationDay {std::uint32_t source_guild{};std::uint8_t type{};};
struct BattleRegion {
    LocalType type{};
    std::uint16_t id{};
    std::uint8_t country{},status{};
    bool valid{},can_battle{};
    // Original source identities only. Never native guild authority.
    std::uint32_t source_guild{};
    std::optional<std::string> guild_name;
    std::string hero;
    std::int64_t hero_second{},next_defend_second{};
    std::optional<std::int64_t> occupied_second;
    // Source leaves several scalars uninitialized for some local types. Keep
    // that absence explicit; future handlers may not read an invented zero.
    std::optional<bool> occupied,gate_opened;
    std::optional<std::uint8_t> last_occupy_type;
    std::array<std::optional<OccupationDay>,7> occupation_days;
    std::optional<std::uint32_t> attack_guild,defend_guild;
    std::optional<std::uint16_t> defend_point,contribution_point;
    std::optional<std::uint8_t> defend_count,attack_count,defend_country;
    std::optional<std::array<std::uint8_t,3>> camp_owners;
    std::set<std::uint16_t> locals;
};
struct BattleMapEnvironment {
    std::set<std::uint16_t> units;
    bool instance_template{};
    std::optional<std::uint16_t> castle,skygarden;
};

// Owned by the existing PostgreSQL Map service, immutable after startup in
// this slice. No raw legacy pointers or detached/global second implementation.
struct BattleRegions {
    std::map<std::uint16_t,BattleZone> zones;
    std::map<std::uint16_t,BattleRegion> locals;
    std::map<std::uint32_t,BattleMapEnvironment> maps; // MAKELONG(map,physical channel)
    std::array<std::vector<std::uint16_t>,4> order;
    std::string manifest,order_sha256,content_manifest,source_timezone;
    std::int64_t run_id{};
    std::vector<std::byte> HeroList() const;
    bool CanTalk(std::uint8_t country,std::uint8_t aid_country,std::uint8_t disguise,
                 std::uint8_t npc_country,std::uint16_t npc_local) const;
};
} // namespace tmapsvr
