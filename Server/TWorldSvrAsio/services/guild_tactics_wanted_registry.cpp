#include "services/guild_tactics_wanted_registry.h"

namespace tworldsvr {

std::uint32_t GuildTacticsWantedRegistry::NextId()
{
    std::unique_lock lock(m_mtx);
    return ++m_next_id;
}

std::uint8_t
GuildTacticsWantedRegistry::AddOrUpdate(const TGuildTacticsWanted& entry)
{
    std::unique_lock lock(m_mtx);
    auto& list = m_by_guild[entry.guild_id];

    // Update in place if a posting with this id already exists
    // under this guild.
    for (auto& w : list)
    {
        if (w.id == entry.id)
        {
            w = entry;
            m_guild_by_id[entry.id] = entry.guild_id;
            return guild::kSuccess;
        }
    }

    // New posting — enforce the per-guild cap.
    if (list.size() >= guild::kMaxTacticsWantedPerGuild)
        return guild::kMaxWanted;

    list.push_back(entry);
    m_guild_by_id[entry.id] = entry.guild_id;
    return guild::kSuccess;
}

std::uint8_t
GuildTacticsWantedRegistry::Remove(std::uint32_t guild_id,
                                   std::uint32_t id)
{
    std::unique_lock lock(m_mtx);
    auto it = m_by_guild.find(guild_id);
    if (it == m_by_guild.end()) return guild::kFail;

    auto& list = it->second;
    for (auto vit = list.begin(); vit != list.end(); ++vit)
    {
        if (vit->id == id)
        {
            list.erase(vit);
            m_guild_by_id.erase(id);
            if (list.empty()) m_by_guild.erase(it);
            return guild::kSuccess;
        }
    }
    return guild::kFail;
}

std::optional<TGuildTacticsWanted>
GuildTacticsWantedRegistry::Find(std::uint32_t id) const
{
    std::shared_lock lock(m_mtx);
    auto idx = m_guild_by_id.find(id);
    if (idx == m_guild_by_id.end()) return std::nullopt;
    auto it = m_by_guild.find(idx->second);
    if (it == m_by_guild.end()) return std::nullopt;
    for (const auto& w : it->second)
        if (w.id == id) return w;
    return std::nullopt;
}

std::vector<TGuildTacticsWanted>
GuildTacticsWantedRegistry::SnapshotByCountry(std::uint8_t country) const
{
    std::vector<TGuildTacticsWanted> out;
    std::shared_lock lock(m_mtx);
    for (const auto& [_, list] : m_by_guild)
        for (const auto& w : list)
            if (w.country == country) out.push_back(w);
    return out;
}

std::size_t GuildTacticsWantedRegistry::Size() const
{
    std::shared_lock lock(m_mtx);
    std::size_t n = 0;
    for (const auto& [_, list] : m_by_guild) n += list.size();
    return n;
}

} // namespace tworldsvr
