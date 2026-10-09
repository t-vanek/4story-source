#pragma once
#include "skill_targets.h"
#include <soci/soci.h>

namespace tmapsvr {
// Shared by fresh/transfer hydration and the locked consumption validator.
inline std::optional<SkillMultiAttack> ReadNativeMultiAttack(soci::session& sql,
    const SkillTemplate& t,std::uint8_t rank,std::uint8_t target_hit) {
    const int id=std::bit_cast<std::int16_t>(t.wID);
    soci::rowset<soci::row> data=(sql.prepare<<
        "SELECT \"bAction\",\"bInc\",\"wValue\",\"wValueInc\",\"bCalc\" FROM character_compat.\"TSKILLDATA\" "
        "WHERE \"wSkillID\"=:id AND \"bType\"=1 AND \"bExec\"=36 ORDER BY \"bAction\",\"bAttr\",\"bInc\",\"wValue\",\"wValueInc\",\"bCalc\"",soci::use(id));
    std::vector<SkillDataRow> rows;
    for(const auto& row:data) {
        SkillDataRow d;d.bType=SDT_ABILITY;d.bExec=36;
        d.bAction=static_cast<std::uint8_t>(row.get<int>(0));d.bInc=static_cast<std::uint8_t>(row.get<int>(1));
        d.wValue=static_cast<std::uint16_t>(row.get<int>(2));d.wValueInc=static_cast<std::uint16_t>(row.get<int>(3));
        d.bCalc=static_cast<std::uint8_t>(row.get<int>(4));rows.push_back(d);
    }
    return skill_targets::Derive(rows,t,rank,target_hit);
}
}
