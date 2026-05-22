#pragma once

// FakeGuildRepository — in-memory IGuildRepository. Used by every
// test under Server/TWorldSvrAsio/tests/ that doesn't depend on a
// live DB. Seeded through AddGuild().

#include "services/guild_repository.h"

#include <mutex>
#include <unordered_map>

namespace tworldsvr {

class FakeGuildRepository : public IGuildRepository
{
public:
    // Seed a guild into the fake store. The repository takes a
    // strong reference; LoadAll / FindById return a fresh
    // shared_ptr to a deep copy so test-side mutation of the
    // returned guild doesn't bleed back into the seed.
    void AddGuild(std::shared_ptr<TGuild> g);

    std::vector<std::shared_ptr<TGuild>> LoadAll() override;
    std::optional<std::shared_ptr<TGuild>> FindById(
        std::uint32_t guild_id) override;

private:
    mutable std::mutex                                         m_mtx;
    std::unordered_map<std::uint32_t, std::shared_ptr<TGuild>> m_guilds;
};

} // namespace tworldsvr
