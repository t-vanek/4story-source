#pragma once

// LocalSpawnManager — timer-based monster lifecycle.
//
// Responsibilities:
//   1. On Start(): spawn initial monsters at all registered spawn points.
//   2. On monster death (OnMonsterDied): schedule a respawn after the
//      spawn point's delay_ms.
//   3. Per-monster roam AI coroutine: every 3–7 s, moves the monster to
//      a random position within `spawn.range` × CELL_SIZE of its origin
//      and broadcasts CS_MOVE_ACK to AOI players.
//
// All coroutines run on the same io_context (single-threaded).
// Weak ownership: each coroutine checks if the monster still exists
// before acting — dead monsters' coroutines exit silently.
//
// Source pattern mirrors legacy:
//   CTAICmdRoam  — random-walk AI command
//   CTMonSpawn   — respawn tracking in TMap.cpp

#include "monster_state.h"
#include "map_state.h"
#include "services/session_registry.h"
#include "level_chart.h"
#include "legacy_port/types.h"
#include "handlers_combat.h"
#include "handlers_map.h"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>
#include <cstdlib>
#include <functional>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

namespace tmapsvr {

class ISpawnManager
{
public:
    virtual ~ISpawnManager() = default;

    // Spawn initial monsters + launch roam AI. Called once after server
    // is ready to accept connections.
    virtual void Start() = 0;

    // Called by OnActionReq when a monster's HP reaches 0. Schedules
    // a respawn coroutine for the spawn point that created the monster.
    virtual void OnMonsterDied(std::uint32_t instance_id,
                               std::uint16_t spawn_id) = 0;

    // Register a spawn point. Must be called before Start().
    virtual void AddSpawnPoint(const legacy::MonsterSpawn& spawn,
                               std::uint8_t               level) = 0;
};

// ---------------------------------------------------------------------------
// LocalSpawnManager
// ---------------------------------------------------------------------------

class LocalSpawnManager : public ISpawnManager
{
public:
    LocalSpawnManager(boost::asio::io_context& io,
                      IMonsterRegistry&        monsters,
                      IMapState&               map_state,
                      ISessionRegistry&        sessions,
                      ILevelChart&             level_chart)
        : m_io(io)
        , m_monsters(monsters)
        , m_map_state(map_state)
        , m_sessions(sessions)
        , m_level_chart(level_chart)
    {}

    void AddSpawnPoint(const legacy::MonsterSpawn& spawn,
                       std::uint8_t               level) override
    {
        SpawnEntry e{};
        e.spawn = spawn;
        e.level = level;
        m_spawn_entries.push_back(std::move(e));
    }

    void Start() override
    {
        for (auto& e : m_spawn_entries)
            for (std::uint8_t i = 0; i < e.spawn.count; ++i)
                SpawnOne(e);
        spdlog::info("spawn_manager: started {} spawn points, {} monsters total",
            m_spawn_entries.size(), m_monsters.Count());
    }

    void OnMonsterDied(std::uint32_t instance_id,
                       std::uint16_t spawn_id) override
    {
        // Unmap instance → spawn entry
        auto it = m_instance_to_spawn.find(instance_id);
        if (it != m_instance_to_spawn.end())
        {
            const std::size_t idx = it->second;
            m_instance_to_spawn.erase(it);
            if (idx < m_spawn_entries.size())
            {
                auto& e = m_spawn_entries[idx];
                spdlog::info("spawn_manager: instance {} died, respawn in {}ms",
                    instance_id, e.spawn.delay_ms);
                ScheduleRespawn(idx, std::chrono::milliseconds(e.spawn.delay_ms));
            }
        }
    }

private:
    struct SpawnEntry {
        legacy::MonsterSpawn spawn{};
        std::uint8_t         level = 1;
    };

    void SpawnOne(SpawnEntry& e)
    {
        const auto id     = m_monsters.NextInstanceId();
        const auto stats  = m_level_chart.GetMonsterStats(e.level);
        const float jitter = static_cast<float>(e.spawn.range * CELL_SIZE);

        MonsterState mon{};
        mon.instance_id = id;
        mon.template_id = e.spawn.template_id;
        mon.level       = e.level;
        mon.max_hp = mon.hp = stats.max_hp;
        mon.max_mp = mon.mp = stats.max_mp;
        mon.pos_x   = e.spawn.pos_x + RandomJitter(jitter);
        mon.pos_y   = e.spawn.pos_y;
        mon.pos_z   = e.spawn.pos_z + RandomJitter(jitter);
        mon.dir     = e.spawn.dir;
        mon.country = e.spawn.country;
        mon.spawn_id = e.spawn.id;
        m_monsters.Add(mon);

        // Track instance → spawn entry index for OnMonsterDied routing
        const std::size_t idx =
            static_cast<std::size_t>(&e - m_spawn_entries.data());
        m_instance_to_spawn[id] = idx;

        // Broadcast CS_ADDMON_ACK to AOI players
        BroadcastAddMon(id, mon);

        // Start roam AI coroutine
        if (e.spawn.roam_type != 0 || e.spawn.range > 0)
            LaunchRoam(id, e.spawn);

        spdlog::debug("spawn_manager: spawned monster {} (tpl={} lv={}) "
                      "at ({:.0f},{:.0f})",
            id, mon.template_id, mon.level, mon.pos_x, mon.pos_z);
    }

