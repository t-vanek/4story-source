#pragma once

// FakeMonthRankRepository — in-memory IMonthRankRepository for
// tests. Records every call in order; SaveRanker fails for char ids
// seeded via FailOnCharId; InitMonthPvPoint returns the seeded
// new-top ranker (if any).

#include "services/month_rank_repository.h"

#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>

namespace tworldsvr {

class FakeMonthRankRepository : public IMonthRankRepository
{
public:
    struct SaveCall
    {
        std::uint8_t month = 0;
        std::uint8_t rank = 0;
        std::uint8_t total_rank = 0;
        MonthRanker  ranker;
    };

    void FailOnCharId(std::uint32_t char_id);
    void SetNewTop(MonthRanker top);

    bool InitMonthRank(std::uint8_t month) override;
    bool SaveRanker(std::uint8_t month, std::uint8_t rank,
                    std::uint8_t total_rank,
                    const MonthRanker& ranker) override;
    bool InitMonthPvPoint(std::uint8_t month,
                          std::uint32_t top_total_point,
                          std::optional<MonthRanker>& new_top) override;

    std::vector<std::uint8_t> InitCalls() const;
    std::vector<SaveCall>     SaveCalls() const;
    std::vector<std::pair<std::uint8_t, std::uint32_t>>
        PvPointCalls() const;

private:
    mutable std::mutex                 m_mtx;
    std::unordered_set<std::uint32_t>  m_fail_ids;
    std::optional<MonthRanker>         m_new_top;
    std::vector<std::uint8_t>          m_init_calls;
    std::vector<SaveCall>              m_save_calls;
    std::vector<std::pair<std::uint8_t, std::uint32_t>> m_pv_calls;
};

} // namespace tworldsvr
