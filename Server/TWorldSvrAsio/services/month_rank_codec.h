#pragma once

// MonthRanker wire codec — byte-for-byte the legacy
// MONTHRANKER::WrapPacketIn / WrapPacketOut order
// (TWorldType.h:1003): TotalRank, MonthRank, CharID, Name,
// TotalPoint, MonthPoint, MonthWin, MonthLose, TotalWin, TotalLose,
// Country, Level, Class, Race, Sex, Hair, Face, Say, Guild.
// Shared by the senders, the UPDATE_ACK handler and the wire tests.

#include "../wire_codec.h"
#include "month_rank_registry.h"

#include <cstddef>
#include <vector>

namespace tworldsvr {

inline void WriteMonthRanker(std::vector<std::byte>& body,
                             const MonthRanker&      r)
{
    using namespace wire;
    WritePOD<std::uint32_t>(body, r.total_rank);
    WritePOD<std::uint32_t>(body, r.month_rank);
    WritePOD<std::uint32_t>(body, r.char_id);
    WriteString(body, r.name);
    WritePOD<std::uint32_t>(body, r.total_point);
    WritePOD<std::uint32_t>(body, r.month_point);
    WritePOD<std::uint16_t>(body, r.month_win);
    WritePOD<std::uint16_t>(body, r.month_lose);
    WritePOD<std::uint32_t>(body, r.total_win);
    WritePOD<std::uint32_t>(body, r.total_lose);
    WritePOD<std::uint8_t>(body, r.country);
    WritePOD<std::uint8_t>(body, r.level);
    WritePOD<std::uint8_t>(body, r.klass);
    WritePOD<std::uint8_t>(body, r.race);
    WritePOD<std::uint8_t>(body, r.sex);
    WritePOD<std::uint8_t>(body, r.hair);
    WritePOD<std::uint8_t>(body, r.face);
    WriteString(body, r.say);
    WriteString(body, r.guild);
}

inline bool ReadMonthRanker(wire::Reader& r, MonthRanker& out)
{
    return r.Read(out.total_rank) && r.Read(out.month_rank) &&
           r.Read(out.char_id) && r.ReadString(out.name) &&
           r.Read(out.total_point) && r.Read(out.month_point) &&
           r.Read(out.month_win) && r.Read(out.month_lose) &&
           r.Read(out.total_win) && r.Read(out.total_lose) &&
           r.Read(out.country) && r.Read(out.level) &&
           r.Read(out.klass) && r.Read(out.race) &&
           r.Read(out.sex) && r.Read(out.hair) &&
           r.Read(out.face) && r.ReadString(out.say) &&
           r.ReadString(out.guild);
}

} // namespace tworldsvr
