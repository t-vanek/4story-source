#include "services/guild_tactics_wanted_registry.h"

#include <ctime>

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

std::uint8_t
GuildTacticsWantedRegistry::AddApp(const TGuildTacticsWantedApp& app,
                                   std::uint8_t  country,
                                   std::uint32_t applicant_guild_id)
{
    std::unique_lock lock(m_mtx);

    // Already-applied gate.
    auto existing = m_apps.find(app.char_id);
    if (existing != m_apps.end())
    {
        return (existing->second.wanted_guild_id == app.wanted_guild_id)
            ? guild::kSame
            : guild::kAlreadyApply;
    }

    // Locate the posting by (guild, id).
    auto git = m_by_guild.find(app.wanted_guild_id);
    if (git == m_by_guild.end()) return guild::kFail;
    const TGuildTacticsWanted* posting = nullptr;
    for (const auto& w : git->second)
        if (w.id == app.wanted_id) { posting = &w; break; }
    if (!posting) return guild::kFail;

    if (posting->country != country) return guild::kFail;

    const std::int64_t now =
        static_cast<std::int64_t>(std::time(nullptr));
    if (posting->end_time < now) return guild::kWantedEnd;

    if (app.level < posting->min_level ||
        app.level > posting->max_level)
        return guild::kMismatchLevel;

    // Can't apply to a posting from your own guild.
    if (applicant_guild_id != 0 &&
        applicant_guild_id == app.wanted_guild_id)
        return guild::kSameGuildTactics;

    // Copy the posting's reward fields onto the stored app so
    // the chief's volunteer list shows what was promised.
    TGuildTacticsWantedApp stored = app;
    stored.day    = posting->day;
    stored.point  = posting->point;
    stored.gold   = posting->gold;
    stored.silver = posting->silver;
    stored.cooper = posting->cooper;
    m_apps.emplace(app.char_id, std::move(stored));
    return guild::kSuccess;
}

bool GuildTacticsWantedRegistry::DelApp(std::uint32_t char_id)
{
    std::unique_lock lock(m_mtx);
    return m_apps.erase(char_id) != 0;
}

std::vector<TGuildTacticsWantedApp>
GuildTacticsWantedRegistry::SnapshotAppsFor(std::uint32_t guild_id) const
{
    std::vector<TGuildTacticsWantedApp> out;
    std::shared_lock lock(m_mtx);
    for (const auto& [_, app] : m_apps)
        if (app.wanted_guild_id == guild_id) out.push_back(app);
    return out;
}

std::uint32_t
GuildTacticsWantedRegistry::FindAppGuildByChar(std::uint32_t char_id) const
{
    std::shared_lock lock(m_mtx);
    auto it = m_apps.find(char_id);
    return it == m_apps.end() ? 0u : it->second.wanted_guild_id;
}

std::optional<TGuildTacticsWantedApp>
GuildTacticsWantedRegistry::FindApp(std::uint32_t char_id) const
{
    std::shared_lock lock(m_mtx);
    auto it = m_apps.find(char_id);
    if (it == m_apps.end()) return std::nullopt;
    return it->second;
}

} // namespace tworldsvr
