#include "services/guild_wanted_registry.h"

namespace tworldsvr {

bool GuildWantedRegistry::AddOrUpdate(const TGuildWanted& w)
{
    std::unique_lock lock(m_mtx);
    m_entries[w.guild_id] = w;
    return true;
}

bool GuildWantedRegistry::Remove(std::uint32_t guild_id)
{
    std::unique_lock lock(m_mtx);
    return m_entries.erase(guild_id) != 0;
}

std::optional<TGuildWanted>
GuildWantedRegistry::Find(std::uint32_t guild_id) const
{
    std::shared_lock lock(m_mtx);
    auto it = m_entries.find(guild_id);
    if (it == m_entries.end()) return std::nullopt;
    return it->second;
}

std::vector<TGuildWanted>
GuildWantedRegistry::SnapshotByCountry(std::uint8_t country) const
{
    std::vector<TGuildWanted> out;
    std::shared_lock lock(m_mtx);
    out.reserve(m_entries.size());
    for (const auto& [_, w] : m_entries)
        if (w.country == country) out.push_back(w);
    return out;
}

std::size_t GuildWantedRegistry::Size() const
{
    std::shared_lock lock(m_mtx);
    return m_entries.size();
}

} // namespace tworldsvr
