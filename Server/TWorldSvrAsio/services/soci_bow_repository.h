#pragma once

// SociBowRepository — IBowRepository against TBOWSETTINGSCHART /
// TCUSTOMTIMECHART and the TAddBOWPlayer / TClearBOWPlayers /
// TDeleteSingleBOWPlayer SPs on the world pool.

#include "services/bow_repository.h"

#include "fourstory/db/session_pool.h"

namespace tworldsvr {

class SociBowRepository : public IBowRepository
{
public:
    explicit SociBowRepository(fourstory::db::SessionPool& pool)
        : m_pool(pool) {}

    std::optional<BowSettings> LoadSettings() override;
    std::vector<std::uint32_t> LoadStartTimes() override;
    void AddPlayer(std::uint32_t char_id,
                   std::uint32_t user_id) override;
    void ClearPlayers() override;
    void DeleteSinglePlayer(std::uint32_t user_id) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tworldsvr
