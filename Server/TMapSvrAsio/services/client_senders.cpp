#include "services/client_senders.h"
#include "services/maintained_effects.h"
#include "services/skill_cooldown.h"

#include "wire_codec.h"
#include <stdexcept>
#include <ctime>
#include <cstdio>

namespace tmapsvr {
namespace {
void WritePlayerEffects(std::vector<std::byte>& b,const CharacterPayload* p) {
    auto effects=p?MaintainedEffects(*p):std::vector<MaintainedEffect>{};
    // Source LOADCHAR discards STATUS_PCBANG visuals before CHARINFO/ENTER;
    // EnterMAP recreates903 from the independently loaded entitlement. Keep
    // the old durable row only until the entry transaction can replace it.
    if(p&&p->during&&p->during->pending_entry)std::erase_if(effects,[](const auto& e){return e.skill==903;});
    if(effects.size()>255)throw std::length_error("Player effect count exceeds BYTE");
    wire::WritePOD<std::uint8_t>(b,static_cast<std::uint8_t>(effects.size()));
    const auto now=SkillClockMs();
    for(auto& e:effects) {
        // CSSender samples GetRemainTick at serialization, independently of
        // the last simulation/DB tick. This never changes durable membership.
        if(e.remaining&&e.sampled_ms&&now>=e.sampled_ms)
            e.remaining=now-e.sampled_ms>=e.remaining?0:e.remaining-static_cast<std::uint32_t>(now-e.sampled_ms);
        wire::WritePOD(b,e.skill);wire::WritePOD(b,e.level);wire::WritePOD(b,e.remaining);
        wire::WritePOD(b,e.attack_id);wire::WritePOD(b,e.attack_type);wire::WritePOD(b,e.host_id);wire::WritePOD(b,e.host_type);
        wire::WritePOD(b,e.hit);wire::WritePOD(b,e.attack_level);wire::WritePOD(b,e.attacker_level);
        for(auto value:e.powers)wire::WritePOD(b,value);
        wire::WritePOD(b,e.can_select);wire::WritePOD(b,e.attack_country);
        for(auto value:e.position)wire::WritePOD(b,value);
    }
}
}
std::vector<std::byte> EncodeSkillEnd(std::uint32_t character,std::uint16_t skill) {
    std::vector<std::byte> b;wire::WritePOD<std::uint32_t>(b,character);
    wire::WritePOD<std::uint8_t>(b,1);wire::WritePOD<std::uint16_t>(b,skill);return b;
}
std::vector<std::byte> EncodePostureDefend(const CharSnapshot& s,std::uint16_t skill) {
    if(!s.payload||(skill!=131&&skill!=132))throw std::runtime_error("Invalid automatic posture reply");
    return EncodeForcedStatusDefend(s,skill,0);
}
static std::vector<std::byte> EncodeStatusDefend(const CharSnapshot& s,std::uint16_t skill,std::uint32_t remaining,
    std::uint32_t host,std::uint8_t host_type,std::uint16_t attack_level) {
    if(!s.payload||!skill)throw std::runtime_error("Forced maintained reply requires hydrated character");
    std::vector<std::byte> b;
    wire::WritePOD<std::uint32_t>(b,s.dwCharID);wire::WritePOD<std::uint32_t>(b,s.dwCharID);
    wire::WritePOD<std::uint8_t>(b,1);wire::WritePOD<std::uint8_t>(b,1);
    wire::WritePOD<std::uint32_t>(b,host);wire::WritePOD<std::uint8_t>(b,host_type);
    wire::WritePOD<std::uint32_t>(b,0);wire::WritePOD<std::uint32_t>(b,0); // act, animation
    wire::WritePOD<std::uint8_t>(b,1);wire::WritePOD<std::uint32_t>(b,remaining);
    wire::WritePOD<std::uint8_t>(b,0);wire::WritePOD<std::uint8_t>(b,1); // hit, HT_NORMAL
    wire::WritePOD<std::uint16_t>(b,attack_level);wire::WritePOD<std::uint8_t>(b,s.bLevel);
    for(int i=0;i<4;++i)wire::WritePOD<std::uint32_t>(b,0);
    wire::WritePOD<std::uint8_t>(b,1);wire::WritePOD<std::uint8_t>(b,0);
    wire::WritePOD<std::uint8_t>(b,s.bCountry);wire::WritePOD<std::uint8_t>(b,s.payload->aid_country);
    wire::WritePOD<std::uint16_t>(b,skill);wire::WritePOD<std::uint8_t>(b,1);
    wire::WritePOD<std::uint16_t>(b,0);wire::WritePOD<std::uint8_t>(b,1);
    for(int i=0;i<2;++i){wire::WritePOD<float>(b,s.fPosX);wire::WritePOD<float>(b,s.fPosY);wire::WritePOD<float>(b,s.fPosZ);}
    wire::WritePOD<std::uint8_t>(b,0);return b;
}
std::vector<std::byte> EncodeForcedStatusDefend(const CharSnapshot& s,std::uint16_t skill,std::uint32_t remaining) {
    return EncodeStatusDefend(s,skill,remaining,0,0,1); // ForceMaintain wire host differs from buff host
}
std::vector<std::byte> EncodeItemStatusDefend(const CharSnapshot& s,std::uint16_t skill,std::uint32_t remaining,std::uint16_t attack_level) {
    return EncodeStatusDefend(s,skill,remaining,s.dwCharID,1,attack_level); // OnCS_ITEMUSE_REQ -> self Defend
}

// "AM/PM HH:MM" server clock string CS_CHARINFO_ACK carries (legacy
// CSSender.cpp:344 formats the wall-clock the same way). Cosmetic — the
// client displays it; kept here so the pure encoder takes it as data.
std::string FormatServerClock()
{
    const std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    const char* ampm = (tm.tm_hour < 12) ? "AM" : "PM";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%s %02d : %02d", ampm, tm.tm_hour,
        tm.tm_min);
    return std::string(buf);
}



