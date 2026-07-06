#pragma once

// SociWarOpsRepository — IWarOpsRepository against TACTIVECHARTABLE
// (+ TCHARTABLE / TAIDTABLE joins) and the TSaveCastleApplicant SP.

#include "services/war_ops_repository.h"

#include "fourstory/db/session_pool.h"

namespace tworldsvr {

class SociWarOpsRepository : public IWarOpsRepository
{
public:
    explicit SociWarOpsRepository(fourstory::db::SessionPool& pool)
        : m_pool(pool) {}

    std::vector<ActiveCharRow>
        LoadActiveChars(std::int64_t prune_before_unix) override;
    bool SaveCastleApplicant(std::uint16_t castle, std::uint32_t char_id,
                             std::uint8_t camp) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tworldsvr
