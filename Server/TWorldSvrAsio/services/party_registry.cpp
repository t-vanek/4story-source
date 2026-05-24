#include "services/party_registry.h"

#include <map>

namespace tworldsvr {

std::uint8_t TParty::GetOrderIndex(std::uint32_t char_id) const
{
    for (std::size_t i = 0; i < members.size(); ++i)
        if (members[i] == char_id)
            return static_cast<std::uint8_t>(i);
    return 0;
}

void TParty::SetNextOrder(std::uint32_t char_id)
{
    const std::size_t next = static_cast<std::size_t>(GetOrderIndex(char_id)) + 1;
    order_char_id = (members.size() <= next) ? (members.empty() ? 0 : members[0])
                                             : members[next];
}

std::uint32_t TParty::GetNextOrder(const std::vector<std::uint32_t>& eligible)
{
    // Map member-position → char_id for the eligible members, so the
    // walk below visits them in party order (legacy mapINDEX).
    std::map<std::uint8_t, std::uint32_t> by_index;
    for (auto id : eligible)
        for (std::size_t j = 0; j < members.size(); ++j)
            if (members[j] == id)
                by_index.emplace(static_cast<std::uint8_t>(j), members[j]);

    if (by_index.empty()) return 0;

    // The member whose turn it currently is, if eligible.
    for (const auto& [idx, id] : by_index)
        if (id == order_char_id)
        {
            SetNextOrder(id);
            return id;
        }

    // Otherwise the first eligible member after the cursor position.
    const std::uint8_t cur = GetOrderIndex(order_char_id);
    for (const auto& [idx, id] : by_index)
        if (idx > cur)
        {
            SetNextOrder(id);
            return id;
        }

    // Else wrap to the first eligible member.
    const std::uint32_t first = by_index.begin()->second;
    SetNextOrder(first);
    return first;
}

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