std::vector<std::byte> EncodeAddConnectAck(
    const std::vector<ConnectRoute>& routes)
{
    std::vector<std::byte> b;
    b.reserve(1 + routes.size() * 7);
    wire::WritePOD<std::uint8_t>(b, static_cast<std::uint8_t>(routes.size()));
    for (const auto& r : routes)
    {
        wire::WritePOD<std::uint32_t>(b, r.ip_addr);
        wire::WritePOD<std::uint16_t>(b, r.port);
        wire::WritePOD<std::uint8_t> (b, r.server_id);
    }
    return b;
}

std::vector<std::byte> EncodeConnectAck(
    std::uint8_t result, const std::vector<std::uint8_t>& server_ids)
{
    std::vector<std::byte> b;
    b.reserve(2 + server_ids.size());
    wire::WritePOD<std::uint8_t>(b, result);
    wire::WritePOD<std::uint8_t>(b, static_cast<std::uint8_t>(server_ids.size()));
    for (const auto id : server_ids)
        wire::WritePOD<std::uint8_t>(b, id);
    return b;
}

std::vector<std::byte> EncodeCharacterStatistics(const CharSnapshot& s,const CharacterStatistics& v) {
    std::vector<std::byte> b;b.reserve(87);
    wire::WritePOD(b,s.dwCharID);
    for(auto stat:v.primary)wire::WritePOD(b,stat);
    for(auto value:{v.min_physical,v.max_physical,v.physical_defense,v.min_ranged,v.max_ranged})wire::WritePOD(b,value);
    for(auto timing:v.timing)wire::WritePOD(b,timing.delay);
    for(auto timing:v.timing)wire::WritePOD(b,timing.rate);
    wire::WritePOD(b,v.attack_level);wire::WritePOD(b,v.defense_level);wire::WritePOD(b,v.physical_critical);
    wire::WritePOD(b,v.min_magic);wire::WritePOD(b,v.max_magic);wire::WritePOD(b,v.magic_defense);
    wire::WritePOD(b,v.magic_attack_level);wire::WritePOD(b,v.magic_defense_level);
    wire::WritePOD(b,v.charge_speed);wire::WritePOD(b,v.charge_probability);wire::WritePOD(b,v.magic_critical);
    wire::WritePOD(b,s.wSkillPoint);wire::WritePOD(b,s.bAftermath);
    return b;
}

