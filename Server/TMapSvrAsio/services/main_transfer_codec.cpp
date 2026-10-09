#include "main_transfer_codec.h"
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace tmapsvr::transfer {
namespace {
constexpr std::size_t MaxBody = 65535 - 8; // original WORD length includes SS header
struct Writer {
    std::vector<std::byte> bytes;
    template<class T> bool Field(const T& value) {
        if constexpr(std::is_same_v<T,bool>) return Field(std::uint8_t(value));
        else if constexpr(std::is_floating_point_v<T>) return Field(std::bit_cast<std::uint32_t>(value));
        else {
            static_assert(std::is_integral_v<T>);
            if(bytes.size()+sizeof(T)>MaxBody)return false;
            using U=std::make_unsigned_t<T>;U n=static_cast<U>(value);
            for(std::size_t i=0;i<sizeof(T);++i){bytes.push_back(std::byte(n&255));n>>=8;}
            return true;
        }
    }
    bool Field(const std::string& value) {
        if(value.size()>MaxBody||!Field(static_cast<std::int32_t>(value.size()))||bytes.size()+value.size()>MaxBody)return false;
        for(unsigned char c:value)bytes.push_back(std::byte(c));
        return true;
    }
    template<class T,std::size_t N> bool Field(const std::array<T,N>& a){for(const auto& v:a)if(!Field(v))return false;return true;}
    template<class...T> bool operator()(const T&...v){return (Field(v)&&...);}
    template<class Count=std::uint16_t,class T,class F> bool List(const std::vector<T>& rows,F encode) {
        if(rows.size()>std::numeric_limits<Count>::max()||!Field(static_cast<Count>(rows.size())))return false;
        for(const auto& row:rows)if(!encode(row))return false;
        return true;
    }
};
struct Reader {
    std::span<const std::byte> bytes;
    std::size_t offset{};
    template<class T> bool Field(T& value) {
        if constexpr(std::is_same_v<T,bool>) {std::uint8_t n{};if(!Field(n)||n>1)return false;value=n!=0;return true;}
        else if constexpr(std::is_floating_point_v<T>) {std::uint32_t n{};if(!Field(n))return false;value=std::bit_cast<float>(n);return true;}
        else {
            static_assert(std::is_integral_v<T>);
            if(bytes.size()-offset<sizeof(T))return false;
            using U=std::make_unsigned_t<T>;U n{};
            for(std::size_t i=0;i<sizeof(T);++i)n|=static_cast<U>(std::to_integer<unsigned char>(bytes[offset++]))<<(8*i);
            value=std::bit_cast<T>(n);return true;
        }
    }
    bool Field(std::string& value) {
        std::int32_t n{};if(!Field(n)||n<0||static_cast<std::size_t>(n)>bytes.size()-offset)return false;
        value.assign(reinterpret_cast<const char*>(bytes.data()+offset),n);offset+=n;return true;
    }
    template<class T,std::size_t N> bool Field(std::array<T,N>& a){for(auto& v:a)if(!Field(v))return false;return true;}
    template<class...T> bool operator()(T&...v){return (Field(v)&&...);}
    template<class Count=std::uint16_t,class T,class F> bool List(std::vector<T>& rows,F decode) {
        Count count{};if(!Field(count)||count>bytes.size()-offset)return false;
        // Allocate only after each complete entry; never reserve from an
        // untrusted count. Every entry consumes at least one source byte.
        rows.clear();
        for(std::size_t i=0;i<count;++i){T row{};if(!decode(row))return false;rows.push_back(std::move(row));}
        return true;
    }
};
template<class A,class B> bool BuffFields(A& a,B& b) {
    return a(b.level,b.skill,b.remaining,b.attack_type,b.attack_id,b.host_type,b.host_id,b.attack_country);
}
template<class A,class R> bool RecordFields(A& a,R& r) {
    return a(r.name,r.klass,r.level,r.win,r.points,r.time);
}
template<class A,class R> bool ItemFields(A& a,R& r) {
    return a(r.storage,r.storage_id,r.owner_type,r.owner_id,r.id,r.slot,r.item,
        r.level,r.gem,r.appearance,r.companion,r.count,r.grade,r.durability_max,r.durability,r.refine,r.expires,
        r.grade_effect,r.eld,r.wrap,r.color,r.guild,r.texture)&&
        a.template List<std::uint8_t>(r.magic,[&](auto& m){return a(m.id,m.value);});
}
template<class A,class S> bool Layout(A& a,S& t) {
    auto& s=t.character;
    if(!a(t.db_load,s.dwCharID,t.key,t.result,s.szNAME,s.bStartAct,s.bRealSex,s.bClass,s.bLevel,s.bRace,
        s.bCountry,s.bOriCountry,s.bSex,s.bHair,s.bFace,s.bBody,s.bPants,s.bHand,s.bFoot,s.bHelmetHide,
        s.dwGold,s.dwSilver,s.dwCooper,s.dwEXP,s.dwHP,s.dwMP,s.wSkillPoint,s.dwRegion,s.bGuildLeave,
        s.dwGuildLeaveTime,s.wMapID,s.wSpawnID,s.wLastSpawnID,s.dwLastDestination,s.wTemptedMon,s.bAftermath,
        s.fPosX,s.fPosY,s.fPosZ,s.wDIR,s.bStatLevel,s.bStatPoint,s.dwStatExp,t.save_age,t.local_id,t.login,
        t.new_security,t.security_code,t.security_tries,t.security_tick,t.security_unlocked,
        t.aid_country,t.aid_date,t.pc_bang,t.pc_bang_time,t.pc_bang_items,t.lucky,t.post_total,t.post_read))return false;
    if(!a.List(t.bags,[&](auto& r){return a(r.bInvenID,r.wItemID,r.dEndTime,r.bELD);})||
       !a.List(t.cabinets,[&](auto& r){return a(r.id,r.use);})||
       !a.List(t.items,[&](auto& r){return ItemFields(a,r);})||
       !a.List(t.skills,[&](auto& r){return a(r.bLevel,r.wSkillID,r.dwRemainTick);})||
       !a.List(t.buffs,[&](auto& r){return BuffFields(a,r);})||
       !a.List(t.quests,[&](auto& r){return a(r.id,r.remaining,r.completed,r.triggered,r.save);})||
       !a.List(t.quest_terms,[&](auto& r){return a(r.quest,r.id,r.type,r.count);})||
       !a.List(t.hotkeys,[&](auto& r){
           if(!a(r.row.inventory,r.save))return false;
           for(auto& key:r.row.keys)if(!a(key.first,key.second))return false;
           return true;
       })||
       !a.List(t.item_cooldowns,[&](auto& r){return a(r.group,r.remaining);})||
       !a(t.saddle.item,t.saddle.expires,t.saddle.type)||
       !a.List(t.pets,[&](auto& r){return a(r.id,r.name,r.used_at,r.effect);})||
       !a.List(t.during_items,[&](auto& r){return a(r.item,r.type,r.remaining,r.expires);})||
       !a.List(t.recalls,[&](auto& r){return a(r.id,r.monster,r.pet,r.attribute,r.level,r.hp,r.mp,r.skill_level,r.x,r.y,r.z,r.time,r.effect);})||
       !a.List(t.recall_buffs,[&](auto& r){return a(r.recall)&&BuffFields(a,r.buff);})||
       !a.List(t.protected_characters,[&](auto& r){return a(r.id,r.name,r.option,r.changed);})||
       !a(t.pvp_available,t.pvp_total,t.pvp_rank,t.pvp_percent,t.pvp_records)||
       !a.List(t.pvp_recent,[&](auto& r){return RecordFields(a,r);})||
       !a.List(t.duel_records,[&](auto& r){return RecordFields(a,r);})||
       !a(t.duel_sets)||t.duel_sets!=1||!a(t.duel_scores)||
       !a.List(t.auction_bids,[&](auto& r){return a(r);})||
       !a.List(t.auction_interests,[&](auto& r){return a(r);})||
       !a.List(t.auction_registrations,[&](auto& r){return a(r);})||
       !a(t.month_points,t.month_wins,t.month_losses,t.month_rank,t.month_percent)||
       !a.List(t.titles,[&](auto& r){return a(r.id,r.selected);})||
       !a.List(t.companions,[&](auto& r){return a(r.slot,r.monster,r.experience,r.next_experience,r.life,r.skill_points,
           r.level,r.name,r.effect,r.attributes,r.bonus);})||
       !a.List(t.companion_items,[&](auto& r){
           if(!a(r.slot,r.tick))return false;
           for(std::size_t i=0;i<2;++i)if(!a(r.items[i],r.expires[i]))return false;
           return true;
       })||!a(t.companion_slots,t.medals,t.rank_points,t.play_time))return false;
    return true;
}
bool Valid(const State& t) {
    const auto& s=t.character;
    return t.db_load<=1&&t.result==0&&t.login==0&&t.new_security<=1&&t.security_unlocked<=1&&t.duel_sets==1&&
        std::isfinite(s.fPosX)&&std::isfinite(s.fPosY)&&std::isfinite(s.fPosZ);
}
}
std::vector<std::byte> Encode(const State& state) {
    Writer writer;
    if(!Valid(state)||!Layout(writer,state))throw std::invalid_argument("Invalid or oversized source Map transfer");
    return std::move(writer.bytes);
}
std::optional<State> Decode(std::span<const std::byte> bytes) {
    if(bytes.size()>MaxBody)return {};
    Reader reader{bytes};State state;
    if(!Layout(reader,state)||reader.offset!=bytes.size()||!Valid(state))return {};
    return state;
}
std::vector<std::byte> EncodeItem(const Item& item) {
    Writer writer;
    if(!ItemFields(writer,item))throw std::invalid_argument("Invalid or oversized source item");
    return std::move(writer.bytes);
}
}
