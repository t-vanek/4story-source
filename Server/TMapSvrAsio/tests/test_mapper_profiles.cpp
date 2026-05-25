// Unit test for the TMapSvrAsio Automapper profile (CharMappingProfile).
// Pure logic — no asio, no DB. Verifies the CharRow → CharSnapshot field
// wiring + the numeric narrowing (DB int32 → uint8/16/32, double → float)
// that SociPlayerService::LoadChar now relies on.

#include "../services/mapper_profiles.h"

#include "fourstory/mapper/mapper.h"

#include <cstdio>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

} // namespace

int main()
{
    using namespace tmapsvr;
    using fourstory::mapper::Adapt;

    CharMappingProfile{}.Configure();

    // Distinct values per field so a mis-wired .Set() shows up. The int32
    // sources exercise narrowing into the snapshot's tinyint/smallint
    // widths; positions exercise double → float.
    CharRow r;
    r.char_id          = 123456;
    r.name             = "Tester";
    r.start_act        = 2;
    r.real_sex         = 1;
    r.klass            = 7;
    r.level            = 80;
    r.race             = 3;
    r.country          = 1;
    r.ori_country      = 2;
    r.sex              = 1;
    r.hair             = 11;
    r.face             = 12;
    r.body             = 13;
    r.pants            = 14;
    r.hand             = 15;
    r.foot             = 16;
    r.helmet_hide      = 1;
    r.gold             = 1000000;
    r.silver           = 5000;
    r.cooper           = 99;
    r.exp              = 250000;
    r.hp               = 4321;
    r.mp               = 1234;
    r.skill_point      = 42;
    r.region           = 777;
    r.guild_leave      = 1;
    r.guild_leave_time = 1700000000;
    r.map              = 60;
    r.spawn            = 5;
    r.last_spawn       = 4;
    r.last_dest        = 9;
    r.tempted_mon      = 250;
    r.aftermath        = 1;
    r.pos_x            = 1234.5;
    r.pos_y            = 67.25;
    r.pos_z            = -890.75;
    r.dir              = 270;
    r.stat_level       = 3;
    r.stat_point       = 6;
    r.stat_exp         = 12345;

    CharSnapshot s = Adapt<CharSnapshot>(r);

    EXPECT(s.dwCharID == 123456);
    EXPECT(s.szNAME == "Tester");
    EXPECT(s.bStartAct == 2);
    EXPECT(s.bRealSex == 1);
    EXPECT(s.bClass == 7);
    EXPECT(s.bLevel == 80);
    EXPECT(s.bRace == 3);
    EXPECT(s.bCountry == 1);
    EXPECT(s.bOriCountry == 2);
    EXPECT(s.bSex == 1);
    EXPECT(s.bHair == 11);
    EXPECT(s.bFace == 12);
    EXPECT(s.bBody == 13);
    EXPECT(s.bPants == 14);
    EXPECT(s.bHand == 15);
    EXPECT(s.bFoot == 16);
    EXPECT(s.bHelmetHide == 1);
    EXPECT(s.dwGold == 1000000);
    EXPECT(s.dwSilver == 5000);
    EXPECT(s.dwCooper == 99);
    EXPECT(s.dwEXP == 250000);
    EXPECT(s.dwHP == 4321);
    EXPECT(s.dwMP == 1234);
    EXPECT(s.wSkillPoint == 42);
    EXPECT(s.dwRegion == 777);
    EXPECT(s.bGuildLeave == 1);
    EXPECT(s.dwGuildLeaveTime == 1700000000u);
    EXPECT(s.wMapID == 60);
    EXPECT(s.wSpawnID == 5);
    EXPECT(s.wLastSpawnID == 4);
    EXPECT(s.dwLastDestination == 9);
    EXPECT(s.wTemptedMon == 250);
    EXPECT(s.bAftermath == 1);
    EXPECT(s.fPosX == 1234.5f);
    EXPECT(s.fPosY == 67.25f);
    EXPECT(s.fPosZ == -890.75f);
    EXPECT(s.wDIR == 270);
    EXPECT(s.bStatLevel == 3);
    EXPECT(s.bStatPoint == 6);
    EXPECT(s.dwStatExp == 12345);

    if (g_fails == 0)
        std::printf("test_mapper_profiles: all scenarios passed\n");
    return g_fails == 0 ? 0 : 1;
}