std::vector<std::byte> EncodeCharInfoAck(
    const CharSnapshot& s, const std::string& time_str)
{
    std::vector<std::byte> b;
    b.reserve(256);
    const CharacterPayload empty;
    const auto& p=s.payload?*s.payload:empty;
    const auto count=[](std::size_t n){if(n>255)throw std::length_error("Client list exceeds BYTE count");return static_cast<std::uint8_t>(n);};

    // Original CHARINFO account-security BYTE fields.
    wire::WritePOD<std::uint32_t>(b, s.dwCharID);
    wire::WritePOD<std::uint8_t> (b, p.security.created);
    wire::WritePOD<std::uint8_t> (b, p.security.unlocked);
    wire::WritePOD<std::uint8_t> (b, p.security.disabled);
    wire::WritePOD<std::uint16_t>(b, p.selected_title); // selected persisted title
    wire::WriteString            (b, s.szNAME);
    wire::WritePOD<std::uint8_t> (b, s.bStartAct);
    wire::WritePOD<std::uint8_t> (b, s.bClass);
    wire::WritePOD<std::uint8_t> (b, s.bRace);
    wire::WritePOD<std::uint8_t> (b, s.bCountry);
    wire::WritePOD<std::uint8_t> (b, s.payload?s.payload->aid_country:3); // aid country
    wire::WritePOD<std::uint8_t> (b, s.bSex);
    wire::WritePOD<std::uint8_t> (b, s.bHair);
    wire::WritePOD<std::uint8_t> (b, s.bFace);
    wire::WritePOD<std::uint8_t> (b, s.bBody);
    wire::WritePOD<std::uint8_t> (b, s.bPants);
    wire::WritePOD<std::uint8_t> (b, s.bHand);
    wire::WritePOD<std::uint8_t> (b, s.bFoot);
    wire::WritePOD<std::uint8_t> (b, s.bHelmetHide);
    wire::WritePOD<std::uint8_t> (b, s.bLevel);

    // --- party + guild + tactics (World-sourced cluster state → 0) ---
    wire::WritePOD<std::uint16_t>(b, s.cluster.party);
    wire::WritePOD<std::uint32_t>(b, s.cluster.guild);
    wire::WritePOD<std::uint32_t>(b, s.cluster.fame);
    wire::WritePOD<std::uint32_t>(b, s.cluster.fame_color);
    wire::WritePOD<std::uint8_t> (b, s.cluster.duty);
    wire::WritePOD<std::uint8_t> (b, s.cluster.peer);
    wire::WriteString            (b, s.cluster.guild_name);
    wire::WritePOD<std::uint32_t>(b, s.cluster.tactics);
    wire::WriteString            (b, s.cluster.tactics_name);

    // --- money + exp + hp/mp -----------------------------------------
    wire::WritePOD<std::uint32_t>(b, s.dwGold);
    wire::WritePOD<std::uint32_t>(b, s.dwSilver);
    wire::WritePOD<std::uint32_t>(b, s.dwCooper);
    wire::WritePOD<std::uint32_t>(b, p.prev_exp);
    wire::WritePOD<std::uint32_t>(b, p.next_exp);
    wire::WritePOD<std::uint32_t>(b, s.dwEXP);
    wire::WritePOD<std::uint32_t>(b, s.dwMaxHP);
    wire::WritePOD<std::uint32_t>(b, s.dwHP);
    wire::WritePOD<std::uint32_t>(b, s.dwMaxMP);
    wire::WritePOD<std::uint32_t>(b, s.dwMP);
    wire::WritePOD<std::uint32_t>(b, s.cluster.party_chief);
    wire::WritePOD<std::uint16_t>(b, s.cluster.commander);

    // --- region + position -------------------------------------------
    wire::WritePOD<std::uint32_t>(b, s.dwRegion);
    wire::WritePOD<std::uint16_t>(b, s.wMapID);
    wire::WritePOD<float>        (b, s.fPosX);
    wire::WritePOD<float>        (b, s.fPosY);
    wire::WritePOD<float>        (b, s.fPosZ);
    wire::WritePOD<std::uint16_t>(b, s.wDIR);
    wire::WritePOD<std::uint16_t>(b, s.wSkillPoint);
    wire::WritePOD<std::uint8_t> (b, p.lucky_number);
    wire::WritePOD<std::uint32_t>(b, 0);            // aid left time

    // --- skill-kind points (4) + rank + bow-death flag ---------------
    wire::WritePOD<std::uint16_t>(b, p.skill_points[0]);
    wire::WritePOD<std::uint16_t>(b, p.skill_points[1]);
    wire::WritePOD<std::uint16_t>(b, p.skill_points[2]);
    wire::WritePOD<std::uint16_t>(b, p.skill_points[3]);
    wire::WritePOD<std::uint32_t>(b, s.payload?s.payload->rank_point:0);
    wire::WritePOD<std::uint8_t> (b, 0);            // non-BOW death flag (FALSE)

    wire::WritePOD<std::uint8_t>(b,count(p.bags.size()));
    for(const auto& bag:p.bags){
        wire::WritePOD<std::uint8_t>(b,bag.bag.bInvenID);
        wire::WritePOD<std::uint16_t>(b,bag.bag.wItemID);
        wire::WritePOD<std::int64_t>(b,bag.bag.dEndTime);
        wire::WritePOD<std::uint8_t>(b,count(bag.items.size()));
        for(const auto& item:bag.items){auto encoded=EncodeItemDescriptor(item,s.dwCharID,true);b.insert(b.end(),encoded.begin(),encoded.end());}
    }
    wire::WritePOD<std::uint8_t>(b,count(p.skills.size()));
    for(const auto& skill:p.skills){
        wire::WritePOD<std::uint16_t>(b,skill.wSkillID);
        wire::WritePOD<std::uint8_t>(b,skill.bLevel);
        wire::WritePOD<std::uint32_t>(b,skill.dwRemainTick);
    }
    WritePlayerEffects(b,&p);
    wire::WritePOD<std::uint8_t>(b,count(p.hotkeys.size()));
    for(const auto& keys:p.hotkeys){
        wire::WritePOD<std::uint8_t>(b,keys.inventory);
        for(const auto& [type,id]:keys.keys){wire::WritePOD<std::uint8_t>(b,type);wire::WritePOD<std::uint16_t>(b,id);}
    }
    auto cooldowns=p.item_cooldowns.value_or(ItemCooldownState{});
    SampleItemCooldowns(cooldowns,SkillClockMs());
    OrderItemCooldowns(cooldowns.timers);
    // CHARINFO retains expired live map entries with zero remaining; SAVE and
    // RELEASEMAIN include only positive durations (CSSender/SSSender).
    wire::WritePOD<std::uint8_t>(b,count(cooldowns.timers.size()));
    for(const auto& row:cooldowns.timers){wire::WritePOD(b,row.group);wire::WritePOD(b,row.remaining);}

    // --- PvP points + server clock + medals --------------------------
    wire::WritePOD<std::uint32_t>(b, p.pvp?p.pvp->total:0);
    wire::WritePOD<std::uint32_t>(b, p.pvp?p.pvp->available:0);
    wire::WritePOD<std::uint32_t>(b, p.pvp?p.pvp->month:0);
    wire::WriteString            (b, time_str);
    wire::WritePOD<std::uint32_t>(b, 0);            // medals

    return b;
}

