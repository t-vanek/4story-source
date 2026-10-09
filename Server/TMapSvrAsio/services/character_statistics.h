#pragma once
#include "domain/character_statistics.h"
#include "domain/stat.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace tmapsvr::character_statistics {
// No rounding/floors invented for missing charts. These conversions mirror the
// original truncation and narrowing for representable inputs; invalid floating
// conversions and signed overflow are rejected instead of invoking C++ UB.
inline std::uint32_t Truncate(double value) {
    if(!std::isfinite(value)||value<0||value>std::numeric_limits<std::uint32_t>::max())
        throw std::domain_error("Character statistic outside original DWORD range");
    return static_cast<std::uint32_t>(value);
}
struct ItemAttributes {
    std::uint32_t min_physical{},max_physical{},min_ranged{},max_ranged{},min_magic{},max_magic{},defense{},magic_defense{};
    void Add(std::uint8_t type,std::uint16_t min_ap,std::uint16_t max_ap,
             std::uint16_t min_map,std::uint16_t max_map,std::uint16_t dp,std::uint16_t mdp) {
        // TItem::Get{Min,Max}{L,Magic}AP and Get{Magic,}DefendPower.
        if(type==1){min_physical+=min_ap;max_physical+=max_ap;min_magic+=min_map;max_magic+=max_map;}
        if(type==4){min_ranged+=min_ap;max_ranged+=max_ap;}
        if(type!=6){defense+=dp;magic_defense+=mdp;} // shields use a separate block path
    }
};

// Formula, primary and passive callbacks are supplied by the pinned native
// catalog loader. Primary receives the source wBaseMIN (defense-level floor).
// equipment contains projected GetMagicValue results, not raw DB percentages.
template<class Formula,class Primary,class Delta,class Equipment>
CharacterStatistics Build(std::uint8_t level,std::uint8_t aftermath,
    const std::array<std::uint32_t,6>& unscaled,const ItemAttributes& items,
    const std::array<SkillAttackTiming,3>& timing,
    Formula formula,Primary primary,Delta delta,Equipment equipment) {
    CharacterStatistics out;out.level=level;out.aftermath=aftermath;out.timing=timing;
    for(unsigned i=0;i<6;++i)out.primary[i]=static_cast<std::uint16_t>(Truncate(primary(i,0)));
    const auto modified=[&](std::uint32_t base,unsigned type){
        const auto value=static_cast<std::int64_t>(base)+delta(base,type);
        if(base>std::numeric_limits<std::int32_t>::max()||value>std::numeric_limits<std::int32_t>::max())
            throw std::domain_error("Character ability outside original INT range");
        return static_cast<std::uint32_t>(std::max<std::int64_t>(0,value));
    };
    const auto base=[&](unsigned id,unsigned stat,bool floor=false){
        const auto f=formula(id);
        return (floor?0:f.dwInit)+Truncate(primary(stat,floor?static_cast<std::uint16_t>(Truncate(f.fRateY)):0)*f.fRateX);
    };
    const auto pair=[&](unsigned fid,unsigned stat,unsigned magic,unsigned min_magic,unsigned max_magic,
                        std::uint32_t min_item,std::uint32_t max_item){
        const auto common=base(fid,stat)+equipment(magic);
        const auto high=modified(common+max_item+equipment(max_magic),magic);
        const auto low=modified(common+min_item+equipment(min_magic),magic);
        return std::array<std::uint32_t,2>{std::min(low,high),high};
    };
    auto ap=pair(1,0,7,61,62,items.min_physical,items.max_physical);
    out.min_physical=ap[0];out.max_physical=ap[1];
    ap=pair(3,1,9,63,64,items.min_ranged,items.max_ranged);
    out.min_ranged=ap[0];out.max_ranged=ap[1];
    ap=pair(13,3,17,65,66,items.min_magic,items.max_magic);
    out.min_magic=ap[0];out.max_magic=ap[1];
    const auto defense=[&](unsigned id,unsigned type,std::uint32_t item){
        const auto f=formula(id);
        return modified(f.dwInit+Truncate(std::pow(static_cast<double>(f.fRateY),level)*f.fRateX)+item+equipment(type),type);
    };
    out.physical_defense=defense(12,8,items.defense);
    out.magic_defense=defense(23,16,items.magic_defense);
    out.attack_level=static_cast<std::uint16_t>(modified(base(5,1)+equipment(11),11));
    out.defense_level=static_cast<std::uint16_t>(modified(base(6,1,true)+equipment(12),12));
    out.magic_attack_level=static_cast<std::uint16_t>(modified(base(28,4)+equipment(86),86));
    out.magic_defense_level=static_cast<std::uint16_t>(modified(base(29,4,true)+equipment(87),87));
    const auto critical=[&](unsigned fid,unsigned stat,unsigned type){
        const auto f=formula(fid);
        return static_cast<std::uint8_t>(modified(f.dwInit+Truncate(unscaled[stat]*f.fRateX)+equipment(type),type));
    };
    out.physical_critical=critical(7,1,13);out.magic_critical=critical(18,4,21);
    const auto charge=formula(17);
    out.charge_probability=static_cast<std::uint8_t>(modified(Truncate(unscaled[5]*charge.fRateX-charge.fRateY)+equipment(20),20));
    out.charge_speed=static_cast<std::uint8_t>(modified(formula(24).dwInit+equipment(19),19));
    return out;
}
}
