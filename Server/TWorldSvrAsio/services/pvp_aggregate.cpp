#include "services/pvp_aggregate.h"

#include <ctime>
#include <limits>
#include <stdexcept>

namespace tworldsvr {

void CalcWeekRecord(TGuildMember& member, std::int64_t today_day_index)
{
    if(today_day_index<0||today_day_index>std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("Source guild DWORD record date unavailable");
    for(const auto& row:member.vRecord)if(row.day_index<0||row.day_index>std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("Source guild DWORD historical date unavailable");
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
        if (static_cast<std::uint32_t>(static_cast<std::uint32_t>(day.day_index)+std::uint32_t{7}) <=
            static_cast<std::uint32_t>(today_day_index))
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
