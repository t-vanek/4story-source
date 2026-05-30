// Entry point for the modernized TMapSvrAsio binary.
//
// Phase F17: control protocol + ops endpoints. Five CT_* ids land
// in dispatch as stubs (announce, kickout, service monitor + data
// clear, control-server handshake). Optional /healthz HTTP endpoint
// spawns alongside the listener when [health].port is set in TOML —
// k8s liveness probes and load balancers hit it. UDP audit shim +
// admin TCP shell wait for the consolidation pass.
//
// This commit closes the F1-F17 scaffolding sweep — all major data
// loaders, services, and dispatch slots exist. See the audit blurb
// in CMakeLists.txt for the documented TODOs that the consolidation
// pass picks up.

#include "audit/audit_log.h"
#include "config.h"
#include "handlers_world.h"
#include "map_server.h"
#include "ops/admin_shell.h"
#include "ops/metrics.h"
#include "ops/metrics_endpoint.h"
#include "db/schema_validator.h"
#include "services/channel_presence.h"
#include "services/char_state_store.h"
#include "services/companion_service.h"
#include "services/inventory_service.h"
#include "services/log_peer.h"
#include "services/map_mon_chart.h"
#include "services/mapper_profiles.h"
#include "services/mon_attr_chart.h"
#include "services/monster_ai.h"
#include "services/monster_chart.h"
#include "services/monster_registry.h"
#include "services/npc_service.h"
#include "services/rate_limiter.h"
#include "services/player_service.h"
#include "services/quest_service.h"
#include "services/session_registry.h"
#include "services/session_validator.h"
#include "services/skill_service.h"
#include "services/soci_companion_service.h"
#include "services/soci_inventory_service.h"
#include "services/soci_monster_chart.h"
#include "services/soci_npc_service.h"
#include "services/soci_player_service.h"
#include "services/soci_quest_service.h"
#include "services/soci_session_validator.h"
#include "services/soci_map_mon_chart.h"
#include "services/soci_mon_attr_chart.h"
#include "services/soci_skill_service.h"
#include "services/skill_chart.h"
#include "services/skill_cooldown.h"
#include "services/soci_skill_chart.h"
#include "services/soci_spawn_chart.h"
#include "services/spawn_chart.h"
#include "services/spawn_manager.h"
#include "services/world_client.h"

#include "fourstory/cluster/peer_client.h"
#include "fourstory/db/session_pool.h"
#include "fourstory/ops/health_endpoint.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/thread_pool.hpp>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>
#include <exception>
#include <atomic>
#include <memory>
#include <string>
#include <utility>

namespace {

void Usage()
{
    std::printf(
        "tmapsvr_asio — modernized 4Story map server (phase F17 scaffold)\n"
        "Usage: tmapsvr_asio [--config FILE] [--help]\n"
        "  --config FILE   TOML config (default: tmapsvr.toml)\n");
}

} // namespace

