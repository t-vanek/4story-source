// Smoke test for layer-1 leaf POD types ported from
// `Server/TMapSvr/TMapType.h`.
//
// The legacy types are pure data carriers — no methods, no
// invariants beyond "all fields are zero-initialized on default
// construction". This test verifies:
//
//   1. Value-initialization gives zeroed fields (legacy relies on
//      `{}` -> all-zero behavior in many places).
//   2. The struct holds the fields a future test / handler expects
//      (a typo in a member name surfaces here, not at the first
//      handler call site).
//   3. Round-trip via memcpy preserves the bytes — proves the
//      types are trivially copyable.
//
// As the layer grows, add a new test function per type. Tests run
// in <1 ms each and don't need any server infrastructure.

#include "legacy_port/types.h"

#include <cstdio>
#include <cstring>
#include <exception>
#include <type_traits>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

void TestStat()
{
    std::printf("[legacy::Stat — value-init + field access]\n");
    using tmapsvr::legacy::Stat;

    Check(std::is_trivially_copyable_v<Stat>,
        "Stat is trivially copyable");

    Stat s{};
    Check(s.str == 0 && s.dex == 0 && s.con == 0 &&
          s.intelligence == 0 && s.wis == 0 && s.men == 0,
        "Stat{} value-inits every field to zero");

    s.str = 17; s.dex = 14; s.con = 16;
    s.intelligence = 8; s.wis = 13; s.men = 10;

    Stat copy = s;
    Check(std::memcmp(&s, &copy, sizeof(Stat)) == 0,
        "memcpy round-trip preserves bytes");

    Check(copy.intelligence == 8 && copy.wis == 13,
        "field values survive copy");
}

void TestSpawnPos()
{
    std::printf("[legacy::SpawnPos — value-init + field access]\n");
    using tmapsvr::legacy::SpawnPos;

    Check(std::is_trivially_copyable_v<SpawnPos>,
        "SpawnPos is trivially copyable");

    SpawnPos p{};
    Check(p.id == 0 && p.map_id == 0 && p.type == 0 &&
          p.pos_x == 0.0f && p.pos_y == 0.0f && p.pos_z == 0.0f,
        "SpawnPos{} value-inits every field to zero");

    p.id = 15003; p.map_id = 2010; p.type = 1;
    p.pos_x = 3664.405f; p.pos_y = 86.16578f; p.pos_z = 557.2542f;

    Check(p.pos_x > 3664.0f && p.pos_x < 3665.0f,
        "float field survives roundtrip with expected precision");
}

void TestLevel()
{
    std::printf("[legacy::Level — value-init + field access]\n");
    using tmapsvr::legacy::Level;

    Check(std::is_trivially_copyable_v<Level>,
        "Level is trivially copyable");

    Level l{};
    Check(l.level == 0 && l.exp == 0 && l.hp == 0 && l.mp == 0 &&
          l.skill_point == 0 && l.money == 0 && l.reg_cost == 0 &&
          l.search_cost == 0 && l.gamble_cost == 0 && l.rep_cost == 0 &&
          l.repair_cost == 0 && l.refine_cost == 0 &&
          l.pv_point == 0 && l.pvp_money == 0,
        "Level{} value-inits all 14 fields to zero");

    l.level = 50; l.hp = 5000; l.mp = 3000; l.skill_point = 3;
    Level copy = l;
    Check(copy.level == 50 && copy.hp == 5000 && copy.skill_point == 3,
        "Level copy preserves chart-row values");
}

void TestAiBuf()
{
    std::printf("[legacy::AiBuf — value-init + field access]\n");
    using tmapsvr::legacy::AiBuf;

    Check(std::is_trivially_copyable_v<AiBuf>,
        "AiBuf is trivially copyable");

    AiBuf b{};
    Check(b.cmd_handle == 0 && b.event_host == 0 && b.rh_id == 0 &&
          b.rh_type == 0 && b.host_key == 0 && b.mon_id == 0 &&
          b.delay == 0 && b.tick == 0 &&
          b.channel == 0 && b.map_id == 0 && b.party_id == 0,
        "AiBuf{} value-inits all 11 fields to zero");
}

void TestInvenDesc()
{
    std::printf("[legacy::InvenDesc — value-init + field access]\n");
    using tmapsvr::legacy::InvenDesc;

    Check(std::is_trivially_copyable_v<InvenDesc>,
        "InvenDesc is trivially copyable");

    InvenDesc d{};
    Check(d.inven_id == 0 && d.item_id == 0,
        "InvenDesc{} value-inits both fields to zero");

    d.inven_id = 3; d.item_id = 1042;
    Check(d.inven_id == 3 && d.item_id == 1042,
        "fields hold assigned values");
}

