#include "services/guild_wanted_registry.h"

#include "services/guild_constants.h"

#include <ctime>

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

std::uint8_t GuildWantedRegistry::AddApp(const TGuildWantedApp& app,
                                          std::uint8_t country)
{
    std::unique_lock lock(m_mtx);

    // Already-applied gate: a char can only hold one pending
    // application at a time. If the existing application points
    // at the same wanted entry → kSame; otherwise → kAlreadyApply.
    auto by_char = m_app_by_char.find(app.char_id);
    if (by_char != m_app_by_char.end())
    {
        return (by_char->second == app.wanted_id)
            ? guild::kSame
            : guild::kAlreadyApply;
    }

    auto entry_it = m_entries.find(app.wanted_id);
    if (entry_it == m_entries.end()) return guild::kFail;
    auto& entry = entry_it->second;

    // Country gate.
    if (entry.country != country) return guild::kFail;

    // Expiry gate (m_dlEndTime < m_timeCurrent → kWantedEnd).
    const std::int64_t now =
        static_cast<std::int64_t>(std::time(nullptr));
    if (entry.end_time < now) return guild::kWantedEnd;

    // Level-range gate.
    if (app.level < entry.min_level || app.level > entry.max_level)
        return guild::kMismatchLevel;

    entry.applicants.push_back(app);
    m_app_by_char[app.char_id] = app.wanted_id;
    return guild::kSuccess;
}

bool GuildWantedRegistry::DelApp(std::uint32_t char_id)
{
    std::unique_lock lock(m_mtx);
    auto by_char = m_app_by_char.find(char_id);
    if (by_char == m_app_by_char.end()) return false;
    const std::uint32_t wanted_id = by_char->second;
    m_app_by_char.erase(by_char);
    auto entry_it = m_entries.find(wanted_id);
    if (entry_it == m_entries.end()) return true; // index was stale
    auto& apps = entry_it->second.applicants;
    for (auto it = apps.begin(); it != apps.end(); ++it)
    {
        if (it->char_id == char_id) { apps.erase(it); break; }
    }
    return true;
}

std::vector<TGuildWantedApp>
GuildWantedRegistry::SnapshotAppsFor(std::uint32_t guild_id) const
{
    std::shared_lock lock(m_mtx);
    auto it = m_entries.find(guild_id);
    if (it == m_entries.end()) return {};
    return it->second.applicants;
}

std::uint32_t
GuildWantedRegistry::FindAppByChar(std::uint32_t char_id) const
{
    std::shared_lock lock(m_mtx);
    auto it = m_app_by_char.find(char_id);
    return it == m_app_by_char.end() ? 0u : it->second;
}

} // namespace tworldsvr
