#pragma once

// SociGuildLevelRepository — TGUILDCHART loader via SOCI. Single
// SELECT against the legacy table; runs once at boot.

#include "services/guild_level_repository.h"

#include "fourstory/db/session_pool.h"

namespace tworldsvr {

class SociGuildLevelRepository : public IGuildLevelRepository
{
public:
    explicit SociGuildLevelRepository(fourstory::db::SessionPool& pool)
        : m_pool(pool) {}

    std::vector<TGuildLevelRow> LoadAll() override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tworldsvr
