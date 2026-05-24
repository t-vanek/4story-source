#include "services/corps_registry.h"

#include "services/party_registry.h"

namespace tworldsvr {

std::uint16_t CorpsRegistry::GenId(const PartyRegistry* parties)
{
    std::lock_guard gen(m_gen_mtx);
    for (std::uint32_t tries = 0; tries < 0x10000u; ++tries)
    {
        if (++m_gen_cursor == 0) m_gen_cursor = 1; // skip the 0 sentinel
        if (Find(m_gen_cursor)) continue;
        if (parties && parties->Find(m_gen_cursor)) continue;
        return m_gen_cursor;
    }
    return 0; // id space exhausted
}

bool CorpsRegistry::Insert(std::shared_ptr<TCorps> c)
{
    if (!c) return false;
    auto& shard = m_shards[ShardOf(c->id)];
    std::unique_lock lock(shard.mtx);
    auto [it, inserted] = shard.corps.emplace(c->id, std::move(c));
    return inserted;
}

std::shared_ptr<TCorps>
CorpsRegistry::Remove(std::uint16_t corps_id)
{
    auto& shard = m_shards[ShardOf(corps_id)];
    std::unique_lock lock(shard.mtx);
    auto it = shard.corps.find(corps_id);
    if (it == shard.corps.end()) return nullptr;
    auto out = std::move(it->second);
    shard.corps.erase(it);
    return out;
}

std::shared_ptr<TCorps>
CorpsRegistry::Find(std::uint16_t corps_id) const
{
    const auto& shard = m_shards[ShardOf(corps_id)];
    std::shared_lock lock(shard.mtx);
    auto it = shard.corps.find(corps_id);
    return it == shard.corps.end() ? nullptr : it->second;
}

std::size_t CorpsRegistry::Size() const
{
    std::size_t total = 0;
    for (const auto& shard : m_shards)
    {
        std::shared_lock lock(shard.mtx);
        total += shard.corps.size();
    }
    return total;
}

std::vector<std::uint16_t> CorpsRegistry::SnapshotIds() const
{
    std::vector<std::uint16_t> out;
    for (const auto& shard : m_shards)
    {
        std::shared_lock lock(shard.mtx);
        out.reserve(out.size() + shard.corps.size());
        for (const auto& [id, _] : shard.corps)
            out.push_back(id);
    }
    return out;
}

} // namespace tworldsvr
