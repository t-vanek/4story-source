#pragma once
#include "domain/main_transfer.h"
#include <soci/soci.h>
#include <map>
#include <stdexcept>

namespace tmapsvr::native_item_row {
inline long long Number(const soci::row& r,const std::string& name) {
    if(r.get_indicator(name)==soci::i_null)return 0;
    switch(r.get_properties(name).get_data_type()) {
    case soci::dt_integer:return r.get<int>(name);
    case soci::dt_long_long:return r.get<long long>(name);
    default:throw std::runtime_error("Unexpected native item numeric type");
    }
}
// The caller selects epoch with the same source null-expiry sentinel as Map
// load. Preserve signed SMALLINT/INT bit patterns and all raw extensions.
inline transfer::Item Read(const soci::row& r) {
    const auto u32=[&](const std::string& key){return static_cast<std::uint32_t>(Number(r,key));};
    const auto u16=[&](const std::string& key){return static_cast<std::uint16_t>(Number(r,key));};
    const auto u8=[&](const std::string& key){return static_cast<std::uint8_t>(Number(r,key));};
    transfer::Item raw;
    raw.storage=u8("bStorageType");raw.storage_id=u32("dwStorageID");
    raw.owner_type=u8("bOwnerType");raw.owner_id=u32("dwOwnerID");
    raw.id=Number(r,"dlID");raw.slot=u8("bItemID");raw.item=u16("wItemID");
    raw.level=u8("bLevel");raw.gem=u8("bGem");raw.appearance=u16("wMoggItemID");
    raw.companion=u32("dwTime5");raw.count=u8("bCount");raw.grade=u8("bGLevel");
    raw.durability_max=u32("dwDuraMax");raw.durability=u32("dwDuraCur");
    raw.refine=u8("bRefineCur");raw.expires=Number(r,"epoch");raw.grade_effect=u8("bGradeEffect");
    raw.eld=u32("dwTime1");raw.wrap=u32("dwTime2");raw.color=u32("dwTime3");
    raw.guild=u32("dwTime4");raw.texture=u32("dwTime6");
    std::map<int,std::uint16_t> magic;
    for(unsigned i=1;i<=6;++i) {
        const auto suffix=std::to_string(i);const auto id=u8("bMagic"+suffix),value_id=id;
        const auto value=u16("wValue"+suffix);if(value_id&&value)magic[id]=value;
    }
    for(const auto& [id,value]:magic)raw.magic.push_back({static_cast<std::uint8_t>(id),value});
    return raw;
}
}