std::vector<std::byte> EncodeEnterAck(
    const CharSnapshot& s, const Position& pos,
    std::uint8_t color, std::uint8_t new_member)
{
    std::vector<std::byte> b;
    b.reserve(160);

    // --- identity + World-sourced cluster state (→ 0/"") -------------
    wire::WritePOD<std::uint32_t>(b, s.dwCharID);
    wire::WriteString            (b, s.szNAME);
    wire::WritePOD<std::uint16_t>(b, s.payload?s.payload->selected_title:0);
    wire::WriteString            (b, color==0?s.cluster.comment:std::string{});
    wire::WritePOD<std::uint32_t>(b, s.cluster.guild);
    wire::WritePOD<std::uint32_t>(b, s.cluster.fame);
    wire::WritePOD<std::uint32_t>(b, s.cluster.fame_color);
    wire::WriteString            (b, s.cluster.guild_name);
    wire::WritePOD<std::uint8_t> (b, s.cluster.peer);
    wire::WritePOD<std::uint32_t>(b, s.cluster.tactics);
    wire::WriteString            (b, s.cluster.tactics_name);
    wire::WritePOD<std::uint8_t> (b, 0);            // store open
    wire::WriteString            (b, std::string{});// store name
    wire::WritePOD<std::uint32_t>(b, s.cluster.riding);

    // --- appearance ---------------------------------------------------
    wire::WritePOD<std::uint8_t> (b, s.bClass);
    wire::WritePOD<std::uint8_t> (b, s.bRace);
    wire::WritePOD<std::uint8_t> (b, s.bCountry);
    wire::WritePOD<std::uint8_t> (b, s.payload?s.payload->aid_country:3); // aid country
    wire::WritePOD<std::uint8_t> (b, s.bSex);
    wire::WritePOD<std::uint8_t> (b, s.bHair);
    wire::WritePOD<std::uint8_t> (b, s.bFace);
    wire::WritePOD<std::uint8_t> (b, s.bBody);
    wire::WritePOD<std::uint8_t> (b, s.bPants);
    wire::WritePOD<std::uint8_t> (b, s.bHand);
    wire::WritePOD<std::uint8_t> (b, s.bFoot);
    wire::WritePOD<std::uint8_t> (b, s.bLevel);
    wire::WritePOD<std::uint8_t> (b, s.bHelmetHide);

    // --- hp/mp + party/corps (party World-sourced → 0) ---------------
    wire::WritePOD<std::uint32_t>(b, s.dwMaxHP);
    wire::WritePOD<std::uint32_t>(b, s.dwHP);
    wire::WritePOD<std::uint32_t>(b, s.dwMaxMP);
    wire::WritePOD<std::uint32_t>(b, s.dwMP);
    // Source CSSender453–455 uses GetPartyChiefID/GetPartyID/GetCommanderID.
    // TYPE ACK preserves the client's raw fields, but later ENTER visibility
    // must still honor the server's effective SOLO1 getters.
    wire::WritePOD<std::uint32_t>(b, s.cluster.party_type==1?0:s.cluster.party_chief);
    wire::WritePOD<std::uint16_t>(b, s.cluster.party_type==1?0:s.cluster.party);
    wire::WritePOD<std::uint16_t>(b, s.cluster.party_type==1?0:s.cluster.commander);

    // --- live position + movement/action state -----------------------
    wire::WritePOD<float>        (b, pos.x);
    wire::WritePOD<float>        (b, pos.y);
    wire::WritePOD<float>        (b, pos.z);
    wire::WritePOD<std::uint8_t> (b, s.payload?s.movement.action:(s.bDead?7:s.movement.action));
    wire::WritePOD<std::uint8_t> (b, 0);            // block
    wire::WritePOD<std::uint8_t> (b, s.cluster.mode);
    wire::WritePOD<std::uint16_t>(b, s.movement.pitch);
    wire::WritePOD<std::uint16_t>(b, s.wDIR);
    wire::WritePOD<std::uint8_t> (b, s.movement.mouse);
    wire::WritePOD<std::uint8_t> (b, s.movement.key);
    wire::WritePOD<std::uint8_t> (b, color);        // faction tint (PvP)
    wire::WritePOD<std::uint32_t>(b, s.dwRegion);
    wire::WritePOD<std::uint8_t> (b, 0);            // in PC-bang
    wire::WritePOD<std::uint8_t> (b, s.bAftermath); // aftermath step
    wire::WritePOD<std::uint32_t>(b, s.payload?s.payload->rank_point:0);
    wire::WritePOD<std::uint16_t>(b, 0);            // castle id
    wire::WritePOD<std::uint8_t> (b, 0);            // camp
    wire::WritePOD<std::uint16_t>(b, 0);            // god ball

    // Source ENTER carries the same full maintained-skill layout as CHARINFO.
    WritePlayerEffects(b,s.payload.get());
    const CharacterBag* equip=nullptr;
    if(s.payload)for(const auto& bag:s.payload->bags)if(bag.bag.bInvenID==254)equip=&bag;
    if(equip&&equip->items.size()>255)throw std::length_error("Equipment exceeds BYTE count");
    wire::WritePOD<std::uint8_t>(b,equip?static_cast<std::uint8_t>(equip->items.size()):0);
    if(equip)for(const auto& item:equip->items){auto encoded=EncodeItemDescriptor(item,s.dwCharID,true);b.insert(b.end(),encoded.begin(),encoded.end());}
    wire::WritePOD<std::uint8_t> (b, new_member);

    return b;
}

