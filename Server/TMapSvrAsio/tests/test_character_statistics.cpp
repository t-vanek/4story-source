#include "services/character_statistics.h"
#include "services/client_senders.h"
#include <iostream>
#include <limits>
#include <string>
using namespace tmapsvr;
int main() {
    unsigned checks=0;
    const auto check=[&](bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;};
    character_statistics::ItemAttributes item;
    item.Add(1,2,8,4,9,3,5);item.Add(4,6,11,99,99,7,13);item.Add(6,99,99,99,99,99,99);
    check(item.min_physical==2&&item.max_physical==8&&item.min_ranged==6&&item.max_ranged==11,"separate melee/ranged attributes; shield excluded");
    check(item.min_magic==4&&item.max_magic==9&&item.defense==10&&item.magic_defense==18,"only melee weapon adds magical attack; both non-shields add defense");
    std::array<FormulaRow,35> f{};
    for(auto& row:f)row={3,1.5f,1.f};
    f[6].fRateY=40;f[29].fRateY=60;f[17]={0,3,1};f[24].dwInit=300;
    std::array<std::uint32_t,6> unscaled{10,20,30,40,50,60};
    auto make=[&](bool clamp){return character_statistics::Build(20,2,unscaled,item,std::array<SkillAttackTiming,3>{{{1,91},{2,92},{3,93}}},
        [&](unsigned i){return f.at(i);},[&](unsigned i,std::uint16_t floor){return float(std::max(unscaled[i],std::uint32_t(floor)))+.75f;},
        [](std::uint32_t,unsigned type){return type==8?-100:2;},[&](unsigned type){return clamp&&type==61?100U:1U;});};
    auto v=make(false);
    // Hand calculation from TObjBase (float product, truncate BEFORE the init
    // term; percentage floors only DL/MDL, critical uses ungrown base stats).
    check(v.primary==std::array<std::uint16_t,6>{10,20,30,40,50,60},"six WORD truncations");
    check(v.min_physical==25&&v.max_physical==31&&v.min_ranged==44&&v.max_ranged==49,"separate source AP/LAP min/max formulas");
    check(v.min_magic==72&&v.max_magic==77,"source magic attack uses INT");
    check(v.physical_defense==0&&v.magic_defense==25,"signed ability delta clamps to zero");
    check(v.attack_level==37&&v.defense_level==64&&v.magic_attack_level==82&&v.magic_defense_level==94,"defense-level floors precede growth and do not add formula init");
    check(v.physical_critical==36&&v.magic_critical==81&&v.charge_speed==47&&v.charge_probability==182,"critical, charge and BYTE narrowing");
    check(make(true).min_physical==31,"minimum attack cannot exceed maximum");
    bool invalid=false;try{character_statistics::Truncate(std::numeric_limits<double>::infinity());}catch(const std::domain_error&){invalid=true;}
    check(invalid,"nonfinite chart result cannot become a successful stat sheet");
    CharSnapshot s;s.dwCharID=0x04030201;s.wSkillPoint=0x5857;s.bAftermath=0x59;
    CharacterStatistics wire;wire.primary={0x0605,0x0807,0x0a09,0x0c0b,0x0e0d,0x100f};
    wire.min_physical=0x14131211;wire.max_physical=0x18171615;wire.physical_defense=0x1c1b1a19;
    wire.min_ranged=0x201f1e1d;wire.max_ranged=0x24232221;
    wire.timing={{{0x28272625,0x34333231},{0x2c2b2a29,0x38373635},{0x302f2e2d,0x3c3b3a39}}};
    wire.attack_level=0x3e3d;wire.defense_level=0x403f;wire.physical_critical=0x41;
    wire.min_magic=0x45444342;wire.max_magic=0x49484746;wire.magic_defense=0x4d4c4b4a;
    wire.magic_attack_level=0x4f4e;wire.magic_defense_level=0x5150;
    wire.charge_speed=0x52;wire.charge_probability=0x53;wire.magic_critical=0x54;
    // Client CSHandler.cpp:6620 consumes 87 bytes. Assign the last three
    // fields 0x55..0x57 for a monotone byte oracle independent of our writer.
    s.wSkillPoint=0x5655;s.bAftermath=0x57;
    auto bytes=EncodeCharacterStatistics(s,wire);check(bytes.size()==87,"exact original body length");
    for(unsigned i=0;i<bytes.size();++i)check(bytes[i]==std::byte(i+1),"original byte order/width/endian oracle");
    std::cout<<"Character statistics: "<<checks<<" checks passed\n";
}