void TestTtnmtInven()
{
    std::printf("[legacy::TtnmtInven — value-init + field access]\n");
    using tmapsvr::legacy::TtnmtInven;

    Check(std::is_trivially_copyable_v<TtnmtInven>,
        "TtnmtInven is trivially copyable");

    TtnmtInven t{};
    Check(t.inven_id == 0 && t.item_id == 0 && t.eld == 0 &&
          t.end_time_unix == 0,
        "TtnmtInven{} value-inits all 4 fields to zero");

    t.inven_id = 1; t.item_id = 5001; t.eld = 2;
    t.end_time_unix = 1716120000;   // arbitrary unix ts
    Check(t.end_time_unix == 1716120000,
        "int64 end_time_unix holds large unix timestamp");
}

void TestSkillData()
{
    std::printf("[legacy::SkillData — value-init + field access]\n");
    using tmapsvr::legacy::SkillData;

    Check(std::is_trivially_copyable_v<SkillData>,
        "SkillData is trivially copyable");

    SkillData s{};
    Check(s.action == 0 && s.type == 0 && s.attr == 0 && s.exec == 0 &&
          s.inc == 0 && s.value == 0 && s.value_inc == 0 && s.calc == 0,
        "SkillData{} value-inits all 8 fields to zero");
}

void TestQuestTerm()
{
    std::printf("[legacy::QuestTerm — value-init + field access]\n");
    using tmapsvr::legacy::QuestTerm;

    Check(std::is_trivially_copyable_v<QuestTerm>,
        "QuestTerm is trivially copyable");

    QuestTerm q{};
    Check(q.term_id == 0 && q.term_type == 0 && q.count == 0,
        "QuestTerm{} value-inits all 3 fields to zero");

    q.term_id = 1024; q.term_type = 3; q.count = 5;
    QuestTerm copy = q;
    Check(copy.term_id == 1024 && copy.term_type == 3 && copy.count == 5,
        "copy preserves quest-term values");
}

void TestAftermath()
{
    std::printf("[legacy::Aftermath — value-init + field access]\n");
    using tmapsvr::legacy::Aftermath;

    Check(std::is_trivially_copyable_v<Aftermath>,
        "Aftermath is trivially copyable");

    Aftermath a{};
    Check(a.step == 0 && a.tick == 0 &&
          a.stat_dec == 0.0f && a.reuse_inc == 0.0f,
        "Aftermath{} value-inits all 4 fields to zero");

    a.step = 12;          // legacy AFTERMATH_HELP
    a.tick = 5000;
    a.stat_dec  = 0.5f;
    a.reuse_inc = 1.5f;
    Check(a.step == 12 && a.tick == 5000,
        "step + tick fields hold assigned values");
    Check(a.stat_dec > 0.49f && a.stat_dec < 0.51f,
        "float stat_dec survives roundtrip");
}

void TestPortal()
{
    std::printf("[legacy::Portal — value-init + field access]\n");
    using tmapsvr::legacy::Portal;

    Check(std::is_trivially_copyable_v<Portal>,
        "Portal is trivially copyable");

    Portal p{};
    Check(p.portal_id == 0 && p.local_id == 0 && p.country == 0 &&
          p.spawn_id == 0 && p.condition == 0 && p.condition_id == 0,
        "Portal{} value-inits all 6 fields to zero");

    p.portal_id = 200; p.spawn_id = 15003; p.country = 1;
    p.condition = 2; p.condition_id = 0x4242;
    Portal copy = p;
    Check(copy.portal_id == 200 && copy.spawn_id == 15003 &&
          copy.condition_id == 0x4242,
        "copy preserves portal-table values");
}

void TestCharAppearance()
{
    std::printf("[legacy::CharAppearance — value-init + field access]\n");
    using tmapsvr::legacy::CharAppearance;

    Check(std::is_trivially_copyable_v<CharAppearance>,
        "CharAppearance is trivially copyable");

    CharAppearance a{};
    Check(a.race == 0 && a.sex == 0 && a.real_sex == 0 &&
          a.char_class == 0 && a.hair == 0 && a.face == 0 &&
          a.body == 0 && a.pants == 0 && a.hand == 0 &&
          a.foot == 0 && a.helmet_hide == 0 &&
          a.country == 0 && a.ori_country == 0 && a.start_act == 0,
        "CharAppearance{} value-inits all 14 fields to zero");

    a.race       = 2;
    a.char_class = 3;
    a.hair       = 7;
    a.face       = 4;
    a.country    = 1;
    CharAppearance copy = a;
    Check(std::memcmp(&a, &copy, sizeof(CharAppearance)) == 0,
        "memcpy round-trip preserves all appearance bytes");
    Check(copy.race == 2 && copy.char_class == 3 &&
          copy.hair == 7 && copy.face == 4 && copy.country == 1,
        "copy preserves visual identity fields");
}