std::vector<std::byte> EncodeAddMonAck(
    const MonsterInstance& m, std::uint8_t level, std::uint8_t country,
    std::uint8_t color, std::uint8_t new_member)
{
    std::vector<std::byte> b;
    b.reserve(48);

    wire::WritePOD<std::uint32_t>(b, m.dwInstanceID);
    wire::WritePOD<std::uint16_t>(b, m.wTemplateID);
    wire::WritePOD<std::uint8_t> (b, level);
    wire::WritePOD<std::uint32_t>(b, m.dwMaxHP);    // max HP
    wire::WritePOD<std::uint32_t>(b, m.dwHP);       // current HP
    wire::WritePOD<std::uint32_t>(b, 0);            // max MP (not modeled)
    wire::WritePOD<std::uint32_t>(b, 0);            // MP
    wire::WritePOD<float>        (b, m.fPosX);
    wire::WritePOD<float>        (b, m.fPosY);
    wire::WritePOD<float>        (b, m.fPosZ);
    wire::WritePOD<std::uint16_t>(b, 0);            // pitch
    wire::WritePOD<std::uint16_t>(b, 0);            // dir (AI-driven)
    wire::WritePOD<std::uint8_t> (b, 0);            // mouse dir
    wire::WritePOD<std::uint8_t> (b, 0);            // key dir
    wire::WritePOD<std::uint8_t> (b, 0);            // action
    wire::WritePOD<std::uint8_t> (b, 0);            // mode
    wire::WritePOD<std::uint8_t> (b, new_member);
    wire::WritePOD<std::uint8_t> (b, country);
    wire::WritePOD<std::uint8_t> (b, color);        // faction tint (PvP)
    wire::WritePOD<std::uint32_t>(b, 0);            // region
    wire::WritePOD<std::uint8_t> (b, 0);            // maintain-skill list empty

    return b;
}

std::vector<std::byte> EncodeHpMpAck(
    std::uint32_t id, std::uint8_t obj_type,
    std::uint32_t max_hp, std::uint32_t hp,
    std::uint32_t max_mp, std::uint32_t mp)
{
    std::vector<std::byte> b;
    b.reserve(21);
    wire::WritePOD<std::uint32_t>(b, id);
    wire::WritePOD<std::uint8_t> (b, obj_type);
    wire::WritePOD<std::uint32_t>(b, max_hp);
    wire::WritePOD<std::uint32_t>(b, hp);
    wire::WritePOD<std::uint32_t>(b, max_mp);
    wire::WritePOD<std::uint32_t>(b, mp);
    return b;
}

