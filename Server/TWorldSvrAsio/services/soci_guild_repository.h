#pragma once

// SociGuildRepository — IGuildRepository backed by SOCI against
// TGUILDTABLE + TGUILDMEMBERTABLE. The legacy module fans LoadAll
// out as `CSPLoadGuilds` + per-guild `CSPGetGuildMembers`; this
// implementation does the equivalent in two batched queries
// (SELECT * FROM TGUILDTABLE, then SELECT * FROM TGUILDMEMBERTABLE
// joined back in memory by guild_id) which scales better for the
// ~100..1000-guild server population than the per-guild fan-out.

#include "services/guild_repository.h"

#include "fourstory/db/session_pool.h"

namespace tworldsvr {

class SociGuildRepository : public IGuildRepository
{
public:
    explicit SociGuildRepository(fourstory::db::SessionPool& pool)
        : m_pool(pool) {}

    std::vector<std::shared_ptr<TGuild>> LoadAll() override;
    std::optional<std::shared_ptr<TGuild>> FindById(
        std::uint32_t guild_id) override;

private:
    fourstory::db::SessionPool& m_pool;
};

} // namespace tworldsvr
