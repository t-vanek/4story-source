#pragma once
#include "domain/main_transfer.h"
#include <soci/soci.h>
#include <bit>
#include <map>
#include <optional>

namespace tmapsvr {
struct QuestMagicItem {
    transfer::Item item;
    std::uint16_t days{};
    std::uint32_t price{};
};
// LoadData/CTBLQuestMagicItemChart/CreateItem: template count is always1,
// last valid duplicate magic wins, missing magic definitions are skipped.
// days is a duration here, not an absolute item expiration timestamp.
inline std::optional<QuestMagicItem> ReadQuestMagicItem(soci::session& sql,std::uint32_t quest_item) {
    const int id=std::bit_cast<std::int32_t>(quest_item);soci::row row;
    sql << "SELECT * FROM game_compat.\"TQUESTITEMCHART\" WHERE \"dwID\"=:id",soci::use(id),soci::into(row);
    if(!sql.got_data())return {};
    QuestMagicItem out;auto& raw=out.item;
    raw.item=static_cast<std::uint16_t>(row.get<int>("wItemID"));raw.count=1;
    const int template_id=std::bit_cast<std::int16_t>(raw.item);int found=0;
    sql << "SELECT 1 FROM character_compat.\"TITEMCHART\" WHERE \"wItemID\"=:id",soci::use(template_id),soci::into(found);
    if(!sql.got_data())return {};
    raw.level=static_cast<std::uint8_t>(row.get<int>("bLevel"));raw.grade=static_cast<std::uint8_t>(row.get<int>("bGLevel"));
    raw.durability_max=static_cast<std::uint32_t>(row.get<int>("dwDuraMax"));raw.durability=static_cast<std::uint32_t>(row.get<int>("dwDuraCur"));
    raw.refine=static_cast<std::uint8_t>(row.get<int>("bRefineCur"));raw.grade_effect=static_cast<std::uint8_t>(row.get<int>("bGradeEffect"));
    raw.gem=static_cast<std::uint8_t>(row.get<int>("bGem"));
    raw.eld=static_cast<std::uint32_t>(row.get<int>("dwTime1"));raw.wrap=static_cast<std::uint32_t>(row.get<int>("dwTime2"));
    raw.color=static_cast<std::uint32_t>(row.get<int>("dwTime3"));raw.guild=static_cast<std::uint32_t>(row.get<int>("dwTime4"));
    raw.companion=static_cast<std::uint32_t>(row.get<int>("dwTime5"));raw.texture=static_cast<std::uint32_t>(row.get<int>("dwTime6"));
    out.days=static_cast<std::uint16_t>(row.get<int>("wUseTime"));out.price=static_cast<std::uint32_t>(row.get<int>("dwMoney"));
    std::map<std::uint8_t,std::uint16_t> magic;
    for(unsigned i=1;i<=6;++i) {
        const int code=row.get<int>("bMagic"+std::to_string(i));
        const auto value=static_cast<std::uint16_t>(row.get<int>("wValue"+std::to_string(i)));
        if(!code||!value)continue;
        sql << "SELECT 1 FROM actor_compat.\"TITEMMAGICCHART\" WHERE \"bMagic\"=:id",soci::use(code),soci::into(found);
        if(sql.got_data())magic[static_cast<std::uint8_t>(code)]=value;
    }
    for(const auto& [code,value]:magic)raw.magic.push_back({code,value});
    return out;
}
}
