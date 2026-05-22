#include "services/char_registry.h"

namespace tworldsvr {

bool CharRegistry::Insert(std::shared_ptr<TChar> ch)
{
    if (!ch) return false;
    auto& shard = m_shards[ShardOf(ch->char_id)];
    std::unique_lock lock(shard.mtx);
    auto [it, inserted] = shard.chars.emplace(ch->char_id, std::move(ch));
    return inserted;
}

std::shared_ptr<TChar>
CharRegistry::Remove(std::uint32_t char_id)
{
    auto& shard = m_shards[ShardOf(char_id)];
    std::unique_lock lock(shard.mtx);
    auto it = shard.chars.find(char_id);
    if (it == shard.chars.end()) return nullptr;
    auto out = std::move(it->second);
    shard.chars.erase(it);
    return out;
}

std::shared_ptr<TChar>
CharRegistry::Find(std::uint32_t char_id) const
{
    const auto& shard = m_shards[ShardOf(char_id)];
    std::shared_lock lock(shard.mtx);
    auto it = shard.chars.find(char_id);
    return it == shard.chars.end() ? nullptr : it->second;
}

std::size_t CharRegistry::Size() const
{
    std::size_t total = 0;
    for (const auto& shard : m_shards)
    {
        std::shared_lock lock(shard.mtx);
        total += shard.chars.size();
    }
    return total;
}

std::vector<std::uint32_t> CharRegistry::SnapshotIds() const
{
    std::vector<std::uint32_t> out;
    for (const auto& shard : m_shards)
    {
        std::shared_lock lock(shard.mtx);
        out.reserve(out.size() + shard.chars.size());
        for (const auto& [id, _] : shard.chars)
            out.push_back(id);
    }
    return out;
}

void CharRegistry::MarkUserActive(std::uint32_t user_id)
{
    auto& shard = m_user_shards[ShardOf(user_id)];
    std::unique_lock lock(shard.mtx);
    shard.users.insert(user_id);
}

void CharRegistry::MarkUserInactive(std::uint32_t user_id)
{
    auto& shard = m_user_shards[ShardOf(user_id)];
    std::unique_lock lock(shard.mtx);
    shard.users.erase(user_id);
}

bool CharRegistry::IsUserActive(std::uint32_t user_id) const
{
    const auto& shard = m_user_shards[ShardOf(user_id)];
    std::shared_lock lock(shard.mtx);
    return shard.users.contains(user_id);
}

std::size_t CharRegistry::ActiveUserCount() const
{
    std::size_t total = 0;
    for (const auto& shard : m_user_shards)
    {
        std::shared_lock lock(shard.mtx);
        total += shard.users.size();
    }
    return total;
}

} // namespace tworldsvr
