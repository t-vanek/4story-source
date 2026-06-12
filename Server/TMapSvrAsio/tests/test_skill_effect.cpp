// Skill-effect engine tests — verifies skill_effect.h against the legacy
// CTSkillTemp::GetValue / Calculate / CalcValue (TSkillTemp.cpp:49-105)
// and the SCT_HP/MP cure roll (TObjBase.cpp:3555), plus the
// CS_SKILLUSE_ACK byte layout (CSSender.cpp:1518).

#include "services/skill_effect.h"
#include "services/skill_data_chart.h"
#include "services/client_senders.h"
#include "wire_codec.h"

#include <cmath>
#include <cstdint>
#include <iostream>

using namespace tmapsvr;

namespace {

int g_fail = 0;
void Check(bool ok, const char* what)
{
    if (!ok) { std::cerr << "FAIL: " << what << "\n"; ++g_fail; }
}

SkillDataRow Row(std::uint8_t calc, std::uint16_t value,
                 std::uint16_t value_inc = 0,
                 std::uint8_t inc = SVI_INCREASE,
                 std::uint8_t type = SDT_CURE,
                 std::uint8_t exec = SCT_HP)
{
    SkillDataRow d;
    d.bCalc = calc; d.wValue = value; d.wValueInc = value_inc;
    d.bInc = inc; d.bType = type; d.bExec = exec;
    return d;
}

SkillTemplate Tmpl(std::uint8_t start = 1, std::uint8_t next = 0,
                   float f1st = 1.03f)
{
    SkillTemplate t;
    t.wID = 7; t.bStartLevel = start; t.bNextLevel = next;
    t.f1stRateX = f1st;
    return t;
}

} // namespace

