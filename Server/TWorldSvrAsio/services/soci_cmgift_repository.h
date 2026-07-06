#pragma once

// SociCmGiftRepository — ICmGiftRepository via TCMGIFTCHART + the
// TCMGiftCanTake / Add / Set / Del SPs on the world pool. CanTake's
// return value and Add's OUTPUT id are captured through
// DECLARE/EXEC/SELECT batches (same shape as the W6-42 rollover).

#include "services/cmgift_repository.h"

#include "fourstory/db/session_pool.h"

namespace tworldsvr {

class SociCmGiftRepository : public ICmGiftRepository
{
public:
    explicit SociCmGiftRepository(fourstory::db::SessionPool& pool)
        : m_pool(pool) {}

    std::vector<CmGift> LoadChart() override;
    std::uint8_t CanTake(const std::string& target_name,
                         std::uint16_t gift_id) override;
    std::optional<std::uint16_t> Add(const CmGift& gift) override;
    bool Set(const CmGift& gift) override;
    bool Del(std::uint16_t gift_id) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tworldsvr
