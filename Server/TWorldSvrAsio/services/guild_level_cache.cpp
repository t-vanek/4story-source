#include "services/guild_level_cache.h"

#include <spdlog/spdlog.h>

namespace tworldsvr {

void GuildLevelCache::LoadFrom(const std::vector<TGuildLevelRow>& rows)
{
    m_rows = {};
    m_present = {};
    m_count = 0;
    for (const auto& row : rows)
    {
        if (row.level == 0 || row.level > kMaxGuildLevel)
        {
            spdlog::debug("GuildLevelCache: dropping out-of-range "
                          "bLevel={} during load", row.level);
            continue;
        }
        m_rows[row.level]    = row;
        m_present[row.level] = true;
        ++m_count;
    }
}

const TGuildLevelRow* GuildLevelCache::Find(std::uint8_t level) const
{
    if (level == 0 || level > kMaxGuildLevel) return nullptr;
    if (!m_present[level]) return nullptr;
    return &m_rows[level];
}

} // namespace tworldsvr