void TestCharPosition()
{
    std::printf("[legacy::CharPosition — value-init + field access]\n");
    using tmapsvr::legacy::CharPosition;

    Check(std::is_trivially_copyable_v<CharPosition>,
        "CharPosition is trivially copyable");

    CharPosition p{};
    Check(p.map_id == 0 && p.spawn_id == 0 && p.last_spawn_id == 0 &&
          p.last_destination == 0 && p.region == 0 &&
          p.pos_x == 0.0f && p.pos_y == 0.0f && p.pos_z == 0.0f &&
          p.dir == 0,
        "CharPosition{} value-inits all 9 fields to zero");

    p.map_id  = 201;
    p.pos_x   = 3664.405f;
    p.pos_y   = 86.16578f;
    p.pos_z   = 557.2542f;
    p.dir     = 2048;
    p.region  = 0x00030001u;

    CharPosition copy = p;
    Check(copy.map_id == 201 && copy.dir == 2048,
        "copy preserves map_id + dir");
    Check(copy.pos_x > 3664.0f && copy.pos_x < 3665.0f,
        "float pos_x survives roundtrip with expected precision");
    Check(copy.region == 0x00030001u,
        "copy preserves region composite id");
}

void TestHotKey()
{
    std::printf("[legacy::HotKey — value-init + field access]\n");
    using tmapsvr::legacy::HotKey;

    Check(std::is_trivially_copyable_v<HotKey>,
        "HotKey is trivially copyable");
    Check(sizeof(HotKey) == 4,
        "HotKey is exactly 4 bytes (BYTE + BYTE + WORD)");

    HotKey h{};
    Check(h.pos == 0 && h.type == 0 && h.id == 0,
        "HotKey{} value-inits all 3 fields to zero");

    h.pos  = 5;
    h.type = 1;  // item slot
    h.id   = 1042;
    HotKey copy = h;
    Check(copy.pos == 5 && copy.type == 1 && copy.id == 1042,
        "copy preserves hotkey slot values");
}

void TestInvenItem()
{
    std::printf("[legacy::InvenItem — value-init + field access]\n");
    using tmapsvr::legacy::InvenItem;

    Check(std::is_trivially_copyable_v<InvenItem>,
        "InvenItem is trivially copyable");

    InvenItem i{};
    Check(i.inven_id == 0 && i.item_id == 0 &&
          i.end_time == 0 && i.eld == 0,
        "InvenItem{} value-inits all 4 fields to zero");

    i.inven_id = 5;
    i.item_id  = 1042;
    i.end_time = 1716120000LL;  // a unix timestamp
    i.eld      = 3;
    InvenItem copy = i;
    Check(copy.inven_id == 5 && copy.item_id == 1042 && copy.eld == 3,
        "copy preserves slot/item/eld fields");
    Check(copy.end_time == 1716120000LL,
        "int64 end_time survives roundtrip");
}

void TestActiveSkill()
{
    std::printf("[legacy::ActiveSkill — value-init + field access]\n");
    using tmapsvr::legacy::ActiveSkill;

    Check(std::is_trivially_copyable_v<ActiveSkill>,
        "ActiveSkill is trivially copyable");

    ActiveSkill s{};
    Check(s.skill_id == 0 && s.level == 0 && s.reuse_tick == 0,
        "ActiveSkill{} value-inits all 3 fields to zero");

    s.skill_id   = 201;
    s.level      = 5;
    s.reuse_tick = 3000;
    ActiveSkill copy = s;
    Check(copy.skill_id == 201 && copy.level == 5 &&
          copy.reuse_tick == 3000,
        "copy preserves skill id + level + cooldown");
}

void TestMaintainSkill()
{
    std::printf("[legacy::MaintainSkill — value-init + field access]\n");
    using tmapsvr::legacy::MaintainSkill;

    Check(std::is_trivially_copyable_v<MaintainSkill>,
        "MaintainSkill is trivially copyable");

    MaintainSkill m{};
    Check(m.skill_id == 0 && m.level == 0 && m.remain_tick == 0,
        "MaintainSkill{} value-inits all 3 fields to zero");

    m.skill_id    = 105;
    m.level       = 3;
    m.remain_tick = 15000;
    MaintainSkill copy = m;
    Check(copy.skill_id == 105 && copy.level == 3 &&
          copy.remain_tick == 15000,
        "copy preserves buff id + level + remaining time");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  legacy_port layer-1 spec ===\n\n");
    try
    {
        TestStat();
        TestSpawnPos();
        TestLevel();
        TestAiBuf();
        TestInvenDesc();
        TestTtnmtInven();
        TestSkillData();
        TestQuestTerm();
        TestAftermath();
        TestPortal();
        TestCharAppearance();
        TestCharPosition();
        TestHotKey();
        TestInvenItem();
        TestActiveSkill();
        TestMaintainSkill();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
