#include "services/guild_tactics_sweep.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <ctime>
#include <vector>

namespace tworldsvr {

boost::asio::awaitable<void>
SweepExpiredTactics(GuildRegistry& guilds, CharRegistry* chars)
{
    const std::int64_t now =
        static_cast<std::int64_t>(std::time(nullptr));

    std::vector<std::uint32_t> freed_chars;
    for (std::uint32_t guild_id : guilds.SnapshotIds())
    {
        auto guild = guilds.Find(guild_id);
        if (!guild) continue;
        std::lock_guard gl(guild->lock);
        auto& roster = guild->tactics_members;
        for (auto it = roster.begin(); it != roster.end(); )
        {
            if (it->end_time != 0 && it->end_time <= now)
            {
                freed_chars.push_back(it->id);
                it = roster.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    if (freed_chars.empty()) co_return;

    // Clear the freed chars' tactics back-pointers.
    if (chars)
    {
        for (std::uint32_t cid : freed_chars)
        {
            if (auto c = chars->Find(cid))
            {
                std::lock_guard g(c->lock);
                c->tactics_guild_id = 0;
            }
        }
    }

    spdlog::info("SweepExpiredTactics: ended {} expired tactics "
                 "contract{}",
        freed_chars.size(), freed_chars.size() == 1 ? "" : "s");
    co_return;
}

} // namespace tworldsvr
