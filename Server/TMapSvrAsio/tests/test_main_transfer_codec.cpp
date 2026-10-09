#include "services/main_transfer_codec.h"
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace tr=tmapsvr::transfer;
static int checks=0;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
static std::vector<std::byte> Fixture(){
    std::ifstream f(TRANSFER_FIXTURE_PATH);std::string hex;f>>hex;
    Check(f.good()&&!hex.empty()&&hex.size()%2==0,"independent Python source fixture exists");
    std::vector<std::byte> bytes;
    for(std::size_t i=0;i<hex.size();i+=2)bytes.push_back(std::byte(std::stoul(hex.substr(i,2),nullptr,16)));
    return bytes;
}
int main(){
    try{
        const auto bytes=Fixture();auto decoded=tr::Decode(bytes);
        Check(decoded.has_value(),"complete independent source graph decodes");
        const auto& t=*decoded;const auto& s=t.character;
        Check(s.dwCharID==0xf0123456&&t.key==0x87654321&&s.szNAME==std::string("H\xe9ro",4),"identity and raw source text preserved");
        Check(s.bStartAct==1&&s.bRealSex==2&&s.bClass==3&&s.bLevel==4&&s.bRace==5&&s.bCountry==6&&s.bOriCountry==7,
              "core class and identity byte order");
        Check(s.bSex==8&&s.bHair==9&&s.bFace==10&&s.bBody==11&&s.bPants==12&&s.bHand==13&&s.bFoot==14&&s.bHelmetHide==15,
              "appearance byte order");
        Check(s.dwGold==0x80000001&&s.dwSilver==0x80000002&&s.dwCooper==0x80000003&&s.dwEXP==0x80000004&&
              s.dwHP==0x80000005&&s.dwMP==0x80000006&&s.wSkillPoint==0xabcd,"unsigned core values preserved");
        Check(s.dwRegion==0xf0000001&&s.bGuildLeave==1&&s.dwGuildLeaveTime==0xdeadbeef&&s.wMapID==50000&&
              s.wSpawnID==50001&&s.wLastSpawnID==50002&&s.dwLastDestination==0xcafebabe&&s.wTemptedMon==65000&&s.bAftermath==3,
              "region and destination fields preserved");
        Check(s.fPosX==1.25f&&s.fPosY==-2.5f&&s.fPosZ==60000.75f&&s.wDIR==32769&&s.bStatLevel==29&&s.bStatPoint==30&&s.dwStatExp==123456789,
              "position and extended progression fields preserved");
        Check(t.save_age==0xfedcba98&&t.local_id==0xaabb&&t.security_code=="ABC123"&&t.security_tries==4&&
              t.security_tick==0x87650001&&t.security_unlocked==1,"save age and non-login security layout preserved");
        Check(t.aid_country==3&&t.aid_date==-1&&t.pc_bang==1&&t.pc_bang_time==1234567&&t.pc_bang_items==8&&
              t.lucky==9&&t.post_total==400&&t.post_read==123,"aid, PC-bang and mail counters preserved");
        Check(t.bags.size()==2&&t.bags[1].bInvenID==254&&t.bags[1].wItemID==40001&&t.bags[1].dEndTime==1800000001&&
              t.cabinets.size()==1&&t.cabinets[0].id==7&&t.cabinets[0].use==1,"bags and cabinet descriptors preserved");
        Check(t.items.size()==2&&t.items[0].storage==0&&t.items[1].storage==1&&t.items[1].storage_id==7&&
              t.items[1].owner_id==0xf0123456&&t.items[1].id==0xfedcba9876543211ULL,"item storage and full 64-bit identity preserved");
        const auto& i=t.items[0];
        Check(i.item==0xbeef&&i.appearance==0xabcd&&i.companion==0x99887766&&i.slot==4&&i.level==80&&i.gem==3&&
              i.count==9&&i.grade==250&&i.durability_max==123000&&i.durability==65432&&i.refine==7&&i.expires==-2&&i.grade_effect==11,
              "complete source server item fields preserved");
        Check(i.eld==0x11223344&&i.wrap==0x22334455&&i.color==0x33445566&&i.guild==0x44556677&&i.texture==0x55667788&&
              i.magic.size()==2&&i.magic[0].id==50&&i.magic[0].value==65432&&i.magic[1].value==4321,
              "DWORD extended item fields and raw magic are not narrowed or replaced with client projections");
        Check(t.skills.size()==1&&t.skills[0].bLevel==7&&t.skills[0].wSkillID==40000&&t.skills[0].dwRemainTick==0xf0000002,
              "learned rank and remaining cooldown preserved");
        Check(t.buffs.size()==1&&t.buffs[0].skill==50001&&t.buffs[0].remaining==0xffffffff&&
              t.buffs[0].attack_id==0xabcdef00&&t.buffs[0].host_id==0xbcdef001&&t.buffs[0].attack_country==3,
              "active skill effect attribution preserved");
        Check(t.quests.size()==1&&t.quests[0].id==123456&&t.quests[0].remaining==0x80001234&&t.quests[0].triggered==2&&
              t.quest_terms.size()==1&&t.quest_terms[0].quest==123456&&t.quest_terms[0].id==87654&&t.quest_terms[0].type==7&&t.quest_terms[0].count==8,
              "quest state and term progress preserved");
        Check(t.hotkeys.size()==1&&t.hotkeys[0].save==1&&t.hotkeys[0].row.inventory==2&&t.hotkeys[0].row.keys[11].first==11&&
              t.hotkeys[0].row.keys[11].second==60011&&t.item_cooldowns.size()==1&&t.item_cooldowns[0].remaining==87654321,
              "hotkeys and shared item cooldowns preserved");
        Check(t.saddle.item==0xaabbccdd&&t.saddle.expires==-3&&t.saddle.type==2,
              "legacy saddle m_wItemID is a DWORD on the wire");
        Check(t.pets.size()==1&&t.pets[0].id==256&&t.pets[0].name==std::string("Pet\x80",4)&&t.pets[0].used_at==1800000123&&
              t.during_items.size()==1&&t.during_items[0].remaining==900000&&t.during_items[0].expires==1800000456,
              "pet and time-limited item state preserved");
        Check(t.recalls.size()==1&&t.recalls[0].id==0xf0f0f0f0&&t.recalls[0].monster==54321&&t.recalls[0].pet==400&&
              t.recalls[0].attribute==0xff00ff00&&t.recalls[0].x==61000&&t.recalls[0].z==63000&&t.recalls[0].time==456789&&
              t.recall_buffs.size()==1&&t.recall_buffs[0].recall==0xf0f0f0f0&&t.recall_buffs[0].buff.skill==50001,
              "recall graph and effects preserved");
        Check(t.protected_characters.size()==1&&t.protected_characters[0].id==0xa0a0a0a0&&t.protected_characters[0].name=="Other"&&
              t.protected_characters[0].changed==1,"protected-character state preserved");
        Check(t.pvp_available==10&&t.pvp_total==20&&t.pvp_rank==30&&t.pvp_percent==40&&t.pvp_records[5][1]==0x8000010b&&
              t.pvp_recent.size()==1&&t.pvp_recent[0].time==-4&&t.duel_records.size()==1&&t.duel_scores[5][1]==0x9000010b,
              "six-class PvP and duel history preserved");
        Check(t.auction_bids.size()==2&&t.auction_interests[0]==0xa0000001&&t.auction_registrations[1]==0xb0000002&&
              t.month_points==999999&&t.month_wins==1000&&t.month_losses==2000&&t.month_rank==300000&&t.month_percent==33,
              "auction and monthly rank state preserved");
        Check(t.titles.size()==1&&t.titles[0].id==55555&&t.titles[0].selected&&t.companions.size()==1&&
              t.companions[0].monster==0xe0000001&&t.companions[0].life==45678&&t.companions[0].name=="Companion"&&
              t.companions[0].attributes[5]==60005&&t.companions[0].bonus==8,
              "titles and complete companion attributes preserved");
        Check(t.companion_items.size()==1&&t.companion_items[0].tick==0xd0000001&&t.companion_items[0].items[1]==50001&&
              t.companion_items[0].expires[1]==1800001001&&t.companion_slots==3&&t.medals==0xf1f2f3f4&&t.rank_points==0xa1a2a3a4&&t.play_time==0xb1b2b3b4,
              "interleaved companion items and final tail preserved");
        Check(tr::Encode(t)==bytes,"encoder matches every byte of independent complete source fixture");
        bool truncated=true;
        for(std::size_t n=0;n<bytes.size();++n)truncated&=!tr::Decode(std::span(bytes).first(n));
        Check(truncated,"all 861 truncated prefixes rejected without accepting a partial graph");
        auto extra=bytes;extra.push_back(std::byte{0});Check(!tr::Decode(extra),"trailing bytes rejected");
        auto corrupt=bytes;std::fill(corrupt.begin()+10,corrupt.begin()+14,std::byte{0xff});
        Check(!tr::Decode(corrupt),"negative source string length rejected");
        for(auto offset:{0U,9U}){corrupt=bytes;corrupt[offset]=std::byte{2};Check(!tr::Decode(corrupt),"invalid transfer selector or result rejected");}
        for(auto offset:{107U,108U,124U,774U}){corrupt=bytes;corrupt[offset]=std::byte{2};Check(!tr::Decode(corrupt),"invalid login or Boolean selector rejected");}
        corrupt=bytes;corrupt[677]=std::byte{2};Check(!tr::Decode(corrupt),"duel set count cannot shift subsequent sections");
        corrupt=bytes;corrupt[145]=corrupt[146]=std::byte{0xff};Check(!tr::Decode(corrupt),"forged WORD collection count rejected");
        corrupt=bytes;corrupt[246]=std::byte{0xff};Check(!tr::Decode(corrupt),"forged raw magic BYTE count cannot consume following sections");
        tr::State oversized=t;oversized.character.szNAME.assign(65536,'x');
        bool rejected=false;try{(void)tr::Encode(oversized);}catch(const std::invalid_argument&){rejected=true;}
        Check(rejected,"oversized strings cannot wrap the original frame length");
        oversized=t;oversized.bags.resize(65536);rejected=false;
        try{(void)tr::Encode(oversized);}catch(const std::invalid_argument&){rejected=true;}
        Check(rejected,"oversized vector count cannot narrow to WORD");
        oversized=t;oversized.character.fPosX=std::numeric_limits<float>::infinity();rejected=false;
        try{(void)tr::Encode(oversized);}catch(const std::invalid_argument&){rejected=true;}
        Check(rejected,"nonfinite transferred positions rejected");
        tr::State empty;Check(tr::Decode(tr::Encode(empty)).has_value(),"empty collections retain every fixed section and boundary");
        std::printf("%d full Map transfer codec checks passed\n",checks);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"Transfer codec: %s\n",e.what());return 1;}
}
