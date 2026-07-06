#pragma once

// SociLuckyEventRepository — ILuckyEventRepository via
// TEVENTQUARTERCHART + TGetItemName + TEventQuarterUpdate on the
// world pool (outputs captured through DECLARE/EXEC/SELECT batches).

#include "services/lucky_event_repository.h"

#include "fourstory/db/session_pool.h"

namespace tworldsvr {

class SociLuckyEventRepository : public ILuckyEventRepository
{
public:
    explicit SociLuckyEventRepository(fourstory::db::SessionPool& pool)
        : m_pool(pool) {}

    std::vector<LuckyEvent> List(std::uint8_t day) override;
    std::vector<LuckyEvent> ListAll() override;
    std::optional<LuckyEventUpdateResult>
        Update(std::uint8_t type, const LuckyEvent& event) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tworldsvr
