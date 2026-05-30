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
#include <cmath>
#include <memory>
#include <random>
#include <vector>

namespace tmapsvr {

Move2D DecideMonsterMove(float mx, float mz,
                         const std::vector<Position>& players,
                         float aggro_range, float chase_step,
                         float roam_dx, float roam_dz)
{
    const float aggro2 = aggro_range * aggro_range;
    const Position* nearest = nullptr;
    float best = aggro2;
    for (const auto& p : players)
    {
        if (p.x == 0.f && p.z == 0.f) continue;   // not yet positioned
        const float dx = p.x - mx, dz = p.z - mz;
        const float d2 = dx * dx + dz * dz;
        if (d2 <= best) { best = d2; nearest = &p; }
    }

    if (!nearest)
        return { mx + roam_dx, mz + roam_dz, false };   // idle roam

    const float dx = nearest->x - mx, dz = nearest->z - mz;
    const float dist = std::sqrt(dx * dx + dz * dz);
    if (dist <= chase_step || dist == 0.f)
        return { nearest->x, nearest->z, true };        // close enough — land on it
    const float t = chase_step / dist;
    return { mx + dx * t, mz + dz * t, true };           // step toward it
}

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

        constexpr float kAggroRange = 30.0f;   // notice players within this
        constexpr float kChaseStep  = 2.5f;    // chase speed (units / tick)

        for (std::size_t i = 0; i < n; ++i)
        {
            const auto& m = mons[i];

            // Watchers + their positions on the monster's channel. Only
            // spend wire (and bother moving) for monsters someone can see.
            std::vector<Position> player_pos;
            std::vector<std::shared_ptr<tnetlib::AsioSession>> watchers;
            presence.ForEachInChannel(m.bChannel, /*skip=*/0,
                [&](const ChannelPresenceEntry& e,
                    std::shared_ptr<tnetlib::AsioSession> ws)
                {
                    player_pos.push_back(e.pos);
                    watchers.push_back(std::move(ws));
                });
            if (watchers.empty())
                continue;

            // Chase the nearest player in aggro range, else roam.
            const Move2D mv = DecideMonsterMove(m.fPosX, m.fPosZ, player_pos,
                kAggroRange, kChaseStep, step(rng), step(rng));
            registry.UpdatePosition(m.dwInstanceID, mv.x, m.fPosY, mv.z);

            const auto packet = EncodeMonMoveAck(m.dwInstanceID, mv.x, m.fPosY,
                mv.z, /*dir=*/0, /*action=*/0);
            for (auto& w : watchers)
                co_await w->SendPacket(
                    static_cast<std::uint16_t>(MessageId::CS_MONMOVE_ACK),
                    packet);
        }
    }
}

} // namespace tmapsvr
