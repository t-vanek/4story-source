#pragma once

// SociItemStateRepository — IItemStateRepository backed by SOCI
// against TITEMCHART. The legacy `TItemStateChange` SP is a probe +
// UPDATE (returns 1 when wItemID has no row); a single UPDATE with an
// affected-rows check has the same net effect, so no SP dependency.

#include "services/item_state_repository.h"

#include "fourstory/db/session_pool.h"

namespace tworldsvr {

class SociItemStateRepository : public IItemStateRepository
{
public:
    explicit SociItemStateRepository(fourstory::db::SessionPool& pool)
        : m_pool(pool) {}

    bool ChangeState(std::uint16_t item_id,
                     std::uint8_t  init_state) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tworldsvr