std::vector<std::byte> EncodeSkillUseAck(
    const SkillUseAckFields& f, const std::vector<SkillTarget>& targets)
{
    std::vector<std::byte> b;
    b.reserve(62 + targets.size() * 5);
    wire::WritePOD<std::uint8_t> (b, f.result);
    wire::WritePOD<std::uint32_t>(b, f.attack_id);
    wire::WritePOD<std::uint8_t> (b, f.attack_type);
    wire::WritePOD<std::uint16_t>(b, f.skill_id);
    wire::WritePOD<std::uint16_t>(b, f.back_skill);
    wire::WritePOD<std::uint8_t> (b, f.action_id);
    wire::WritePOD<std::uint32_t>(b, f.act_id);
    wire::WritePOD<std::uint32_t>(b, f.ani_id);
    wire::WritePOD<std::uint8_t> (b, f.skill_level);
    wire::WritePOD<std::uint16_t>(b, f.attack_level);
    wire::WritePOD<std::uint8_t> (b, f.attacker_level);
    wire::WritePOD<std::uint32_t>(b, f.pys_min_power);
    wire::WritePOD<std::uint32_t>(b, f.pys_max_power);
    wire::WritePOD<std::uint32_t>(b, f.mg_min_power);
    wire::WritePOD<std::uint32_t>(b, f.mg_max_power);
    wire::WritePOD<std::uint16_t>(b, f.trans_hp);
    wire::WritePOD<std::uint16_t>(b, f.trans_mp);
    wire::WritePOD<std::uint8_t> (b, f.curse_prob);
    wire::WritePOD<std::uint8_t> (b, f.equip_special);
    wire::WritePOD<std::uint8_t> (b, f.can_select);
    wire::WritePOD<std::uint8_t> (b, f.country);
    wire::WritePOD<std::uint8_t> (b, f.aid_country);
    wire::WritePOD<std::uint8_t> (b, f.cp);
    wire::WritePOD<float>        (b, f.gnd_x);
    wire::WritePOD<float>        (b, f.gnd_y);
    wire::WritePOD<float>        (b, f.gnd_z);
    wire::WritePOD<std::uint8_t> (b, static_cast<std::uint8_t>(targets.size()));
    for (const auto& t : targets)
    {
        wire::WritePOD<std::uint32_t>(b, t.id);
        wire::WritePOD<std::uint8_t> (b, t.type);
    }
    return b;
}

std::vector<std::byte> EncodeLoopSkillAck(
    const SkillUseAckFields& f,const std::vector<SkillTarget>& targets)
{
    if(targets.size()>255)throw std::length_error("Loop skill target count exceeds BYTE");
    std::vector<std::byte> b;b.reserve(45+targets.size()*5);
    wire::WritePOD(b,f.result);wire::WritePOD(b,f.attack_id);wire::WritePOD(b,f.attack_type);
    wire::WritePOD(b,f.skill_id);wire::WritePOD(b,f.skill_level);wire::WritePOD(b,f.attack_level);
    wire::WritePOD(b,f.attacker_level);wire::WritePOD(b,f.pys_min_power);wire::WritePOD(b,f.pys_max_power);
    wire::WritePOD(b,f.mg_min_power);wire::WritePOD(b,f.mg_max_power);wire::WritePOD(b,f.can_select);
    wire::WritePOD(b,f.country);wire::WritePOD(b,f.aid_country);wire::WritePOD(b,f.cp);
    wire::WritePOD(b,f.gnd_x);wire::WritePOD(b,f.gnd_y);wire::WritePOD(b,f.gnd_z);
    wire::WritePOD(b,static_cast<std::uint8_t>(targets.size()));
    for(const auto& t:targets){wire::WritePOD(b,t.id);wire::WritePOD(b,t.type);}
    return b;
}

std::vector<std::byte> EncodeDelMonAck(
    std::uint32_t mon_id, std::uint8_t exit_map)
{
    std::vector<std::byte> b;
    b.reserve(5);
    wire::WritePOD<std::uint32_t>(b, mon_id);
    wire::WritePOD<std::uint8_t> (b, exit_map);
    return b;
}

std::vector<std::byte> EncodeExpAck(
    std::uint32_t exp, std::uint32_t prev_level_exp,
    std::uint32_t next_level_exp, std::uint32_t soul_lot_exp)
{
    std::vector<std::byte> b;
    b.reserve(16);
    wire::WritePOD<std::uint32_t>(b, exp);
    wire::WritePOD<std::uint32_t>(b, prev_level_exp);
    wire::WritePOD<std::uint32_t>(b, next_level_exp);
    wire::WritePOD<std::uint32_t>(b, soul_lot_exp);
    return b;
}

