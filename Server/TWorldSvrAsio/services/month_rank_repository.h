#pragma once

// IMonthRankRepository — DB surface for the W6-42 month-rollover
// chain (legacy OnDM_MONTHRANKSAVE_REQ, SSHandler.cpp:11088):
//
//   TInitMonthRank(month)      — clear the month's persisted rows
//                                (called for the closing month and,
//                                on success, for the next one).
//   TSaveMonthRank(20 params)  — persist one ranker row; the legacy
//                                per-row loop stops at the first
//                                failure (bSuccess=FALSE).
//   TInitMonthPvPoint(...)     — reset the monthly PvP points and
//                                return (RET != 0) the character who
//                                now tops TotalPoint — the new fame
//                                slot-0 entry. 16 OUTPUT columns; no
//                                `say` output (legacy leaves it "").

#include "month_rank_registry.h"

#include <cstdint>
#include <optional>

namespace tworldsvr {

class IMonthRankRepository
{
public:
    virtual ~IMonthRankRepository() = default;

    virtual bool InitMonthRank(std::uint8_t month) = 0;

    // `rank` = the table slot j (0..16), `total_rank` = the index
    // into the cross-country total array (t; 51 = unranked) — the
    // legacy CSPSaveMonthRank param order is (month, rank,
    // total_rank, char fields...).
    virtual bool SaveRanker(std::uint8_t       month,
                            std::uint8_t       rank,
                            std::uint8_t       total_rank,
                            const MonthRanker& ranker) = 0;

    // Returns false on DB failure. On success `new_top` carries the
    // new top ranker when the SP reported one (RET != 0).
    virtual bool InitMonthPvPoint(std::uint8_t  month,
                                  std::uint32_t top_total_point,
                                  std::optional<MonthRanker>& new_top) = 0;
};

} // namespace tworldsvr
