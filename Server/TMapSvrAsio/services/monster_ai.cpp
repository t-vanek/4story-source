#include "services/monster_ai.h"

#include "services/channel_presence.h"
#include "services/client_senders.h"
#include "services/monster_registry.h"
#include "asio_session.h"

#include "MessageId.h"

#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <memory>
#include <random>
#include <vector>

namespace tmapsvr {

boost::asio::awaitable<void>
RunMonsterAi(IMonsterRegistry&         registry,
             IChannelPresence&         presence,
             std::chrono::milliseconds interval,
             std::size_t               max_per_tick)
{
    using tnetlib::protocol::MessageId;

    std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> step(-3.0f, 3.0f);

    boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor);

    spdlog::info("monster_ai: idle-roam tick every {}ms, up to {} monster(s) "
                 "per tick", interval.count(), max_per_tick);

    for (;;)
    {
        timer.expires_after(interval);
        co_await timer.async_wait(boost::asio::use_awaitable);

        auto mons = registry.All();
        if (mons.empty())
            continue;

        // Cap the per-tick fan-out: shuffle and take a bounded slice so
        // the channel-wide broadcast stays bounded regardless of how many
        // monsters exist.
        std::shuffle(mons.begin(), mons.end(), rng);
        const std::size_t n = std::min(max_per_tick, mons.size());

        for (std::size_t i = 0; i < n; ++i)
        {
            const auto& m = mons[i];
            const float nx = m.fPosX + step(rng);
            const float nz = m.fPosZ + step(rng);
            registry.UpdatePosition(m.dwInstanceID, nx, m.fPosY, nz);

            // Only spend wire on monsters someone can see.
            std::vector<std::shared_ptr<tnetlib::AsioSession>> watchers;
            presence.ForEachInChannel(m.bChannel, /*skip=*/0,
                [&](const ChannelPresenceEntry&,
                    std::shared_ptr<tnetlib::AsioSession> ws)
                { watchers.push_back(std::move(ws)); });
            if (watchers.empty())
                continue;

            const auto mv = EncodeMonMoveAck(m.dwInstanceID, nx, m.fPosY, nz,
                                             /*dir=*/0, /*action=*/0);
            for (auto& w : watchers)
                co_await w->SendPacket(
                    static_cast<std::uint16_t>(MessageId::CS_MONMOVE_ACK), mv);
        }
    }
}

} // namespace tmapsvr
