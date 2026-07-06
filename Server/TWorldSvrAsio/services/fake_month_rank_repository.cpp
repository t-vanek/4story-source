#include "services/fake_month_rank_repository.h"

namespace tworldsvr {

void FakeMonthRankRepository::FailOnCharId(std::uint32_t char_id)
{
    std::lock_guard g(m_mtx);
    m_fail_ids.insert(char_id);
}

void FakeMonthRankRepository::SetNewTop(MonthRanker top)
{
    std::lock_guard g(m_mtx);
    m_new_top = std::move(top);
}

bool FakeMonthRankRepository::InitMonthRank(std::uint8_t month)
{
    std::lock_guard g(m_mtx);
    m_init_calls.push_back(month);
    return true;
}

bool FakeMonthRankRepository::SaveRanker(std::uint8_t month,
                                         std::uint8_t rank,
                                         std::uint8_t total_rank,
                                         const MonthRanker& ranker)
{
    std::lock_guard g(m_mtx);
    m_save_calls.push_back({month, rank, total_rank, ranker});
    return m_fail_ids.count(ranker.char_id) == 0;
}

bool FakeMonthRankRepository::InitMonthPvPoint(
    std::uint8_t month, std::uint32_t top_total_point,
    std::optional<MonthRanker>& new_top)
{
    std::lock_guard g(m_mtx);
    m_pv_calls.emplace_back(month, top_total_point);
    new_top = m_new_top;
    return true;
}

std::vector<std::uint8_t> FakeMonthRankRepository::InitCalls() const
{
    std::lock_guard g(m_mtx);
    return m_init_calls;
}

std::vector<FakeMonthRankRepository::SaveCall>
FakeMonthRankRepository::SaveCalls() const
{
    std::lock_guard g(m_mtx);
    return m_save_calls;
}

std::vector<std::pair<std::uint8_t, std::uint32_t>>
FakeMonthRankRepository::PvPointCalls() const
{
    std::lock_guard g(m_mtx);
    return m_pv_calls;
}

} // namespace tworldsvr
