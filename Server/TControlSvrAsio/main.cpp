// Entry point for the modernized TControlSvrAsio binary. F1 brought
// up the scaffold + operator login; F2 layers on the SOCI service
// inventory + auth, peer dialer, peer registry, monitoring timers,
// and the schema validator.

#include "admin_shell.h"
#include "config.h"
#include "control_server.h"
#include "message_router.h"
#include "event_scheduler.h"
#include "peer_dialer.h"
#include "db/schema_validator.h"
#include "services/alerter.h"
#include "services/chat_ban_repository.h"
#include "services/disabled_service_controller.h"
#include "services/event_registry.h"
#include "services/event_repository.h"
#include "services/fake_event_repository.h"
#include "services/fake_operator_auth_service.h"
#include "services/fake_patch_metadata_service.h"
#include "services/fake_service_inventory.h"
#include "services/fake_user_protected_service.h"
#include "services/operator_auth_service.h"
#include "services/patch_metadata_service.h"
#include "services/mapper_profiles.h"
#include "services/peer_registry.h"
#include "services/service_inventory.h"
#include "services/soci_admin_audit_logger.h"
#include "services/soci_alerter.h"
#include "services/soci_event_repository.h"
#include "services/soci_operator_auth_service.h"
#include "services/soci_patch_metadata_service.h"
#include "services/soci_peer_repository.h"
#include "services/soci_service_inventory.h"
#include "services/soci_user_protected_service.h"
#include "services/spdlog_admin_audit_logger.h"
#include "services/spdlog_alerter.h"
#include "services/user_protected_service.h"

#include "fourstory/db/co_offload.h"
#include "fourstory/db/session_pool.h"
#include "fourstory/ops/health_endpoint.h"
#include "fourstory/ops/rate_limiter.h"
#include "fourstory/ops/registry_refresher.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/thread_pool.hpp>

#include "fourstory/mapper/mapper.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

namespace {
void Usage()
{
    std::printf(
        "tcontrolsvr_asio — modernized 4Story control / orchestration server\n"
        "Usage: tcontrolsvr_asio [--config FILE]\n");
}

// Pick the controller backend. F2 ships only the disabled default
// on Linux; the Windows SCM impl is gated behind FOURSTORY_HAS_WIN32
// (see services/windows_scm_service_controller.h once added).
std::unique_ptr<tcontrolsvr::IServiceController> MakeServiceController()
{
    return std::make_unique<tcontrolsvr::DisabledServiceController>();
}

} // namespace

