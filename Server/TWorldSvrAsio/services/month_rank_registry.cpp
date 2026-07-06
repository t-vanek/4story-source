#include "month_rank_registry.h"

namespace tworldsvr {

std::uint8_t MonthRankRegistry::RankMonth() const
{
    std::lock_guard g(m_mtx);
    return m_rank_month;
}

void MonthRankRegistry::SetRankMonth(std::uint8_t month)
{
    std::lock_guard g(m_mtx);
    m_rank_month = month;
}

std::optional<MonthRankDelta>
MonthRankRegistry::Update(std::uint8_t country, const MonthRanker& r)
{
    if (country >= kMonthRankCountryCount)
        return std::nullopt;

    std::lock_guard g(m_mtx);
    auto& row = m_table[country];
    MonthRankDelta out{};

    // Warlord pre-select (SSHandler.cpp:11284): the reporter takes
    // slot 0 if it beats the current TotalPoint (or *is* the current
    // warlord — refresh), then the whole row re-competes for slot 0.
    auto reselect_warlord = [&]
    {
        if (row[0].total_point < r.total_point ||
            row[0].char_id == r.char_id)
        {
            row[0] = r;
            out.new_warlord = true;
            for (std::size_t w = 1; w < kMonthRankCount; ++w)
                if (row[w].total_point > row[0].total_point)
                    row[0] = row[w];
        }
    };
    reselect_warlord();

    // Old slot (0 = not ranked yet).
    std::uint8_t old_rank = 0;
    for (std::size_t b = 1; b < kMonthRankCount; ++b)
    {
        if (row[b].char_id == r.char_id)
        {
            old_rank = static_cast<std::uint8_t>(b);
            break;
        }
    }

    // New slot — first slot whose occupant the reporter displaces
    // (MonthPoint, then MonthWin, then MonthLose, then char-id —
    // verbatim legacy comparison ladder, SSHandler.cpp:11306).
    std::uint8_t new_rank = 0;
    for (std::size_t i = 1; i < kMonthRankCount; ++i)
    {
        const auto& cur = row[i];
        if (cur.month_point > r.month_point)
            continue;
        if (cur.month_point < r.month_point)
            new_rank = static_cast<std::uint8_t>(i);
        else if (cur.month_win < r.month_win)
            new_rank = static_cast<std::uint8_t>(i);
        else if (cur.month_win > r.month_win)
            new_rank = static_cast<std::uint8_t>(i + 1);
        else if (cur.month_lose < r.month_lose)
            new_rank = static_cast<std::uint8_t>(i);
        else if (cur.month_lose > r.month_lose)
            new_rank = static_cast<std::uint8_t>(i + 1);
        else
            new_rank = static_cast<std::uint8_t>(
                cur.char_id < r.char_id ? i : i + 1);
        break;
    }

    if (old_rank != 0 || new_rank != 0)
    {
        if (old_rank == 0)
            old_rank = kMonthRankCount - 1;        // 32
        else if (new_rank == 0)
            new_rank = kMonthRankCount;            // 33 (one past end)
    }
    else
    {
        return std::nullopt;                       // lands nowhere
    }

    if (old_rank == new_rank)
    {
        row[new_rank] = r;
        out.start_rank = out.end_rank = new_rank;
    }
    else if (old_rank < new_rank)
    {
        for (std::uint8_t j = old_rank;
             j + 1 <= new_rank - 1; ++j)
            row[j] = row[j + 1];
        row[new_rank - 1] = r;
        out.start_rank = old_rank;
        out.end_rank   = static_cast<std::uint8_t>(new_rank - 1);
    }
    else
    {
        for (std::uint8_t j = old_rank; j > new_rank; --j)
            row[j] = row[j - 1];
        row[new_rank] = r;
        out.start_rank = new_rank;
        out.end_rank   = old_rank;
    }

    // Warlord re-select after the shuffle (SSHandler.cpp:11367).
    reselect_warlord();
    return out;
}

std::array<MonthRanker, kMonthRankCount>
MonthRankRegistry::SnapshotCountry(std::uint8_t country) const
{
    std::lock_guard g(m_mtx);
    if (country >= kMonthRankCountryCount)
        return {};
    return m_table[country];
}

std::vector<MonthRanker>
MonthRankRegistry::SnapshotRange(std::uint8_t country,
                                 std::uint8_t start,
                                 std::uint8_t end) const
{
    std::vector<MonthRanker> out;
    std::lock_guard g(m_mtx);
    if (country >= kMonthRankCountryCount)
        return out;
    for (std::uint8_t i = start;
         i <= end && i < kMonthRankCount; ++i)
        out.push_back(m_table[country][i]);
    return out;
}

MonthRanker MonthRankRegistry::Get(std::uint8_t country,
                                   std::uint8_t rank) const
{
    std::lock_guard g(m_mtx);
    if (country >= kMonthRankCountryCount || rank >= kMonthRankCount)
        return {};
    return m_table[country][rank];
}

MonthRankRegistry::Table MonthRankRegistry::SnapshotTable() const
{
    std::lock_guard g(m_mtx);
    return m_table;
}

MonthRankRegistry::FameRank MonthRankRegistry::LastFameRank() const
{
    std::lock_guard g(m_mtx);
    return m_last_fame;
}

void MonthRankRegistry::SetLastFameRank(const FameRank& fame)
{
    std::lock_guard g(m_mtx);
    m_last_fame = fame;
}

MonthRankRegistry::FirstGrade MonthRankRegistry::FirstGradeGroup() const
{
    std::lock_guard g(m_mtx);
    return m_first_grade;
}

void MonthRankRegistry::SetFirstGradeGroup(const FirstGrade& group)
{
    std::lock_guard g(m_mtx);
    m_first_grade = group;
}

void MonthRankRegistry::ResetForNewMonth()
{
    std::lock_guard g(m_mtx);
    for (auto& row : m_table)
        for (std::size_t j = 1; j < kMonthRankCount; ++j)
            row[j] = MonthRanker{};
    ++m_rank_month;
    if (m_rank_month > 12)
        m_rank_month -= 12;
}

} // namespace tworldsvr