int main(int argc, char** argv)
{
    std::string config_path = "tmapsvr.toml";
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)
            config_path = argv[++i];
        else if (std::strcmp(argv[i], "--help") == 0 ||
                 std::strcmp(argv[i], "-h")     == 0)
        {
            Usage();
            return 0;
        }
    }

    try
    {
        auto cfg = tmapsvr::LoadConfig(config_path);
        spdlog::set_level(cfg.log_level);

        boost::asio::io_context io;

        // T5: shutdown sequence is wired below — accept loop close,
        // drain window, then io.stop(). The signal handler captures
        // a pointer to the MapServer that we can't construct yet;
        // declared here and filled in once the server is built.
        tmapsvr::MapServer* server_ptr = nullptr;
        const auto drain = std::chrono::milliseconds(cfg.shutdown.drain_ms);

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io, &server_ptr, drain](auto, int sig) {
            spdlog::info("received signal {}, beginning graceful shutdown "
                         "(drain {}ms)",
                sig, static_cast<long long>(drain.count()));
            if (server_ptr) server_ptr->StopAccepting();

            // Sleep drain ms via a steady_timer so in-flight handlers
            // and outbound peer sends have a chance to finish before
            // io.stop() yanks the executor.
            auto t = std::make_shared<boost::asio::steady_timer>(io);
            t->expires_after(drain);
            t->async_wait([&io, t](auto) {
                spdlog::info("graceful shutdown drain elapsed — stopping io_context");
                io.stop();
            });
        });

        // Optional SOCI pool — without one, no validators / services
        // come up. The listener still binds so dev runs can netcat-
        // poke the wire codec, but every handler that needs DB data
        // refuses with INTERNAL.
        std::unique_ptr<fourstory::db::SessionPool>     pool;
        std::unique_ptr<boost::asio::thread_pool>       db_thread_pool;
        std::unique_ptr<tmapsvr::IMapSessionValidator>  validator;
        std::unique_ptr<tmapsvr::IPlayerService>        player_service;
        std::unique_ptr<tmapsvr::IInventoryService>     inventory_service;
        std::unique_ptr<tmapsvr::INpcService>           npc_service;
        std::unique_ptr<tmapsvr::ISkillService>         skill_service;
        std::unique_ptr<tmapsvr::IQuestService>         quest_service;
        std::unique_ptr<tmapsvr::IMonsterChart>         monster_chart;
        std::unique_ptr<tmapsvr::ISpawnChart>           spawn_chart;
        std::unique_ptr<tmapsvr::IMapMonChart>          map_mon_chart;
        std::unique_ptr<tmapsvr::IMonAttrChart>         mon_attr_chart;
        std::unique_ptr<tmapsvr::ISkillTemplateChart>   skill_chart;
        std::unique_ptr<tmapsvr::ICompanionService>     companion_service;

        // Configure the fourstory::mapper Automapper once at startup
        // (mirrors TWorldSvrAsio / TControlSvrAsio). CharMappingProfile
        // wires CharRow→CharSnapshot so SociPlayerService maps the
        // TCHARTABLE row to the domain snapshot without hand-written
        // narrowing. Registered unconditionally — harmless on the no-DB
        // dev path, keeps the bootstrap identical on both branches.
        {
            auto& reg = fourstory::mapper::MapperRegistry::Get();
            if (!reg.Applied())
            {
                reg.Register<tmapsvr::CharMappingProfile>();
                reg.ApplyAll();
                spdlog::info("mapper: {} profile(s) applied", reg.Count());
            }
        }

        if (!cfg.database.connection_string.empty())
        {
            if (cfg.database.backend.empty())
                throw std::runtime_error("database.connection_string set but database.backend empty");
            const auto backend = fourstory::db::ParseBackend(cfg.database.backend);
            pool = std::make_unique<fourstory::db::SessionPool>(
                backend, cfg.database.connection_string, cfg.database.pool_size);
            if (cfg.database.worker_threads > 0)
                db_thread_pool = std::make_unique<boost::asio::thread_pool>(
                    cfg.database.worker_threads);
            tmapsvr::db::ValidateUserSchema(*pool);
            tmapsvr::db::ValidateCharSchema(*pool);
            tmapsvr::db::ValidateInventorySchema(*pool);
            tmapsvr::db::ValidateNpcSchema(*pool);
            tmapsvr::db::ValidateSkillSchema(*pool);
            tmapsvr::db::ValidateQuestSchema(*pool);
            tmapsvr::db::ValidateMonsterSchema(*pool);
            tmapsvr::db::ValidateCompanionSchema(*pool);
            validator         = std::make_unique<tmapsvr::SociMapSessionValidator>(*pool);
            player_service    = std::make_unique<tmapsvr::SociPlayerService>(*pool);
            inventory_service = std::make_unique<tmapsvr::SociInventoryService>(*pool);
            npc_service       = std::make_unique<tmapsvr::SociNpcService>(*pool);
            skill_service     = std::make_unique<tmapsvr::SociSkillService>(*pool);
            quest_service     = std::make_unique<tmapsvr::SociQuestService>(*pool);
            monster_chart     = std::make_unique<tmapsvr::SociMonsterChart>(*pool);
            spawn_chart       = std::make_unique<tmapsvr::SociSpawnChart>(*pool);
            map_mon_chart     = std::make_unique<tmapsvr::SociMapMonChart>(*pool);
            mon_attr_chart    = std::make_unique<tmapsvr::SociMonAttrChart>(*pool);
            skill_chart       = std::make_unique<tmapsvr::SociSkillChart>(*pool);
            companion_service = std::make_unique<tmapsvr::SociCompanionService>(*pool);
            spdlog::info("schema OK ({}) — services ready: {} NPC, {} monster "
                         "template(s), {} spawn point(s), {} spawn-mon link(s), "
                         "{} mon-attr row(s)",
                fourstory::db::BackendName(backend),
                npc_service->Size(),
                monster_chart->Size(),
                spawn_chart->Size(),
                map_mon_chart->Size(),
                mon_attr_chart->Size());
        }
        else
        {
            spdlog::warn("no [database] configured — CS_CONNECT_REQ and "
                         "DM_LOADCHAR_REQ will refuse with INTERNAL, "
                         "CS_NPCTALK_REQ / CS_SKILLUSE_REQ / CS_QUEST*_REQ "
                         "will silently drop, monster registry stays empty");
        }

        // In-memory monster registry. Populated once at boot by the
        // static SpawnManager (below) from the spawn / map-mon / monster
        // charts; the respawn timer + roam/chase/attack AI tick land with
        // the next phase. Empty when no DB is configured.
        tmapsvr::InMemoryMonsterRegistry monster_reg;

        // Static monster spawn — realise the world's standing monster
        // population on channel 0. Needs all three charts (DB path only).
        // The CS_CONREADY enter-map flood broadcasts these via the
        // registry's ListInMap. Multi-channel replication + respawn + AI
        // are the next increments.
        std::uint32_t monster_boot_seq = 1;
        if (spawn_chart && map_mon_chart && monster_chart && mon_attr_chart)
        {
            tmapsvr::SpawnAllStatic(*spawn_chart, *map_mon_chart,
                *monster_chart, *mon_attr_chart, monster_reg,
                monster_boot_seq, /*channel=*/0);
        }

        // Shared, thread-safe allocator for runtime monster instance ids
        // (respawn), continuing past the ids the boot population used.
        std::atomic<std::uint32_t> monster_seq{monster_boot_seq};

        // T3: UDP audit sink (TLogSvrAsio collector). Empty host /
        // port=0 disables the peer; events still go to spdlog. T4
        // observability builds the structured audit log on top.
        tmapsvr::UdpLogPeer    log_peer(io, cfg.audit.host, cfg.audit.port);

        // T4: structured audit emitter (mirrors to spdlog + sends
        // POD events to the UDP log peer when configured).
        tmapsvr::audit::AuditLog audit_log(&log_peer);

        // T4: metrics registry — counters + latency per handler /
        // per DB query. Snapshots will feed a /metrics endpoint in
        // a follow-up commit.
        tmapsvr::ops::Metrics    metrics;

        // T5: per-session rate limiter. Default config (burst=0,
        // refill=0) disables the gate so dev runs aren't affected;
        // production sets sensible values in [rate_limit].
        tmapsvr::TokenBucketLimiter rate_limiter(
            cfg.rate_limit.burst, cfg.rate_limit.refill_per_s);

        // char_id → AsioSession map. Lives for the io.run() duration,
        // bound at CS_CONNECT_REQ success, unbound by the MapServer
        // per-connection teardown. Consulted by handlers_world to
        // route DM_/MW_ inbound traffic back to the right client.
        tmapsvr::InMemorySessionRegistry session_reg;

        // Per-channel presence (char_id → channel + position + session).
        // Driven by the same handshake / teardown pair as session_reg.
        // CS_MOVE_REQ updates position and broadcasts to other
        // channel members.
        tmapsvr::InMemoryChannelPresence presence;

        // Live char snapshot store — populated on DM_LOADCHAR_REQ,
        // read by the MapServer teardown hook to call SaveChar.
        tmapsvr::InMemoryCharStateStore char_state;

        // Per-(char, skill) cooldown gate — records last-use stamps so the
        // skill handler rejects re-uses faster than TSKILLCHART's reuse
        // delay (legacy CTSkill::CanUse). Lives for the io.run() duration.
        tmapsvr::SkillCooldownTracker skill_cooldown;

        // Build the HandlerContext now so the world inbound dispatch
        // lambda can capture it by reference (the context's pointer
        // fields are filled in below as each service comes online).
        tmapsvr::HandlerContext ctx{};
        ctx.validator         = validator.get();
        ctx.session_reg       = &session_reg;
        ctx.presence          = &presence;
        ctx.player_service    = player_service.get();
        ctx.inventory_service = inventory_service.get();
        ctx.npc_service       = npc_service.get();
        ctx.skill_service     = skill_service.get();
        ctx.quest_service     = quest_service.get();
        ctx.monster_chart     = monster_chart.get();
        ctx.skill_chart       = skill_chart.get();
        ctx.skill_cooldown    = &skill_cooldown;
        ctx.spawn_chart       = spawn_chart.get();
        ctx.monster_registry  = &monster_reg;
        ctx.monster_seq       = &monster_seq;
        ctx.companion_service = companion_service.get();
        ctx.char_state        = &char_state;
        ctx.db_pool           = db_thread_pool.get();
        ctx.log_peer          = &log_peer;
        ctx.audit             = &audit_log;
        ctx.metrics           = &metrics;
        ctx.rate_limiter      = &rate_limiter;
        ctx.mode              = cfg.mode;

        // Optional World peer — only spun up when [world] port is set
        // in the TOML. Without it, MW_ADDCHAR_ACK after a clean
        // handshake gets logged as deferred and never sent (no buffer
        // in F5). Dev runs that only need to exercise the local
        // dispatch can leave [world].port = 0.
        std::unique_ptr<tmapsvr::AsioWorldClient> world_client;
        if (cfg.world.port != 0)
        {
            // Inbound dispatch — synchronous callback fired from the
            // world's read loop; copy the body (the span is only
            // valid during the callback) and co_spawn the awaitable
            // DispatchWorld detached so SendPacket calls don't block
            // the read.
            auto on_world_packet =
                [&io, &ctx](std::uint16_t wId,
                            std::span<const std::byte> body)
                {
                    std::vector<std::byte> owned(body.begin(), body.end());
                    boost::asio::co_spawn(
                        io,
                        tmapsvr::DispatchWorld(wId, std::move(owned), ctx),
                        [wId](std::exception_ptr ep) {
                            if (!ep) return;
                            try { std::rethrow_exception(ep); }
                            catch (const std::exception& ex) {
                                spdlog::error("world dispatch threw (wId=0x{:04X}): {}",
                                    wId, ex.what());
                            }
                            catch (...) {
                                spdlog::error("world dispatch threw unknown (wId=0x{:04X})",
                                    wId);
                            }
                        });
                };
            world_client = std::make_unique<tmapsvr::AsioWorldClient>(
                io, cfg.world.host, cfg.world.port,
                std::move(on_world_packet));
            // Map identity advertised to TWorld via RW_RELAYSVR_REQ on
            // connect. Convention: LOBYTE = server_id, HIBYTE = group_id
            // (TWorld derives main_server_id = LOBYTE(wID)). A zero wid
            // (both ids unset) leaves the link anonymous — log a warning
            // so a misconfigured deploy is visible.
            const std::uint16_t relay_wid = static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(cfg.cluster.group_id) << 8) |
                 static_cast<std::uint16_t>(cfg.cluster.server_id));
            if (relay_wid == 0)
                spdlog::warn("world_client: [server] group_id/server_id "
                             "unset — relay wid=0, TWorld link stays "
                             "anonymous (no MW routing back)");
            world_client->SetRelayWid(relay_wid);
            boost::asio::co_spawn(io, world_client->Run(), boost::asio::detached);
            spdlog::info("world_client: dialing {}:{} as wid=0x{:04X} "
                         "(background)",
                cfg.world.host, cfg.world.port, relay_wid);
        }
        else
        {
            spdlog::warn("no [world] port configured — MW_ADDCHAR_ACK and "
                         "future DM_/MW_ traffic will be skipped");
        }

        // Now that the world client (if any) is built we can fill the
        // last pointer; assigning before construction would have
        // captured a nullptr by reference into the dispatch lambda.
        ctx.world_client = world_client.get();
        cfg.server.handlers = ctx;

        const bool crypto_on = !cfg.server.rc4_secret_key.empty();
        const auto mode_name = tmapsvr::ModeName(cfg.mode);
        tmapsvr::MapServer server(io, std::move(cfg.server));
        server_ptr = &server;   // wire up the signal handler's pointer
        spdlog::info("tmapsvr_asio: F17 listener on 0.0.0.0:{} (mode={}, crypto={}) — "
                     "send SIGINT/SIGTERM to exit",
                     server.Port(), mode_name,
                     crypto_on ? "on" : "off");
        boost::asio::co_spawn(io, server.Run(), boost::asio::detached);

        // Monster AI — the roam / chase / attack tick. Detached; runs for
        // the life of the io_context. Needs the monster chart (attack
        // level) + char state (player HP); without a DB there are no
        // monsters anyway, so it's only spun up when the chart loaded.
        if (monster_chart)
        {
            boost::asio::co_spawn(io,
                tmapsvr::RunMonsterAi(monster_reg, presence, char_state,
                    *monster_chart),
                [](std::exception_ptr ep)
                {
                    if (!ep) return;
                    try { std::rethrow_exception(ep); }
                    catch (const std::exception& ex)
                    { spdlog::error("monster_ai tick stopped: {}", ex.what()); }
                    catch (...) { spdlog::error("monster_ai tick stopped (unknown)"); }
                });
        }

        // Optional /healthz HTTP endpoint on a separate port. Same
        // pattern as TPatchSvrAsio / TLoginSvrAsio — warn rather
        // than abort if the port is already taken (dev environments
        // commonly clash on these), so the main listener can still
        // come up.
        std::unique_ptr<fourstory::ops::HealthEndpoint> health;
        if (cfg.health_port != 0)
        {
            try
            {
                health = std::make_unique<fourstory::ops::HealthEndpoint>(
                    io, cfg.health_port);
                spdlog::info("health endpoint listening on 0.0.0.0:{}",
                    health->Port());
                boost::asio::co_spawn(io, health->Run(),
                    boost::asio::detached);
            }
            catch (const std::exception& ex)
            {
                spdlog::warn("health endpoint failed to bind on port {}: {}",
                    cfg.health_port, ex.what());
            }
        }

        // T6: Prometheus /metrics endpoint. Loopback only; separate
        // from /healthz so a slow scrape doesn't degrade the
        // liveness probe.
        std::unique_ptr<tmapsvr::ops::MetricsEndpoint> metrics_ep;
        if (cfg.metrics_port != 0)
        {
            metrics_ep = std::make_unique<tmapsvr::ops::MetricsEndpoint>(
                io, cfg.metrics_port, metrics);
            if (metrics_ep->Port() != 0)
                boost::asio::co_spawn(io, metrics_ep->Run(),
                    boost::asio::detached);
        }

        // T6: admin TCP shell. Disabled by default (port=0 in the
        // example TOML); operators enable per deployment.
        std::unique_ptr<tmapsvr::ops::AdminShell> admin;
        if (cfg.admin.port != 0)
        {
            tmapsvr::ops::AdminShellConfig admin_cfg{
                cfg.admin.bind, cfg.admin.port, cfg.admin.secret};
            admin = std::make_unique<tmapsvr::ops::AdminShell>(
                io, std::move(admin_cfg), ctx);
            if (admin->Port() != 0)
                boost::asio::co_spawn(io, admin->Run(),
                    boost::asio::detached);
        }

        io.run();

        // Wait for any in-flight SaveChar calls to complete before
        // the pool is destroyed (same pattern as TControlSvrAsio).
        if (db_thread_pool)
        {
            db_thread_pool->stop();
            db_thread_pool->join();
        }


    }
    catch (const std::exception& ex)
    {
        spdlog::critical("fatal: {}", ex.what());
        return 1;
    }
    return 0;
}
