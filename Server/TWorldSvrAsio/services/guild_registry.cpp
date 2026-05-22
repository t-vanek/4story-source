#include "services/guild_registry.h"

namespace tworldsvr {

bool GuildRegistry::Insert(std::shared_ptr<TGuild> g)
{
    if (!g) return false;
    auto& shard = m_shards[ShardOf(g->id)];
    std::unique_lock lock(shard.mtx);
    auto [it, inserted] = shard.guilds.emplace(g->id, std::move(g));
    return inserted;
}

std::shared_ptr<TGuild>
GuildRegistry::Remove(std::uint32_t guild_id)
{
    auto& shard = m_shards[ShardOf(guild_id)];
    std::unique_lock lock(shard.mtx);
    auto it = shard.guilds.find(guild_id);
    if (it == shard.guilds.end()) return nullptr;
    auto out = std::move(it->second);
    shard.guilds.erase(it);
    return out;
}

std::shared_ptr<TGuild>
GuildRegistry::Find(std::uint32_t guild_id) const
{
    const auto& shard = m_shards[ShardOf(guild_id)];
    std::shared_lock lock(shard.mtx);
    auto it = shard.guilds.find(guild_id);
    return it == shard.guilds.end() ? nullptr : it->second;
}

std::size_t GuildRegistry::Size() const
{
    std::size_t total = 0;
    for (const auto& shard : m_shards)
    {
        std::shared_lock lock(shard.mtx);
        total += shard.guilds.size();
    }
    return total;
}

std::vector<std::uint32_t> GuildRegistry::SnapshotIds() const
{
    std::vector<std::uint32_t> out;
    for (const auto& shard : m_shards)
    {
        std::shared_lock lock(shard.mtx);
        out.reserve(out.size() + shard.guilds.size());
        for (const auto& [id, _] : shard.guilds)
            out.push_back(id);
    }
    return out;
}

} // namespace tworldsvr
