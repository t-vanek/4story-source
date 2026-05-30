#include "services/monster_ai.h"

#include "domain/character.h"
#include "services/channel_presence.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/damage_formula.h"
#include "services/monster_chart.h"
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

int NearestPlayerIndex(float mx, float mz,
                       const std::vector<Position>& players, float range)
{
    const float r2 = range * range;
    int   best_i = -1;
    float best   = r2;
    for (int i = 0; i < static_cast<int>(players.size()); ++i)
    {
        const auto& p = players[static_cast<std::size_t>(i)];
        if (p.x == 0.f && p.z == 0.f) continue;   // not yet positioned
        const float dx = p.x - mx, dz = p.z - mz;
        const float d2 = dx * dx + dz * dz;
        if (d2 <= best) { best = d2; best_i = i; }
    }
    return best_i;
}

Move2D DecideMonsterMove(float mx, float mz,
                         const std::vector<Position>& players,
                         float aggro_range, float chase_step,
                         float roam_dx, float roam_dz)
{
    const int idx = NearestPlayerIndex(mx, mz, players, aggro_range);
    if (idx < 0)
        return { mx + roam_dx, mz + roam_dz, false };    // idle roam

    const Position& tgt = players[static_cast<std::size_t>(idx)];
    const float dx = tgt.x - mx, dz = tgt.z - mz;
    const float dist = std::sqrt(dx * dx + dz * dz);
    if (dist <= chase_step || dist == 0.f)
        return { tgt.x, tgt.z, true };                   // close enough — land on it
    const float t = chase_step / dist;
    return { mx + dx * t, mz + dz * t, true };            // step toward it
}

boost::asio::awaitable<void>
RunMonsterAi(IMonsterRegistry&         registry,
             IChannelPresence&         presence,
             ICharStateStore&          char_state,
             IMonsterChart&            monsters,
             std::chrono::milliseconds interval,
             std::size_t               max_per_tick)
{
    using tnetlib::protocol::MessageId;

    std::mt19937 rng{ std::random_device{}() };
    std::uniform_real_distribution<float> step(-3.0f, 3.0f);

    boost::asio::steady_timer timer(co_await boost::asio::this_coro::executor);

    spdlog::info("monster_ai: roam/chase/attack tick every {}ms, up to {} "
                 "monster(s) per tick", interval.count(), max_per_tick);

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

        constexpr float kAggroRange  = 30.0f;  // notice players within this
        constexpr float kChaseStep   = 2.5f;   // chase speed (units / tick)
        constexpr float kAttackRange = 4.0f;   // melee reach

        for (std::size_t i = 0; i < n; ++i)
        {
            const auto& m = mons[i];

            // Watchers (char id + position + session) on the monster's
            // channel. Only bother with monsters someone can see.
            std::vector<Position>      player_pos;
            std::vector<std::uint32_t> player_cid;
            std::vector<std::shared_ptr<tnetlib::AsioSession>> watchers;
            presence.ForEachInChannel(m.bChannel, /*skip=*/0,
                [&](const ChannelPresenceEntry& e,
                    std::shared_ptr<tnetlib::AsioSession> ws)
                {
                    player_pos.push_back(e.pos);
                    player_cid.push_back(e.char_id);
                    watchers.push_back(std::move(ws));
                });
            if (watchers.empty())
                continue;

            // Chase the nearest player in aggro range, else roam.
            const Move2D mv = DecideMonsterMove(m.fPosX, m.fPosZ, player_pos,
                kAggroRange, kChaseStep, step(rng), step(rng));
            registry.UpdatePosition(m.dwInstanceID, mv.x, m.fPosY, mv.z);

            const auto move = EncodeMonMoveAck(m.dwInstanceID, mv.x, m.fPosY,
                mv.z, /*dir=*/0, /*action=*/0);
            for (auto& w : watchers)
                co_await w->SendPacket(
                    static_cast<std::uint16_t>(MessageId::CS_MONMOVE_ACK), move);

            // Attack: a player now within melee reach takes damage. Roll
            // the monster's real attack power (TMONATTRCHART wAP + weapon
            // range, carried on the instance from spawn) through the same
            // physical formula as player→monster. The player's defense is
            // 0 until the equipment/stat layer gives players a real DP, so
            // the formula floors (5/7) apply. HP is floored at 1 — player
            // death + revival (CS_REVIVAL) is the next wave.
            const int ti = NearestPlayerIndex(mv.x, mv.z, player_pos,
                                              kAttackRange);
            if (ti < 0)
                continue;
            if (!monsters.Find(m.wTemplateID))   // no template → not a fighter
                continue;

            auto rand_below = [&](std::uint32_t k) -> std::uint32_t
            {
                return k > 1
                    ? std::uniform_int_distribution<std::uint32_t>(0, k - 1)(rng)
                    : 0u;
            };
            const std::uint32_t dmg = RollPhysicalDamage(
                static_cast<std::uint32_t>(m.wAP) + m.wMinWAP,
                static_cast<std::uint32_t>(m.wAP) + m.wMaxWAP,
                /*player defense=*/0u, rand_below);
            const std::uint32_t victim = player_cid[static_cast<std::size_t>(ti)];

            char_state.Update(victim, [&](CharSnapshot& s)
                { s.dwHP = (s.dwHP > dmg) ? (s.dwHP - dmg) : 1u; });

            const auto vs = char_state.Get(victim);
            if (!vs || vs->dwMaxHP == 0)
                continue;
            const auto hp = EncodeHpMpAck(victim, vs->dwMaxHP, vs->dwHP, 0, 0);
            for (auto& w : watchers)
                co_await w->SendPacket(
                    static_cast<std::uint16_t>(MessageId::CS_HPMP_ACK), hp);
        }
    }
}

} // namespace tmapsvr
