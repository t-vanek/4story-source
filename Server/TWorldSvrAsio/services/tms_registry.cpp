#include "services/tms_registry.h"

namespace tworldsvr {

bool TmsRegistry::Insert(std::shared_ptr<TTms> t)
{
    if (!t) return false;
    auto& shard = m_shards[ShardOf(t->id)];
    std::unique_lock lock(shard.mtx);
    auto [it, inserted] = shard.groups.emplace(t->id, std::move(t));
    return inserted;
}

std::shared_ptr<TTms> TmsRegistry::Remove(std::uint32_t tms_id)
{
    auto& shard = m_shards[ShardOf(tms_id)];
    std::unique_lock lock(shard.mtx);
    auto it = shard.groups.find(tms_id);
    if (it == shard.groups.end()) return nullptr;
    auto out = std::move(it->second);
    shard.groups.erase(it);
    return out;
}

std::shared_ptr<TTms> TmsRegistry::Find(std::uint32_t tms_id) const
{
    const auto& shard = m_shards[ShardOf(tms_id)];
    std::shared_lock lock(shard.mtx);
    auto it = shard.groups.find(tms_id);
    return it == shard.groups.end() ? nullptr : it->second;
}

std::uint32_t TmsRegistry::GenId()
{
    std::lock_guard gen(m_gen_mtx);
    for (std::uint64_t tries = 0; tries < 0x100000000ull; ++tries)
    {
        if (++m_gen_cursor == 0) m_gen_cursor = 1; // skip the 0 sentinel
        if (!Find(m_gen_cursor)) return m_gen_cursor;
    }
    return 0; // id space exhausted (unreachable in practice)
}

std::size_t TmsRegistry::Size() const
{
    std::size_t total = 0;
    for (const auto& shard : m_shards)
    {
        std::shared_lock lock(shard.mtx);
        total += shard.groups.size();
    }
    return total;
}

std::vector<std::uint32_t> TmsRegistry::SnapshotIds() const
{
    std::vector<std::uint32_t> out;
    for (const auto& shard : m_shards)
    {
        std::shared_lock lock(shard.mtx);
        out.reserve(out.size() + shard.groups.size());
        for (const auto& [id, _] : shard.groups)
            out.push_back(id);
    }
    return out;
}

} // namespace tworldsvr
