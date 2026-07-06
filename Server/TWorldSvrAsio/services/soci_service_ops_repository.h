#pragma once

// SociServiceOpsRepository — IServiceOpsRepository via the legacy
// THelpMessage / TClearMapCurrentUser stored procedures on the world
// pool (legacy DEFINE_QUERY(&m_db, …) parity).

#include "services/service_ops_repository.h"

#include "fourstory/db/session_pool.h"

namespace tworldsvr {

class SociServiceOpsRepository : public IServiceOpsRepository
{
public:
    explicit SociServiceOpsRepository(fourstory::db::SessionPool& pool)
        : m_pool(pool) {}

    bool SaveHelpMessage(std::uint8_t id, std::int64_t start_unix,
                         std::int64_t end_unix,
                         const std::string& message) override;
    bool ClearMapCurrentUser(std::uint8_t group_id,
                             std::uint8_t server_id,
                             std::uint8_t svr_type) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tworldsvr
