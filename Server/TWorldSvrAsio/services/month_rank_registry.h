#pragma once

// MonthRankRegistry — the cluster's monthly PvP ranking table
// (legacy `m_arMonthRank[COUNTRY_COUNT][MONTHRANKCOUNT]`,
// TWorldSvr.cpp:1894 + SSHandler.cpp:11263). Three countries ×
// 33 slots; slot 0 is the *warlord* (highest TotalPoint), slots
// 1..32 are ordered by MonthPoint with win / lose / char-id
// tiebreakers. The table boots empty (legacy fills it live from
// map-side MW_MONTHRANKUPDATE_ACK reports; there is no boot-time
// DB load) and the whole table replays to every joining map peer
// (SSHandler.cpp:698).
//
// The W6-42 month-rollover chain (SM_MONTHRANKSAVE → persist →
// reset + FIRSTGRADEGROUP broadcast) layers on top of this store.
//
// Known legacy quirk NOT kept: `MONTHRANKER::operator=` copies
// m_dwTotalRank into m_dwMonthRank (TWorldType.h:962) — every
// table shuffle corrupted the stored MonthRank field. We copy
// field-for-field; the wire still carries whatever the map
// reported (clients derive display rank from table position).

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace tworldsvr {

inline constexpr std::size_t kMonthRankCountryCount = 3;  // COUNTRY_COUNT
inline constexpr std::size_t kMonthRankCount        = 33; // MONTHRANKCOUNT
inline constexpr std::size_t kFameRankCount         = 9;  // FAMERANKCOUNT
inline constexpr std::size_t kFirstGradeGroupCount  = 17; // FIRSTGRADEGROUPCOUNT
inline constexpr std::size_t kTotalMonthRankCount   =
    kFirstGradeGroupCount * kMonthRankCountryCount;       // 51

// One ranked character (legacy MONTHRANKER, TWorldType.h:935).
// Field order here == WrapPacketIn/Out wire order.
struct MonthRanker
{
    std::uint32_t total_rank  = 0;
    std::uint32_t month_rank  = 0;
    std::uint32_t char_id     = 0;
    std::string   name;
    std::uint32_t total_point = 0;
    std::uint32_t month_point = 0;
    std::uint16_t month_win   = 0;
    std::uint16_t month_lose  = 0;
    std::uint32_t total_win   = 0;
    std::uint32_t total_lose  = 0;
    std::uint8_t  country     = 0;
    std::uint8_t  level       = 0;
    std::uint8_t  klass       = 0;
    std::uint8_t  race        = 0;
    std::uint8_t  sex         = 0;
    std::uint8_t  hair        = 0;
    std::uint8_t  face        = 0;
    std::string   say;
    std::string   guild;
};

// Result of a rank update: the dirty slot range the broadcast has
// to carry plus whether slot 0 (warlord) changed.
struct MonthRankDelta
{
    std::uint8_t start_rank  = 0;
    std::uint8_t end_rank    = 0;
    bool         new_warlord = false;
};

class MonthRankRegistry
{
public:
    MonthRankRegistry() = default;
    MonthRankRegistry(const MonthRankRegistry&) = delete;
    MonthRankRegistry& operator=(const MonthRankRegistry&) = delete;

    // Current rank month (1..12). Legacy boots it from the local
    // clock (TWorldSvr.cpp:1894 `m_bRankMonth = t.GetMonth()`).
    std::uint8_t RankMonth() const;
    void SetRankMonth(std::uint8_t month);

    // Apply one map-reported ranker update — the verbatim legacy
    // re-rank algorithm (SSHandler.cpp:11284-11380): warlord
    // pre-select, old/new slot scan (MonthPoint desc, then MonthWin
    // desc, then MonthLose asc-of-sorts, then char-id), slot shift,
    // warlord re-select. Returns nullopt when the update lands
    // nowhere (legacy `!bOldRank && !bNewRank` drop).
    std::optional<MonthRankDelta> Update(std::uint8_t       country,
                                         const MonthRanker& ranker);

    // Copy out one country row [0..32] (senders serialize outside
    // the lock).
    std::array<MonthRanker, kMonthRankCount>
        SnapshotCountry(std::uint8_t country) const;

    // Copy out a slot range for the UPDATE broadcast.
    std::vector<MonthRanker> SnapshotRange(std::uint8_t country,
                                           std::uint8_t start,
                                           std::uint8_t end) const;

    // Test / W6-42 helper: read one slot.
    MonthRanker Get(std::uint8_t country, std::uint8_t rank) const;

    // --- W6-42 month-rollover state -------------------------------

    using Table =
        std::array<std::array<MonthRanker, kMonthRankCount>,
                   kMonthRankCountryCount>;
    using FameRank  = std::array<MonthRanker, kFameRankCount>;
    using FirstGrade =
        std::array<std::array<MonthRanker, kFirstGradeGroupCount>,
                   kMonthRankCountryCount>;

    // Full-table copy for the SM_MONTHRANKSAVE computation.
    Table SnapshotTable() const;

    // m_arLastFameRank (fed by the rollover; [0] refreshed from the
    // TInitMonthPvPoint new-top result).
    FameRank LastFameRank() const;
    void SetLastFameRank(const FameRank& fame);

    // m_arFirstGradeGroup — the frozen previous-month top-17 per
    // country, broadcast as MW_FIRSTGRADEGROUP_REQ.
    FirstGrade FirstGradeGroup() const;
    void SetFirstGradeGroup(const FirstGrade& group);

    // Legacy MonthRankReset (TWorldSvr.cpp:5598) + month advance:
    // slots 1..32 reset, **slot 0 (warlord) survives**; month wraps
    // 12 -> 1 (legacy `if(m_bRankMonth > 12) -= 12`).
    void ResetForNewMonth();

private:
    mutable std::mutex m_mtx;
    std::uint8_t       m_rank_month = 0;
    std::array<std::array<MonthRanker, kMonthRankCount>,
               kMonthRankCountryCount> m_table{};
    FameRank           m_last_fame{};
    FirstGrade         m_first_grade{};
};

} // namespace tworldsvr
