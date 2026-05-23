#include "services/pvp_aggregate.h"

#include <ctime>

namespace tworldsvr {

void CalcWeekRecord(TGuildMember& member, std::int64_t today_day_index)
{
    // Zero the rolling aggregate before re-derive. Matches
    // legacy TGuild::CalcWeekRecord's `memset(&weekrecord, 0,
    // ...)` at the top of the loop.
    member.weekrecord = TPvPRecord{};

    // Walk + trim + sum in one pass. Iterate by index so the
    // erase-shift is safe on a forward sweep (the `continue;`
    // after erase skips the increment).
    for (std::size_t w = 0; w < member.vRecord.size(); /*advance below*/)
    {
        const auto& day = member.vRecord[w];
        if (day.day_index + guild::kPvPRecordWindowDays <=
            today_day_index)
        {
            // Stale row — drop. Don't advance w; the next
            // iteration sees the shifted element at this slot.
            member.vRecord.erase(member.vRecord.begin() +
                static_cast<std::ptrdiff_t>(w));
            continue;
        }
        member.weekrecord.kill_count += day.kill_count;
        member.weekrecord.die_count  += day.die_count;
        for (std::size_t p = 0; p < guild::kPvPEventCount; ++p)
            member.weekrecord.points[p] += day.points[p];
        ++w;
    }
}

void CalcWeekRecord(TGuildMember& member)
{
    const std::int64_t today =
        static_cast<std::int64_t>(std::time(nullptr)) / guild::kDaySec;
    CalcWeekRecord(member, today);
}

} // namespace tworldsvr