int main(int argc, char** argv)
{
    std::string config_path = "tcontrolsvr.toml";
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)
            config_path = argv[++i];
        else if (std::strcmp(argv[i], "--help") == 0 ||
                 std::strcmp(argv[i], "-h") == 0)
        {
            Usage();
            return 0;
        }
    }

    try
    {
        auto cfg = tcontrolsvr::LoadConfig(config_path);
        spdlog::set_level(cfg.log_level);

        // Register fourstory::mapper profiles so handlers / admin shell
        // can flow RegistryEntry / ServiceInstance through Adapt<PeerStatusDto>().
        {
            using namespace fourstory::mapper;
            auto& reg = MapperRegistry::Get();
            if (!reg.Applied())
            {
                reg.Register<tcontrolsvr::ControlMappingProfile>();
                reg.ApplyAll();
                spdlog::info("mapper: {} profile(s) configured",
                    reg.Count());
            }
        }

        boost::asio::io_context io;

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](auto, int sig) {
            spdlog::info("received signal {}, shutting down", sig);
            io.stop();
        });

        // --- Auth + inventory --------------------------------------
        //
        // Prefer the SOCI-backed services when a [database] section
        // is present. Fall back to the in-memory fakes otherwise (F1
        // bring-up path).
        std::unique_ptr<fourstory::db::SessionPool>           pool;
        std::unique_ptr<boost::asio::thread_pool>             db_pool;
        std::unique_ptr<tcontrolsvr::IOperatorAuthService>    auth;
        std::unique_ptr<tcontrolsvr::IServiceInventory>       inventory_ptr;
        std::unique_ptr<tcontrolsvr::IUserProtectedService>   user_ban;
        std::unique_ptr<tcontrolsvr::IEventRepository>        event_repo;
        std::unique_ptr<tcontrolsvr::IPatchMetadataService>   patch_meta;
        std::unique_ptr<tcontrolsvr::IAlerter>                alerter;
        std::unique_ptr<tcontrolsvr::IPeerRepository>         peer_repo;
        std::unique_ptr<tcontrolsvr::IAdminAuditLogger>       audit_owned;

        if (!cfg.database.connection_string.empty())
        {
            if (cfg.database.backend.empty())
                throw std::runtime_error(
                    "database.connection_string set but database.backend empty");
            const auto backend =
                fourstory::db::ParseBackend(cfg.database.backend);
            pool = std::make_unique<fourstory::db::SessionPool>(
                backend, cfg.database.connection_string, cfg.database.pool_size);

            // Worker pool for synchronous SOCI calls. Sized off
            // cfg.database.worker_threads; 0 disables (legacy
            // F1–F5 behavior of running SOCI in-line on the
            // io_context). Production deploys with non-trivial DB
            // latency should keep this at 2+.
            if (cfg.database.worker_threads > 0)
            {
                db_pool = std::make_unique<boost::asio::thread_pool>(
                    cfg.database.worker_threads);
                spdlog::info("db worker pool: {} threads",
                    cfg.database.worker_threads);
            }
            else
            {
                spdlog::info("db worker pool: disabled "
                             "(SOCI calls run on io_context thread)");
            }

            // Fail-fast on missing inventory tables before we accept
            // operator traffic. Optional tables (TEVENTCHART etc.)
            // log warnings; required tables throw.
            tcontrolsvr::db::ValidateGlobalSchema(*pool);

            auto soci_inv =
                std::make_unique<tcontrolsvr::SociServiceInventory>(*pool);
            soci_inv->Reload();
            inventory_ptr = std::move(soci_inv);
            auth = std::make_unique<tcontrolsvr::SociOperatorAuthService>(*pool);
            user_ban =
                std::make_unique<tcontrolsvr::SociUserProtectedService>(*pool);
            event_repo =
                std::make_unique<tcontrolsvr::SociEventRepository>(*pool);
            patch_meta =
                std::make_unique<tcontrolsvr::SociPatchMetadataService>(*pool);
            // F5: SMS alerter via OPTool_SMSEmergency. Wired only
            // when the DB is configured — otherwise the spdlog
            // default catches the offline-peer event in logs.
            alerter =
                std::make_unique<tcontrolsvr::SociAlerter>(*pool);

            // Peer registry persistence — TPEER_REGISTRY / TPEER_STATUS_LOG
            // / TPEER_METRICS. Purge old rows on startup (30d status log, 7d
            // metrics), then restore prior registrations so PeerRegistry is
            // warm without waiting for every peer to re-register.
            auto soci_peer_repo =
                std::make_unique<tcontrolsvr::SociPeerRepository>(*pool);
            soci_peer_repo->PurgeOldRows(30, 7);
            peer_repo = std::move(soci_peer_repo);

            // Dual-sink audit logger: spdlog + TOP_AUDIT_LOG in DB.
            audit_owned =
                std::make_unique<tcontrolsvr::SociAdminAuditLogger>(*pool);

            spdlog::info("auth + inventory: SOCI ({}) ready",
                fourstory::db::BackendName(backend));
        }
        else
        {
            spdlog::info("no [database] — running with FakeAuth + FakeInventory");
            auto fake_auth =
                std::make_unique<tcontrolsvr::FakeOperatorAuthService>();
            for (const auto& op : cfg.fake_operators)
                fake_auth->AddOperator(op.id, op.password, op.authority);
            auth = std::move(fake_auth);

            auto fake_inv =
                std::make_unique<tcontrolsvr::FakeServiceInventory>();
            for (const auto& g : cfg.fake_inventory.groups)
                fake_inv->AddGroup({g.id, g.name});
            for (const auto& m : cfg.fake_inventory.machines)
                fake_inv->AddMachine({m.id, m.name, 0, {}, {}, ""});
            for (const auto& t : cfg.fake_inventory.types)
                fake_inv->AddType({t.id, 0, t.name});
            inventory_ptr = std::move(fake_inv);
            user_ban = std::make_unique<tcontrolsvr::FakeUserProtectedService>();
            event_repo = std::make_unique<tcontrolsvr::FakeEventRepository>();
            patch_meta = std::make_unique<tcontrolsvr::FakePatchMetadataService>();
        }
        if (!alerter)
            alerter = std::make_unique<tcontrolsvr::SpdlogAlerter>();

        // --- Admin audit + chat-ban registry + event registry -----
        // When DB is configured, audit_owned is a SociAdminAuditLogger
        // (dual-sink: spdlog + TOP_AUDIT_LOG). Fallback: stack-allocated
        // SpdlogAdminAuditLogger for dev/no-DB runs.
        tcontrolsvr::SpdlogAdminAuditLogger spdlog_audit_fallback;
        tcontrolsvr::IAdminAuditLogger& audit =
            audit_owned ? *audit_owned : spdlog_audit_fallback;
        tcontrolsvr::ChatBanRepository      chat_bans;
        tcontrolsvr::EventRegistry          events;
        if (event_repo) events.LoadFrom(event_repo->LoadAll());
        spdlog::info("event_registry: loaded {} events", events.Size());

        // --- Peer infra --------------------------------------------
        tcontrolsvr::PeerRegistry peers(*inventory_ptr);

        // Restore peer registrations saved before the last restart.
        // Entries expire naturally if the peer doesn't heartbeat within
        // the lease window — no special cleanup needed here.
        if (peer_repo)
        {
            const auto loaded = peer_repo->LoadAll();
            for (const auto& entry : loaded)
                peers.Register(entry);

            // Project recovered registrations through the Mapper into
            // the compact admin DTO so boot logs use the same shape
            // operators see in the admin shell `peers` command.
            const auto dtos =
                fourstory::mapper::AdaptAll<tcontrolsvr::PeerStatusDto>(loaded);
            spdlog::info("peer_repo: recovered {} registration(s) from DB",
                dtos.size());
            for (const auto& d : dtos)
            {
                spdlog::info("  service_id=0x{:06X} group={} type={} server={} "
                             "name='{}' addr={}:{} version='{}' users={}/{} "
                             "lease={}",
                    d.service_id, static_cast<int>(d.group_id),
                    static_cast<int>(d.type_id),
                    static_cast<int>(d.server_id),
                    d.reported_name, d.reported_addr, d.reported_port,
                    d.version, d.cur_users, d.max_users, d.lease_epoch);
            }
        }

        auto controller = MakeServiceController();
        tcontrolsvr::PeerDialer dialer(io, peers, *inventory_ptr);

        // Per-IP login throttle. burst=0 disables.
        std::unique_ptr<fourstory::ops::LoginRateLimiter> login_rate;
        if (cfg.login_rate_burst > 0)
        {
            fourstory::ops::RateLimitConfig rl{};
            rl.burst           = cfg.login_rate_burst;
            rl.refill_interval = std::chrono::seconds(
                cfg.login_rate_refill_seconds);
            login_rate =
                std::make_unique<fourstory::ops::LoginRateLimiter>(rl);
            spdlog::info("login rate limit: burst={} refill={}s",
                cfg.login_rate_burst, cfg.login_rate_refill_seconds);
        }
        else
        {
            spdlog::info("login rate limit: disabled");
        }

        // --- Server ------------------------------------------------
        tcontrolsvr::ControlServerConfig svr_cfg{};
        svr_cfg.port       = cfg.port;
        svr_cfg.auth       = auth.get();
        svr_cfg.inventory  = inventory_ptr.get();
        svr_cfg.controller = controller.get();
        svr_cfg.dialer     = &dialer;
        svr_cfg.peers      = &peers;
        svr_cfg.audit      = &audit;
        svr_cfg.user_ban   = user_ban.get();
        svr_cfg.chat_bans  = &chat_bans;
        svr_cfg.events     = &events;
        svr_cfg.event_repo = event_repo.get();
        svr_cfg.patch_meta = patch_meta.get();
        svr_cfg.peer_repo  = peer_repo.get();
        svr_cfg.alerter    = alerter.get();

        // Server-to-server security gate. Wired before the listener
        // spawns so every accept() is filtered.
        auto security_gate =
            std::make_unique<fourstory::security::PeerSecurityGate>(
                cfg.security, pool.get());
        if (pool && cfg.security.db_trust_store)
            security_gate->LoadTrustStore();
        spdlog::info("security: ip_allowlist={} enforce={} peer_auth={} "
                     "trust_store={}",
            cfg.security.ip_allowlist.size(),
            cfg.security.ip_allowlist_enforce,
            cfg.security.peer_auth_required,
            security_gate->TrustCount());
        svr_cfg.security   = security_gate.get();

        svr_cfg.login_rate = login_rate.get();
        svr_cfg.db_pool    = db_pool.get();
        svr_cfg.auto_start = cfg.auto_start;
        tcontrolsvr::ControlServer server(io, svr_cfg);
        spdlog::info("control server listening on 0.0.0.0:{}", server.Port());
        boost::asio::co_spawn(io, server.Run(), boost::asio::detached);

        // 1Hz peer-keepalive watchdog (legacy TimerThread).
        boost::asio::co_spawn(io,
            server.PeerKeepaliveLoop(),
            boost::asio::detached);

        // Lease expiry sweep for modern peer self-registration.
        // Drops registry entries whose last heartbeat is older than
        // ~3 heartbeat intervals (90s default).
        boost::asio::co_spawn(io,
            server.RegistryLeaseExpiryLoop(),
            boost::asio::detached);

        // 1Hz event scheduler — daily / term events, alarms,
        // auto-delete for one-shot lottery / gifttime kinds.
        tcontrolsvr::EventSchedulerLoop event_loop(io, events, peers,
            event_repo.get());
        boost::asio::co_spawn(io, event_loop.Run(),
            boost::asio::detached);

        // Optional inventory refresher: re-reads TMACHINE / TGROUP /
        // TSVRTYPE / TSERVER / TIPADDR every N seconds so the
        // operator GUI sees topology edits without a daemon restart.
        // Only wired when SOCI is configured *and* the period is
        // > 0; the SOCI snapshot is the source of truth, so rebinding
        // PeerRegistry afterwards picks up new services + drops
        // removed ones.
        std::shared_ptr<fourstory::ops::RegistryRefresher> refresher;
        if (auto* soci_inv =
                dynamic_cast<tcontrolsvr::SociServiceInventory*>(
                    inventory_ptr.get());
            soci_inv != nullptr && cfg.inventory_refresh_seconds > 0)
        {
            refresher = fourstory::ops::RegistryRefresher::Make(io,
                std::chrono::seconds(cfg.inventory_refresh_seconds));
            // Offload the SOCI reload onto the worker pool so the
            // io_context isn't blocked during a slow DB roundtrip;
            // the Rebind step is pure in-memory and stays inline
            // after the coroutine resumes. Falls back to inline
            // execution when db_pool is null (legacy in-line
            // behaviour for dev runs without a worker pool).
            auto* db_pool_ptr = db_pool.get();
            refresher->AddCoroutineHook(
                [soci_inv, &peers, db_pool_ptr]()
                    -> boost::asio::awaitable<void>
                {
                    try
                    {
                        co_await fourstory::db::CoOffloadVoidIf(
                            db_pool_ptr, [soci_inv] { soci_inv->Reload(); });
                        peers.Rebind(*soci_inv);
                    }
                    catch (const std::exception& ex)
                    {
                        spdlog::warn("inventory refresh failed: {}",
                            ex.what());
                    }
                });
            refresher->Start();
            spdlog::info("inventory refresher: every {}s (offloaded={})",
                cfg.inventory_refresh_seconds,
                db_pool_ptr ? "yes" : "no");
        }

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

        // The cluster's single operator entry point. Cross-server
        // commands (`peers`, `kick`, `announce`, `service start/stop`,
        // `route`) route through PeerRegistry + MessageRouter +
        // IServiceController so the existing CT_* forwarder pipeline
        // handles fan-out — no new wire surface needed.
        tcontrolsvr::MessageRouter router(peers);
        std::shared_ptr<tcontrolsvr::AdminShell> admin;
        if (cfg.admin_port != 0)
        {
            try
            {
                admin = std::make_shared<tcontrolsvr::AdminShell>(
                    io, cfg.admin_bind, cfg.admin_port,
                    [&server] { return server.LiveOperators(); },
                    peers, *controller, &audit, &router,
                    std::chrono::steady_clock::now());
                spdlog::info("admin shell listening on {}:{}",
                    cfg.admin_bind, admin->Port());
                boost::asio::co_spawn(io, admin->Run(),
                    boost::asio::detached);
            }
            catch (const std::exception& ex)
            {
                spdlog::warn("admin shell failed to bind on {}:{}: {}",
                    cfg.admin_bind, cfg.admin_port, ex.what());
            }
        }

        io.run();

        // Graceful shutdown of the worker pool: wait for any
        // in-flight SOCI call to finish before returning so a SOCI
        // session lease isn't dropped mid-query.
        if (db_pool)
        {
            db_pool->stop();
            db_pool->join();
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::critical("fatal: {}", ex.what());
        return 1;
    }
    return 0;
}
