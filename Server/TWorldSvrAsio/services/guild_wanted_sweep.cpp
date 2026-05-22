#include "services/guild_wanted_sweep.h"

#include "fourstory/db/co_offload.h"

#include <spdlog/spdlog.h>

#include <ctime>

namespace tworldsvr {

boost::asio::awaitable<void>
SweepExpiredWanted(GuildWantedRegistry&      reg,
                   IGuildRepository*         repo,
                   boost::asio::thread_pool* db_pool)
{
    const std::int64_t now =
        static_cast<std::int64_t>(std::time(nullptr));
    const auto removed = reg.PruneExpired(now);
    if (removed.empty()) co_return;

    if (repo)
    {
        for (std::uint32_t guild_id : removed)
        {
            co_await fourstory::db::CoOffloadVoidIf(db_pool,
                [repo, guild_id] {
                    repo->DeleteWanted(guild_id);
                });
        }
    }

    spdlog::info("SweepExpiredWanted: pruned {} expired wanted entr{}",
        removed.size(), removed.size() == 1 ? "y" : "ies");
}

} // namespace tworldsvr