std::vector<std::byte> EncodeMonMoveAck(
    std::uint32_t mon_id, float x, float y, float z,
    std::uint16_t dir, std::uint8_t action)
{
    constexpr std::uint8_t kOtMon = 2;   // OBJ_TYPE::OT_MON
    std::vector<std::byte> b;
    b.reserve(24);
    wire::WritePOD<std::uint16_t>(b, 1);            // count
    wire::WritePOD<std::uint32_t>(b, mon_id);
    wire::WritePOD<std::uint8_t> (b, kOtMon);
    wire::WritePOD<float>        (b, x);
    wire::WritePOD<float>        (b, y);
    wire::WritePOD<float>        (b, z);
    wire::WritePOD<std::uint16_t>(b, 0);            // pitch
    wire::WritePOD<std::uint16_t>(b, dir);
    wire::WritePOD<std::uint8_t> (b, 0);            // mouse dir
    wire::WritePOD<std::uint8_t> (b, 0);            // key dir
    wire::WritePOD<std::uint8_t> (b, action);
    return b;
}

std::vector<std::byte> EncodeActionAck(
    std::uint8_t result, std::uint32_t obj_id, std::uint8_t obj_type,
    std::uint8_t action_id, std::uint32_t act_id, std::uint32_t ani_id,
    std::uint16_t skill_id)
{
    std::vector<std::byte> b;
    b.reserve(16);
    wire::WritePOD<std::uint8_t> (b, result);
    wire::WritePOD<std::uint32_t>(b, obj_id);
    wire::WritePOD<std::uint8_t> (b, obj_type);
    wire::WritePOD<std::uint8_t> (b, action_id);
    wire::WritePOD<std::uint32_t>(b, act_id);
    wire::WritePOD<std::uint32_t>(b, ani_id);
    wire::WritePOD<std::uint16_t>(b, skill_id);
    return b;
}

std::vector<std::byte> EncodeDieAck(std::uint32_t id, std::uint8_t obj_type)
{
    std::vector<std::byte> b;
    b.reserve(5);
    wire::WritePOD<std::uint32_t>(b, id);
    wire::WritePOD<std::uint8_t> (b, obj_type);
    return b;
}

std::vector<std::byte> EncodeRevivalAck(
    std::uint32_t char_id, float x, float y, float z)
{
    std::vector<std::byte> b;
    b.reserve(16);
    wire::WritePOD<std::uint32_t>(b, char_id);
    wire::WritePOD<float>        (b, x);
    wire::WritePOD<float>        (b, y);
    wire::WritePOD<float>        (b, z);
    return b;
}

std::vector<std::byte> EncodeMoneyAck(
    std::uint32_t gold, std::uint32_t silver, std::uint32_t cooper)
{
    std::vector<std::byte> b;
    b.reserve(12);
    wire::WritePOD<std::uint32_t>(b, gold);
    wire::WritePOD<std::uint32_t>(b, silver);
    wire::WritePOD<std::uint32_t>(b, cooper);
    return b;
}

std::vector<std::byte> EncodeEquipment(const CharSnapshot& s) {
    std::vector<std::byte> out;wire::WritePOD(out,s.dwCharID);
    const CharacterBag* equip=nullptr;
    if(s.payload)for(const auto& bag:s.payload->bags)if(bag.bag.bInvenID==254)equip=&bag;
    if(equip&&equip->items.size()>255)throw std::length_error("Equipment exceeds source BYTE count");
    wire::WritePOD<std::uint8_t>(out,equip?static_cast<std::uint8_t>(equip->items.size()):0);
    if(equip)for(const auto& item:equip->items) {
        auto descriptor=EncodeItemDescriptor(item,s.dwCharID,true);
        out.insert(out.end(),descriptor.begin(),descriptor.end());
    }
    return out;
}

