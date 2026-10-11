#pragma once
#include <soci/soci.h>
#include <bit>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace tmapsvr {
// CTSkillTemp::GetItemProbBuff returns the first matching raw WORD. The
// original CTBLSkillData has no ORDER BY. A single value (even repeated) is
// unambiguous; conflicting matching rows require source-order evidence rather
// than an invented SQL sort. nullopt means no matching status, not value zero.
inline std::optional<std::uint16_t> NativeItemBuffValue(soci::session& sql,std::uint16_t skill,int status) {
    const int id=std::bit_cast<std::int16_t>(skill);
    soci::rowset<int> rows=(sql.prepare << "SELECT \"wValue\" FROM character_compat.\"TSKILLDATA\" "
        "WHERE \"wSkillID\"=:id AND \"bType\"=6 AND \"bExec\"=:status",soci::use(id,"id"),soci::use(status,"status"));
    std::optional<std::uint16_t> result;
    for(int row:rows) {
        const auto value=static_cast<std::uint16_t>(row);
        if(result&&*result!=value)throw std::runtime_error("Item buff has ambiguous original skill-data ordering");
        result=value;
    }
    return result;
}
}
