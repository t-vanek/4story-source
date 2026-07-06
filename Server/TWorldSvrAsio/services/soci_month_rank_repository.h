#pragma once

// SociMonthRankRepository — IMonthRankRepository via the legacy
// TInitMonthRank / TSaveMonthRank / TInitMonthPvPoint SPs on the
// world pool. TInitMonthPvPoint's 16 OUTPUT params + return value
// are captured through a DECLARE/EXEC/SELECT batch (the same shape
// fourstory::db::orm::SpCall generates).

#include "services/month_rank_repository.h"

#include "fourstory/db/session_pool.h"

namespace tworldsvr {

class SociMonthRankRepository : public IMonthRankRepository
{
public:
    explicit SociMonthRankRepository(fourstory::db::SessionPool& pool)
        : m_pool(pool) {}

    bool InitMonthRank(std::uint8_t month) override;
    bool SaveRanker(std::uint8_t month, std::uint8_t rank,
                    std::uint8_t total_rank,
                    const MonthRanker& ranker) override;
    bool InitMonthPvPoint(std::uint8_t month,
                          std::uint32_t top_total_point,
                          std::optional<MonthRanker>& new_top) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tworldsvr