    void BroadcastAddMon(std::uint32_t id, const MonsterState& mon)
    {
        // This is called from Start() (non-coroutine context). We co_spawn
        // the actual send to stay within the io_context executor.
        const auto pids = m_map_state.GetNeighborIds(mon.pos_x, mon.pos_z);
        for (std::uint32_t pid : pids)
        {
            auto sess = m_sessions.Get(pid);
            if (!sess) continue;
            const MonsterState mon_copy = mon;
            boost::asio::co_spawn(m_io,
                [this, sess, mon_copy]() -> boost::asio::awaitable<void> {
                    co_await SendAddMonAck(sess, mon_copy, true);
                },
                boost::asio::detached);
        }
    }

    void ScheduleRespawn(std::size_t spawn_idx, std::chrono::milliseconds delay)
    {
        boost::asio::co_spawn(m_io,
            [this, spawn_idx, delay]() -> boost::asio::awaitable<void> {
                auto timer = std::make_shared<boost::asio::steady_timer>(m_io);
                timer->expires_after(delay);
                boost::system::error_code ec;
                co_await timer->async_wait(
                    boost::asio::redirect_error(
                        boost::asio::use_awaitable, ec));
                if (!ec && spawn_idx < m_spawn_entries.size())
                    SpawnOne(m_spawn_entries[spawn_idx]);
            },
            boost::asio::detached);
    }

    void LaunchRoam(std::uint32_t instance_id,
                    const legacy::MonsterSpawn& spawn)
    {
        const float ox = spawn.pos_x, oz = spawn.pos_z;
        const float range = static_cast<float>(spawn.range) * CELL_SIZE;

        boost::asio::co_spawn(m_io,
            [this, instance_id, ox, oz, range]()
                -> boost::asio::awaitable<void>
            {
                co_await RoamLoop(instance_id, ox, oz, range);
            },
            boost::asio::detached);
    }

    boost::asio::awaitable<void>
    RoamLoop(std::uint32_t instance_id,
             float origin_x, float origin_z, float range)
    {
        constexpr int ROAM_INTERVAL_MIN_S = 3;
        constexpr int ROAM_INTERVAL_MAX_S = 7;

        while (true)
        {
            // Random pause between steps
            const int sec = ROAM_INTERVAL_MIN_S +
                (std::rand() % (ROAM_INTERVAL_MAX_S - ROAM_INTERVAL_MIN_S + 1));
            boost::asio::steady_timer t(m_io);
            t.expires_after(std::chrono::seconds(sec));
            boost::system::error_code ec;
            co_await t.async_wait(
                boost::asio::redirect_error(
                    boost::asio::use_awaitable, ec));
            if (ec) co_return;  // io stopped

            // Monster gone? (died or despawned)
            auto* mon = m_monsters.GetMutable(instance_id);
            if (!mon || mon->IsDead()) co_return;

            // Pick a random target position within range of origin
            const float r = range > 0 ? range : static_cast<float>(CELL_SIZE);
            const float new_x = origin_x + RandomJitter(r);
            const float new_z = origin_z + RandomJitter(r);

            const float old_x = mon->pos_x, old_z = mon->pos_z;
            mon->pos_x = new_x;
            mon->pos_z = new_z;
            mon->action = 2;  // RUN

            // Broadcast move to AOI players
            const auto pids = m_map_state.GetNeighborIds(new_x, new_z);
            for (std::uint32_t pid : pids)
            {
                auto sess = m_sessions.Get(pid);
                if (!sess) continue;
                co_await SendMoveAck(sess, instance_id,
                    new_x, mon->pos_y, new_z,
                    0, mon->dir, 0, 0, mon->action, 1.5f);
            }
            (void)old_x; (void)old_z;
        }
    }

    static float RandomJitter(float max_radius)
    {
        if (max_radius <= 0.0f) return 0.0f;
        const float r = max_radius * 2.0f;
        return (static_cast<float>(std::rand()) /
                static_cast<float>(RAND_MAX)) * r - max_radius;
    }

    static constexpr int CELL_SIZE = 64;

    // Wire helpers imported via handlers_combat.h + handlers_map.h
    // (SendAddMonAck, SendMoveAck declared there)

    boost::asio::io_context&                        m_io;
    IMonsterRegistry&                               m_monsters;
    IMapState&                                      m_map_state;
    ISessionRegistry&                               m_sessions;
    ILevelChart&                                    m_level_chart;
    std::vector<SpawnEntry>                         m_spawn_entries;
    std::unordered_map<std::uint32_t, std::size_t>  m_instance_to_spawn;
};

} // namespace tmapsvr
