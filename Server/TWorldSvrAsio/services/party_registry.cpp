#include "services/party_registry.h"

namespace tworldsvr {

bool PartyRegistry::Insert(std::shared_ptr<TParty> p)
{
    if (!p) return false;
    auto& shard = m_shards[ShardOf(p->id)];
    std::unique_lock lock(shard.mtx);
    auto [it, inserted] = shard.parties.emplace(p->id, std::move(p));
    return inserted;
}

std::shared_ptr<TParty>
PartyRegistry::Remove(std::uint16_t party_id)
{
    auto& shard = m_shards[ShardOf(party_id)];
    std::unique_lock lock(shard.mtx);
    auto it = shard.parties.find(party_id);
    if (it == shard.parties.end()) return nullptr;
    auto out = std::move(it->second);
    shard.parties.erase(it);
    return out;
}

std::shared_ptr<TParty>
PartyRegistry::Find(std::uint16_t party_id) const
{
    const auto& shard = m_shards[ShardOf(party_id)];
    std::shared_lock lock(shard.mtx);
    auto it = shard.parties.find(party_id);
    return it == shard.parties.end() ? nullptr : it->second;
}

std::uint16_t PartyRegistry::GenId()
{
    std::lock_guard gen(m_gen_mtx);
    for (std::uint32_t tries = 0; tries < 0x10000u; ++tries)
    {
        if (++m_gen_cursor == 0) m_gen_cursor = 1; // skip the 0 sentinel
        if (!Find(m_gen_cursor)) return m_gen_cursor;
    }
    return 0; // id space exhausted
}

std::size_t PartyRegistry::Size() const
{
    std::size_t total = 0;
    for (const auto& shard : m_shards)
    {
        std::shared_lock lock(shard.mtx);
        total += shard.parties.size();
    }
    return total;
}

std::vector<std::uint16_t> PartyRegistry::SnapshotIds() const
{
    std::vector<std::uint16_t> out;
    for (const auto& shard : m_shards)
    {
        std::shared_lock lock(shard.mtx);
        out.reserve(out.size() + shard.parties.size());
        for (const auto& [id, _] : shard.parties)
            out.push_back(id);
    }
    return out;
}

} // namespace tworldsvr
