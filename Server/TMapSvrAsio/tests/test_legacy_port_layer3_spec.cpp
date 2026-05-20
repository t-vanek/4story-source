// Spec test for layer-3 aggregate types.
//
// Layer-3 types hold std::string + std::vector, so they are not
// trivially copyable. Tests verify:
//   1. Value-init gives zeroed scalars and empty containers/strings.
//   2. Embedded layer-1 aggregates (CharAppearance, CharPosition)
//      also value-init cleanly.
//   3. vector + string members grow and survive copy.
//   4. Move leaves source in a valid (empty) state.

#include "legacy_port/types_layer3.h"

#include <cstdio>
#include <exception>
#include <type_traits>
#include <utility>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

void TestCharSnapshot()
{
    std::printf("[legacy::CharSnapshot — value-init + copy/move]\n");
    using tmapsvr::legacy::CharSnapshot;
    using tmapsvr::legacy::InvenItem;
    using tmapsvr::legacy::ActiveSkill;
    using tmapsvr::legacy::MaintainSkill;

    Check(std::is_default_constructible_v<CharSnapshot>,
        "CharSnapshot is default-constructible");
    Check(std::is_copy_constructible_v<CharSnapshot>,
        "CharSnapshot is copy-constructible");
    Check(std::is_move_constructible_v<CharSnapshot>,
        "CharSnapshot is move-constructible");

    // Value-init
    CharSnapshot s{};
    Check(s.char_id == 0 && s.user_id == 0 && s.dw_key == 0,
        "CharSnapshot{} value-inits ids to zero");
    Check(s.name.empty(),
        "CharSnapshot{} name is empty string");
    Check(s.level == 0 && s.exp == 0 && s.hp == 0 && s.mp == 0 &&
          s.skill_point == 0,
        "CharSnapshot{} zeroes progression fields");
    Check(s.gold == 0 && s.silver == 0 && s.copper == 0,
        "CharSnapshot{} zeroes economy fields");
    Check(s.stat_level == 0 && s.stat_point == 0 && s.stat_exp == 0,
        "CharSnapshot{} zeroes stat-allocation fields");
    Check(s.guild_leave == 0 && s.guild_leave_time == 0 &&
          s.tempted_mon == 0 && s.aftermath == 0,
        "CharSnapshot{} zeroes social/death fields");
    Check(s.inventory.empty() && s.skills.empty() &&
          s.maintain_skills.empty(),
        "CharSnapshot{} leaves all vectors empty");

    // Embedded CharAppearance
    Check(s.appearance.race == 0 && s.appearance.char_class == 0 &&
          s.appearance.hair == 0 && s.appearance.face == 0 &&
          s.appearance.country == 0,
        "CharSnapshot{} zeroes embedded CharAppearance");

    // Embedded CharPosition
    Check(s.position.map_id == 0 && s.position.pos_x == 0.0f &&
          s.position.pos_z == 0.0f && s.position.dir == 0,
        "CharSnapshot{} zeroes embedded CharPosition");

    // Populate
    s.char_id = 123456u;
    s.user_id = 654321u;
    s.dw_key  = 0xDEADBEEFu;
    s.name    = "HeroName";
    s.level   = 60;
    s.exp     = 99000u;
    s.hp      = 10000u;
    s.mp      = 5000u;
    s.gold    = 1000000u;
    s.appearance.race       = 1;
    s.appearance.char_class = 3;
    s.appearance.hair       = 7;
    s.position.map_id = 201;
    s.position.pos_x  = 3664.0f;
    s.position.pos_z  = 557.0f;
    s.position.dir    = 2048;

    InvenItem item{};
    item.inven_id = 5;
    item.item_id  = 1042;
    item.end_time = 1716120000LL;
    item.eld      = 3;
    s.inventory.push_back(item);

    ActiveSkill sk{};
    sk.skill_id   = 201;
    sk.level      = 5;
    sk.reuse_tick = 3000u;
    s.skills.push_back(sk);

    MaintainSkill buff{};
    buff.skill_id    = 105;
    buff.level       = 2;
    buff.remain_tick = 8000u;
    s.maintain_skills.push_back(buff);

    // Copy preserves everything
    CharSnapshot copy = s;
    Check(copy.char_id == 123456u && copy.user_id == 654321u &&
          copy.dw_key == 0xDEADBEEFu,
        "copy preserves ids");
    Check(copy.name == "HeroName" && copy.level == 60,
        "copy preserves name + level");
    Check(copy.appearance.race == 1 && copy.appearance.char_class == 3 &&
          copy.appearance.hair == 7,
        "copy preserves embedded CharAppearance");
    Check(copy.position.map_id == 201 && copy.position.dir == 2048,
        "copy preserves embedded CharPosition");
    Check(copy.inventory.size() == 1 &&
          copy.inventory[0].item_id == 1042 &&
          copy.inventory[0].end_time == 1716120000LL,
        "copy preserves inventory vector");
    Check(copy.skills.size() == 1 &&
          copy.skills[0].skill_id == 201 &&
          copy.skills[0].reuse_tick == 3000u,
        "copy preserves skills vector");
    Check(copy.maintain_skills.size() == 1 &&
          copy.maintain_skills[0].skill_id == 105 &&
          copy.maintain_skills[0].remain_tick == 8000u,
        "copy preserves maintain_skills vector");

    // Move takes ownership
    CharSnapshot moved = std::move(s);
    Check(moved.char_id == 123456u && moved.name == "HeroName",
        "move takes ownership of scalar + string content");
    Check(moved.inventory.size() == 1 && moved.skills.size() == 1 &&
          moved.maintain_skills.size() == 1,
        "move takes ownership of all vectors");

    // Moved-from variable is safely re-assignable
    s = CharSnapshot{};
    Check(s.name.empty() && s.inventory.empty() && s.char_id == 0,
        "moved-from variable safely reset by re-assignment");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  legacy_port layer-3 spec ===\n\n");
    try
    {
        TestCharSnapshot();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