std::vector<std::byte> EncodeItemDescriptor(
    const ItemInstance& it, std::uint32_t viewer_char_id, bool add_item_id)
{
    // Faithful field order from CTItem::WrapPacketClient (TItem.cpp:514-547).
    // Non-cash path (m_dEndTime, the 8-byte __int64 — verified against
    // CPacket::operator<<(__int64), Packet.cpp:500). 38 bytes with the slot
    // id + zero magic options.
    std::vector<std::byte> b;
    b.reserve(40);
    if (add_item_id)
        wire::WritePOD<std::uint8_t>(b, it.bItemID);
    wire::WritePOD<std::uint16_t>(b, it.wItemID);
    wire::WritePOD<std::uint8_t> (b, it.bLevel);
    wire::WritePOD<std::uint8_t> (b, it.bGem);
    wire::WritePOD<std::uint16_t>(b, it.wMoggItemID);
    wire::WritePOD<std::uint16_t>(b, it.wCompanion);     // WORD(IEV_COMPANION)
    wire::WritePOD<std::uint8_t> (b, it.bCount);
    wire::WritePOD<std::uint32_t>(b, it.dwDuraMax);
    wire::WritePOD<std::uint32_t>(b, it.dwDuraCur);
    wire::WritePOD<std::uint8_t> (b, it.bRefineMax);
    wire::WritePOD<std::uint8_t> (b, it.bRefineCur);
    wire::WritePOD<std::uint8_t> (b, it.bGLevel);
    wire::WritePOD<std::int64_t> (b, it.dEndTime);       // __int64 m_dEndTime
    wire::WritePOD<std::uint8_t> (b, it.bGradeEffect);
    wire::WritePOD<std::uint8_t> (b, it.bELD);           // BYTE(IEV_ELD)
    wire::WritePOD<std::uint8_t> (b, it.bWrap);          // BYTE(IEV_WRAP)
    wire::WritePOD<std::uint16_t>(b, it.wColor);         // WORD(IEV_COLOR)
    wire::WritePOD<std::uint16_t>(b, it.wCustomTex);     // WORD(IEV_CUSTOMTEX)
    const std::uint8_t reg_guild =
        (it.dwGuildBound != 0 && it.dwGuildBound == viewer_char_id) ? 1u : 0u;
    wire::WritePOD<std::uint8_t> (b, reg_guild);
    if(it.magic.size()>255)throw std::length_error("Item magic exceeds BYTE count");
    wire::WritePOD<std::uint8_t>(b,static_cast<std::uint8_t>(it.magic.size()));
    for(const auto& [id,value]:it.magic){wire::WritePOD<std::uint8_t>(b,id);wire::WritePOD<std::uint16_t>(b,value);}
    return b;
}

std::vector<std::byte> EncodeMonItemListAck(
    std::uint8_t ret, std::uint8_t update, std::uint32_t mon_id,
    std::uint32_t gold, std::uint32_t silver, std::uint32_t cooper,
    const std::vector<ItemInstance>& items, std::uint32_t viewer_char_id)
{
    std::vector<std::byte> b;
    b.reserve(16 + items.size() * 40);
    wire::WritePOD<std::uint8_t> (b, ret);
    wire::WritePOD<std::uint8_t> (b, update);
    wire::WritePOD<std::uint32_t>(b, mon_id);
    wire::WritePOD<std::uint32_t>(b, gold);
    wire::WritePOD<std::uint32_t>(b, silver);
    wire::WritePOD<std::uint32_t>(b, cooper);
    if (ret == 0)   // MIL_SUCCESS — the corpse contents follow
    {
        wire::WritePOD<std::uint8_t>(
            b, static_cast<std::uint8_t>(items.size()));
        for (const auto& it : items)
        {
            const auto d = EncodeItemDescriptor(it, viewer_char_id,
                                                /*add_item_id=*/true);
            b.insert(b.end(), d.begin(), d.end());
        }
    }
    return b;
}

std::vector<std::byte> EncodeMonItemTakeAck(std::uint8_t result)
{
    std::vector<std::byte> b;
    b.reserve(1);
    wire::WritePOD<std::uint8_t>(b, result);
    return b;
}

std::vector<std::byte> EncodeQuestUpdateAck(
    std::uint32_t quest_id, std::uint32_t term_id, std::uint8_t type,
    std::uint8_t count, std::uint8_t status)
{
    std::vector<std::byte> b;
    b.reserve(11);
    wire::WritePOD<std::uint32_t>(b, quest_id);
    wire::WritePOD<std::uint32_t>(b, term_id);
    wire::WritePOD<std::uint8_t> (b, type);
    wire::WritePOD<std::uint8_t> (b, count);
    wire::WritePOD<std::uint8_t> (b, status);
    return b;
}

std::vector<std::byte> EncodeQuestCompleteAck(
    std::uint8_t result, std::uint32_t quest_id, std::uint32_t term_id,
    std::uint8_t type, std::uint32_t drop_id)
{
    std::vector<std::byte> b;
    b.reserve(14);
    wire::WritePOD<std::uint8_t> (b, result);
    wire::WritePOD<std::uint32_t>(b, quest_id);
    wire::WritePOD<std::uint32_t>(b, term_id);
    wire::WritePOD<std::uint8_t> (b, type);
    wire::WritePOD<std::uint32_t>(b, drop_id);
    return b;
}

} // namespace tmapsvr