int main()
{
    const auto t = Tmpl();

    // --- GetValue calc modes (TSkillTemp.cpp:62) ----------------------
    {
        Check(skill_effect::GetValue(Row(0, 250), t, 5) == 250,
              "calc0 constant");
        Check(skill_effect::GetValue(Row(1, 100, 20), t, 1) == 100,
              "calc1 linear at lvl1");
        Check(skill_effect::GetValue(Row(1, 100, 20), t, 4) == 160,
              "calc1 linear at lvl4");
        Check(skill_effect::GetValue(Row(3, 100, 30), t, 4) == 10,
              "calc3 falling");
        Check(skill_effect::GetValue(Row(3, 100, 30), t, 6) == -50,
              "calc3 goes negative (signed)");

        // calc2 exponential: wValue * pow(f1st, start+(lvl-1)*next) / 100.
        const auto t2 = Tmpl(/*start=*/10, /*next=*/5, /*f1st=*/1.03f);
        const int expect =
            static_cast<int>((4000 * std::pow(1.03f, 10 + 2 * 5)) / 100.0);
        Check(skill_effect::GetValue(Row(2, 4000), t2, 3) == expect,
              "calc2 exponential vs hand pow");
        Check(skill_effect::GetValue(Row(2, 4000), t2, 0) == 40,
              "calc2 lvl0 quirk -> exponent 0 (wValue/100)");
    }

    // --- Calculate SVI_ combine modes (TSkillTemp.cpp:77) -------------
    {
        Check(skill_effect::Calculate(Row(0, 250), t, 1, 1000) == 250,
              "SVI_INCREASE passes value");
        Check(skill_effect::Calculate(
                  Row(0, 250, 0, SVI_DECREASE), t, 1, 1000) == -250,
              "SVI_DECREASE negates");
        Check(skill_effect::Calculate(
                  Row(0, 3, 0, SVI_MULTIPLY), t, 1, 1000) == 2000,
              "SVI_MULTIPLY returns delta (base*v - base)");
        Check(skill_effect::Calculate(
                  Row(0, 4, 0, SVI_DIVIDE), t, 1, 1000) == -750,
              "SVI_DIVIDE returns delta (base/v - base)");
        Check(skill_effect::Calculate(
                  Row(0, 0, 0, SVI_DIVIDE), t, 1, 1000) == 0,
              "SVI_DIVIDE guards divisor <= 0 to 1");
        Check(skill_effect::Calculate(
                  Row(0, 30, 0, SVI_PRECENT), t, 1, 1000) == -700,
              "SVI_PRECENT returns delta (30% of base - base)");
    }

    // --- CalcValue sums matching rows only -----------------------------
    {
        std::vector<SkillDataRow> rows = {
            Row(0, 100),                                          // HP cure
            Row(0, 40, 0, SVI_INCREASE, SDT_CURE, SCT_MP),        // MP cure
            Row(0, 999, 0, SVI_INCREASE, SDT_ABILITY, SCT_HP),    // not cure
            Row(1, 10, 5),                                        // HP cure
        };
        Check(skill_effect::CalcValue(rows, t, 3, SDT_CURE, SCT_HP, 500)
                  == 100 + 20,
              "CalcValue sums HP-cure rows (100 + linear 20)");
        Check(skill_effect::CalcValue(rows, t, 3, SDT_CURE, SCT_MP, 500)
                  == 40,
              "CalcValue picks MP-cure row only");
    }

    // --- CureRoll variance: value + value*(rand%16)/100 ----------------
    {
        auto lo = [](std::uint32_t) { return 0u; };
        auto hi = [](std::uint32_t) { return 15u; };
        Check(skill_effect::CureRoll(200, lo) == 200, "cure roll min = value");
        Check(skill_effect::CureRoll(200, hi) == 230, "cure roll max = +15%");
        Check(skill_effect::CureRoll(0, hi) == 0,     "zero cure stays zero");
    }

    // --- CS_SKILLUSE_ACK byte layout (CSSender.cpp:1518) ---------------
    {
        SkillUseAckFields f;
        f.result = 7;            // SKILL_NEEDMP
        f.attack_id = 0xA1B2C3D4; f.attack_type = 1; f.skill_id = 0x1234;
        f.back_skill = 0x5678; f.action_id = 9; f.act_id = 0x11223344;
        f.ani_id = 0x55667788; f.skill_level = 3; f.attack_level = 0x0A0B;
        f.attacker_level = 42; f.pys_min_power = 1; f.pys_max_power = 2;
        f.mg_min_power = 3; f.mg_max_power = 4; f.trans_hp = 5;
        f.trans_mp = 6; f.curse_prob = 7; f.equip_special = 8;
        f.can_select = 1; f.country = 2; f.aid_country = 3; f.cp = 99;
        f.gnd_x = 1.5f; f.gnd_y = -2.5f; f.gnd_z = 3.5f;

        const std::vector<SkillTarget> targets = {
            { 0xDEADBEEF, 2 }, { 0xCAFEF00D, 1 } };
        auto b = EncodeSkillUseAck(f, targets);
        Check(b.size() == 62 + 2 * 5, "fat ack size 72");

        wire::Reader r(b.data(), b.size());
        std::uint8_t  u8 = 0;  std::uint16_t u16 = 0;
        std::uint32_t u32 = 0; float f32 = 0;
        Check(r.Read(u8)  && u8  == 7,          "result");
        Check(r.Read(u32) && u32 == 0xA1B2C3D4, "attack id");
        Check(r.Read(u8)  && u8  == 1,          "attack type");
        Check(r.Read(u16) && u16 == 0x1234,     "skill id");
        Check(r.Read(u16) && u16 == 0x5678,     "back skill after skill id");
        Check(r.Read(u8)  && u8  == 9,          "action id");
        Check(r.Read(u32) && u32 == 0x11223344, "act id");
        Check(r.Read(u32) && u32 == 0x55667788, "ani id");
        Check(r.Read(u8)  && u8  == 3,          "skill level");
        Check(r.Read(u16) && u16 == 0x0A0B,     "attack level");
        Check(r.Read(u8)  && u8  == 42,         "attacker level");
        Check(r.Read(u32) && u32 == 1, "pys min");
        Check(r.Read(u32) && u32 == 2, "pys max");
        Check(r.Read(u32) && u32 == 3, "mg min");
        Check(r.Read(u32) && u32 == 4, "mg max");
        Check(r.Read(u16) && u16 == 5, "trans hp");
        Check(r.Read(u16) && u16 == 6, "trans mp");
        Check(r.Read(u8) && u8 == 7,  "curse prob");
        Check(r.Read(u8) && u8 == 8,  "equip special");
        Check(r.Read(u8) && u8 == 1,  "can select");
        Check(r.Read(u8) && u8 == 2,  "country");
        Check(r.Read(u8) && u8 == 3,  "aid country");
        Check(r.Read(u8) && u8 == 99, "cp");
        Check(r.Read(f32) && f32 == 1.5f,  "gnd x");
        Check(r.Read(f32) && f32 == -2.5f, "gnd y");
        Check(r.Read(f32) && f32 == 3.5f,  "gnd z");
        Check(r.Read(u8) && u8 == 2, "target count");
        Check(r.Read(u32) && u32 == 0xDEADBEEF, "target 0 id");
        Check(r.Read(u8)  && u8  == 2,          "target 0 type");
        Check(r.Read(u32) && u32 == 0xCAFEF00D, "target 1 id");
        Check(r.Read(u8)  && u8  == 1,          "target 1 type");
        Check(r.Eof(), "no trailing bytes");

        // Reject form: defaults + empty list — 62 bytes, count 0.
        SkillUseAckFields rej;
        rej.result = 6;   // SKILL_SPEEDYUSE
        auto rb = EncodeSkillUseAck(rej, {});
        Check(rb.size() == 62, "reject ack fixed size 62");
        Check(static_cast<std::uint8_t>(rb[0]) == 6, "reject result byte");
        Check(static_cast<std::uint8_t>(rb[61]) == 0, "reject count 0");
    }

    // --- InMemory skill-data chart grouping ----------------------------
    {
        InMemorySkillDataChart chart;
        chart.Add(7, Row(0, 100));
        chart.Add(7, Row(0, 40, 0, SVI_INCREASE, SDT_CURE, SCT_MP));
        chart.Add(8, Row(0, 1));
        Check(chart.ForSkill(7).size() == 2, "two rows for skill 7");
        Check(chart.ForSkill(8).size() == 1, "one row for skill 8");
        Check(chart.ForSkill(9).empty(),     "unknown skill empty");
        Check(chart.Size() == 3,             "total row count");
    }

    if (g_fail == 0)
        std::cout << "test_skill_effect: all checks passed\n";
    return g_fail == 0 ? 0 : 1;
}
