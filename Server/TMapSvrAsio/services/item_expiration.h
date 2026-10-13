#pragma once
#include "domain/character_payload.h"
#include <cstdint>

namespace tmapsvr {
// TMapSvr::SetAlarmTime: strict comparisons matter at the four boundaries.
// An expired item retains its previous alarm until CheckItemEndTime deletes it.
inline std::int64_t SourceItemAlarm(std::int64_t expires,std::int64_t now,std::int64_t previous=0) {
    if(!expires||expires<=now)return previous;
    const auto remaining=expires-now;
    for(const std::int64_t seconds:{1800,1200,600,60})if(remaining>seconds)return expires-seconds;
    return expires;
}
inline bool ItemExpirationDue(const CharacterPayload& p,std::int64_t now) {
    if(p.personal_store)return false; // source store pauses both alarms and deletion
    for(const auto& bag:p.bags) {
        if(bag.bag.dEndTime&&bag.bag.dEndTime<=now)return true;
        for(const auto& item:bag.items)if(item.dEndTime&&
            (item.dEndTime<=now||(item.alarm_time&&item.alarm_time<=now)))return true;
    }
    return false;
}
}
