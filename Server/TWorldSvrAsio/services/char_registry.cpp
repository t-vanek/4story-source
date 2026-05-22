#include "services/char_registry.h"

#include <algorithm>
#include <cctype>

namespace tworldsvr {

std::string CharRegistry::ToUpper(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s)
        out.push_back(static_cast<char>(
            std::toupper(static_cast<unsigned char>(c))));
    return out;
}

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
    lock.unlock();

    // Drop the name-index entry too (if any). We take a snapshot
    // of the current name under the per-char lock to avoid racing
    // a concurrent Rename. Empty name = nothing indexed.
    std::string upper;
    {
        std::lock_guard g(out->lock);
        if (!out->name.empty()) upper = ToUpper(out->name);
    }
    if (!upper.empty())
    {
        auto& ns = m_name_shards[NameShardOf(upper)];
        std::unique_lock nlock(ns.mtx);
        auto nit = ns.names.find(upper);
        if (nit != ns.names.end() && nit->second.get() == out.get())
            ns.names.erase(nit);
    }
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

std::shared_ptr<TChar>
CharRegistry::FindByName(const std::string& name) const
{
    if (name.empty()) return nullptr;
    const auto upper = ToUpper(name);
    const auto& ns = m_name_shards[NameShardOf(upper)];
    std::shared_lock lock(ns.mtx);
    auto it = ns.names.find(upper);
    return it == ns.names.end() ? nullptr : it->second;
}

bool CharRegistry::Rename(std::uint32_t char_id, const std::string& new_name)
{
    auto target = Find(char_id);
    if (!target) return false;

    // Snapshot the old name under the per-char lock so we can drop
    // it from the name index atomically with installing the new
    // one. Empty new_name is treated as "clear the name index for
    // this char but keep the char_id entry" — used at CloseChar
    // setup time when guild-broadcast handlers want to drop the
    // lookup before the actual remove.
    std::string old_upper;
    {
        std::lock_guard g(target->lock);
        if (!target->name.empty()) old_upper = ToUpper(target->name);
    }

    if (new_name.empty())
    {
        // Drop-only path. Empty new_name keeps target->name intact
        // for any in-flight handler that snapshotted it, but
        // removes the index entry so FindByName misses.
        if (!old_upper.empty())
        {
            auto& ns = m_name_shards[NameShardOf(old_upper)];
            std::unique_lock lock(ns.mtx);
            auto it = ns.names.find(old_upper);
            if (it != ns.names.end() && it->second.get() == target.get())
                ns.names.erase(it);
        }
        return true;
    }

    const auto new_upper = ToUpper(new_name);

    // Insert under the new name. If the destination shard already
    // has a different char by that name, refuse (legacy + W3a-3
    // contract: cluster-wide name uniqueness).
    {
        auto& nns = m_name_shards[NameShardOf(new_upper)];
        std::unique_lock lock(nns.mtx);
        auto it = nns.names.find(new_upper);
        if (it != nns.names.end() && it->second.get() != target.get())
            return false;
        nns.names[new_upper] = target;
    }

    // Update the TChar's stored name.
    {
        std::lock_guard g(target->lock);
        target->name = new_name;
    }

    // Drop the old name entry if it's different from the new one
    // (a no-op self-rename leaves things alone).
    if (!old_upper.empty() && old_upper != new_upper)
    {
        auto& ons = m_name_shards[NameShardOf(old_upper)];
        std::unique_lock lock(ons.mtx);
        auto it = ons.names.find(old_upper);
        if (it != ons.names.end() && it->second.get() == target.get())
            ons.names.erase(it);
    }
    return true;
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
