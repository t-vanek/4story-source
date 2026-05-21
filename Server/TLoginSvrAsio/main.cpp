// Entry point for the modernized TLoginSvrAsio binary. PCH-free,
// portable C++20 + Boost.Asio.
//
// Responsibilities:
//   1. Parse CLI (just --config / --help).
//   2. Load + validate the TOML config (tloginsvr::LoadConfig).
//   3. Construct services in dependency order:
//        a. LocalConnectionRegistry  (always — single-process
//                                     duplicate-kick map; matches
//                                     legacy CTLoginSvrModule::m_mapTUSER).
//        b. LoginRateLimiter         (always — per-IP token bucket).
//        c. LocalEventRegistry       (always — GM event store; matches
//                                     legacy m_mapEVENT).
//        d. ISmtpClient              (AsioSmtpClient when [smtp] host
//                                     set, else log-only fallback).
//        e. IAuditLogger             (SpdlogAuditLogger + optional UDP
//                                     shim to legacy TLogSvr collector).
//        f. SOCI services            (auth, char, map, terminator) +
//                                     SessionPool×2 when [database] is
//                                     configured. Fail-fast if pools
//                                     can't be opened. Validates
//                                     TGLOBAL + TGAME schema before
//                                     listener opens.
//   4. Run the boot-time ClearStaleSessions sweep (mirrors legacy
//      CSPClearLoginUser in CTLoginSvrModule::OnEnter — prevents
//      every account hitting LR_DUPLICATE after a crash).
//   5. Wire SIGINT/SIGTERM + SM_QUITSERVICE_REQ → graceful shutdown
//      (snapshot the registry, terminate each live session, io.stop).
//   6. co_spawn LoginServer::Run + the health endpoint; hand control
//      to io.run(). Admin shell intentionally NOT spawned here — the
//      cluster's single operator entry point is TControlSvrAsio.
//
// Legacy parity: Server/TLoginSvr/TLoginSvr.cpp (WinMain) +
// CTLoginSvrModule::OnEnter / OnLeave for the boot/shutdown chain.

#include "config.h"
#include "fourstory/ops/health_endpoint.h"
#include "login_server.h"
#include "db/schema_validator.h"
#include "fourstory/db/session_pool.h"
#include "services/local_connection_registry.h"
#include "services/local_event_registry.h"
#include "fourstory/ops/rate_limiter.h"
#include "fourstory/ops/registry_refresher.h"
#include "services/soci_auth_service.h"
#include "services/soci_char_service.h"
#include "services/soci_map_server_locator.h"
#include "services/soci_session_terminator.h"
#include "fourstory/audit/spdlog_audit_logger.h"
#include "fourstory/smtp/spdlog_smtp_client.h"
#include "fourstory/smtp/asio_smtp_client.h"
#include "fourstory/audit/udp_audit_logger.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/thread_pool.hpp>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <string>

namespace {

void Usage()
{
    std::printf(
        "tloginsvr_asio — modernized 4Story login server (work-in-progress)\n"
        "Usage: tloginsvr_asio [--config FILE]\n"
        "  --config FILE   path to TOML config (default: tloginsvr.toml in cwd)\n"
        "\n"
        "All other behavior — port, RC4 secret, log level, health-endpoint\n"
        "port — comes from the config file. See tloginsvr.example.toml for\n"
        "the schema. Missing keys fall back to defaults that match the\n"
        "shipped legacy server's hardcoded behavior.\n");
}

} // namespace

int main(int argc, char** argv)
{
    std::string config_path = "tloginsvr.toml";
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)
        {
            config_path = argv[++i];
        }
        else if (std::strcmp(argv[i], "--help") == 0 ||
                 std::strcmp(argv[i], "-h") == 0)
        {
            Usage();
            return 0;
        }
    }

    try
    {
        auto cfg = tloginsvr::LoadConfig(config_path);
        spdlog::set_level(cfg.log_level);

        // Refuse to start with the CT_* gate fully open unless the
        // operator explicitly opted in. Production deploys MUST pin
        // control_server_ip to TControlSvr's address — leaving it
        // empty lets any peer push CT_EVENTUPDATE_REQ et al. into the
        // event registry. Dev / in-process tests can set
        // [server] control_server_gate_open = true to silence this.
        if (cfg.server.control_server_ip.empty()
            && !cfg.server.control_server_gate_open)
        {
            throw std::runtime_error(
                "[server] control_server_ip is empty and "
                "control_server_gate_open is not set. CT_* dispatch "
                "would accept packets from any peer. Pin "
                "control_server_ip to TControlSvr's address, or "
                "explicitly opt in to the open gate with "
                "control_server_gate_open = true (dev/test only).");
        }
        if (cfg.server.control_server_ip.empty()
            && cfg.server.control_server_gate_open)
        {
            spdlog::warn("control_server_gate_open = true — CT_* dispatch "
                         "accepts packets from any peer (dev/test mode)");
        }

        boost::asio::io_context io;

        // Connection registry — single-process duplicate-kick map.
        // ALWAYS constructed (production-correct in-memory impl;
        // matches legacy CTLoginSvrModule::m_mapTUSER). Sharded
        // multi-instance deploys would swap for a Redis-backed
        // impl behind the same IConnectionRegistry interface.
        auto registry =
            std::make_unique<tloginsvr::services::LocalConnectionRegistry>();
        cfg.server.connection_registry = registry.get();

        // Per-IP login rate limiter. In-memory token bucket; no
        // distributed coordination needed for single-process deploys.
        auto rate_limiter =
            std::make_unique<fourstory::ops::LoginRateLimiter>();
        cfg.server.login_rate_limiter = rate_limiter.get();

        // GM event registry — populated by CT_EVENTUPDATE_REQ from the
        // control server. Single-process LocalEventRegistry; sharded
        // deploys would swap for a Redis-backed IEventRegistry impl.
        auto event_registry =
            std::make_unique<tloginsvr::services::LocalEventRegistry>();
        cfg.server.event_registry = event_registry.get();

        // SMTP client for 2FA emails. `[smtp] host` set → real
        // Asio-based SMTP transport; empty → log-only fallback (codes
        // land in the spdlog stream so dev / staging deploys still
        // exercise the 2FA path without a relay).
        std::unique_ptr<fourstory::smtp::ISmtpClient> smtp_client;
        if (!cfg.smtp.host.empty())
        {
            fourstory::smtp::AsioSmtpConfig sc;
            sc.host         = cfg.smtp.host;
            sc.port         = cfg.smtp.port;
            sc.from_address = cfg.smtp.from_address;
            sc.from_display = cfg.smtp.from_display;
            sc.username     = cfg.smtp.username;
            sc.password     = cfg.smtp.password;
            smtp_client = std::make_unique<fourstory::smtp::AsioSmtpClient>(std::move(sc));
            spdlog::info("smtp: real relay {}:{} (from={}{})",
                cfg.smtp.host, cfg.smtp.port, cfg.smtp.from_address,
                cfg.smtp.username.empty() ? "" : ", AUTH LOGIN");
        }
        else
        {
            smtp_client = std::make_unique<fourstory::smtp::SpdlogSmtpClient>();
            spdlog::info("smtp: log-only (no [smtp] host configured)");
        }
        cfg.server.smtp_client = smtp_client.get();

        // Audit logger — emits structured records via spdlog "audit"
        // logger. Always wired so events land somewhere; production
        // deploys can register a different "audit" logger before main
        // returns control to overlap with a structured sink.
        std::unique_ptr<fourstory::audit::IAuditLogger> audit =
            std::make_unique<fourstory::audit::SpdlogAuditLogger>();

        // Optional UDP shim — when [audit.udp] host is set, wrap the
        // spdlog logger so each event also lands on the legacy TLogSvr
        // collector in wire-compatible _UDPPACKET format.
        if (!cfg.audit_udp_host.empty() && cfg.audit_udp_port != 0)
        {
            audit = std::make_unique<fourstory::audit::UdpAuditLogger>(
                io, cfg.audit_udp_host, cfg.audit_udp_port, std::move(audit));
            spdlog::info("audit: UDP shim → {}:{}",
                cfg.audit_udp_host, cfg.audit_udp_port);
        }

        // SOCI services declared up front so the shutdown lambda below
        // can capture them by reference even though they're populated
        // later (only after [database] is validated).
        std::unique_ptr<fourstory::db::SessionPool>                  global_pool;
        std::unique_ptr<fourstory::db::SessionPool>                  world_pool;
        std::unique_ptr<boost::asio::thread_pool>                    db_pool;
        std::unique_ptr<tloginsvr::services::SociAuthService>        soci_auth;
        std::unique_ptr<tloginsvr::services::SociCharService>        soci_char;
        std::unique_ptr<tloginsvr::services::SociMapServerLocator>   soci_map;
        std::unique_ptr<tloginsvr::services::SociSessionTerminator>  soci_term;

        // Pre-shutdown sweep: legacy CTLoginSvrModule::UpdateData walks
        // m_mapTSESSION and calls CSPLogout for every session with
        // m_bLogout=TRUE. We do the same: snapshot the registry, drive
        // SessionTerminator::Terminate (Disconnect reason → DELETE
        // TCURRENTUSER + UPDATE TLOG.timeLOGOUT) for each live entry,
        // then signal io.stop. Without this every live session leaves a
        // stale TCURRENTUSER row that has to be reaped on the next
        // boot's ClearStaleSessions sweep.
        auto* registry_raw = registry.get();
        auto graceful_shutdown = [&io, registry_raw, &soci_term]() {
            if (registry_raw != nullptr && soci_term != nullptr)
            {
                const auto live = registry_raw->Snapshot();
                if (!live.empty())
                {
                    spdlog::info("shutdown: terminating {} live session(s)",
                        live.size());
                    for (const auto& it : live)
                    {
                        soci_term->Terminate(it.entry.user_id,
                            it.entry.session_key,
                            tloginsvr::services::TerminationReason::Disconnect,
                            it.entry.last_char_id);
                    }
                }
            }
            io.stop();
        };

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([graceful_shutdown](auto, int sig) {
            spdlog::info("received signal {}, shutting down", sig);
            graceful_shutdown();
        });

        // Wire SM_QUITSERVICE_REQ → graceful shutdown so wire-protocol
        // shutdown matches the SIGINT path.
        cfg.server.on_quit_request = graceful_shutdown;

        // Wire DB-backed services. Two pools mirror the legacy schema split:
        //   * global_pool → TGLOBAL (accounts, sessions, server registry)
        //   * world_pool  → TGAME   (per-world chars + items + guilds)
        // The pools throw soci::soci_error if the DB is unreachable — we
        // surface that as a fatal startup error (failing fast is correct:
        // running with a connection_string set but no DB would silently
        // downgrade to no-auth, which is worse).
        if (!cfg.database.connection_string.empty())
        {
            if (cfg.database.backend.empty())
                throw std::runtime_error(
                    "database.connection_string is set but database.backend "
                    "is empty — pick one of: postgresql | sqlite3 | odbc");
            const auto backend = fourstory::db::ParseBackend(cfg.database.backend);
            global_pool = std::make_unique<fourstory::db::SessionPool>(
                backend, cfg.database.connection_string, cfg.database.pool_size,
                std::chrono::seconds(cfg.database.acquire_timeout_secs));
            tloginsvr::db::ValidateGlobalSchema(*global_pool);

            // Worker pool for off-loop SOCI calls. Handlers call
            // through fourstory::db::CoOffloadIf — non-null pool
            // moves the SOCI work off the io_context thread, keeping
            // the reactor responsive when the DB is slow. Sized off
            // cfg.database.worker_threads (0 = legacy in-line).
            if (cfg.database.worker_threads > 0)
            {
                db_pool = std::make_unique<boost::asio::thread_pool>(
                    cfg.database.worker_threads);
                spdlog::info("db worker pool: {} thread(s)",
                    cfg.database.worker_threads);
            }
            else
            {
                spdlog::info("db worker pool: disabled "
                             "(SOCI calls run on io_context thread)");
            }

            if (!cfg.database_world.connection_string.empty())
            {
                if (cfg.database_world.backend.empty())
                    throw std::runtime_error(
                        "database.world.connection_string is set but "
                        "database.world.backend is empty");
                const auto wbackend =
                    fourstory::db::ParseBackend(cfg.database_world.backend);
                world_pool = std::make_unique<fourstory::db::SessionPool>(
                    wbackend,
                    cfg.database_world.connection_string,
                    cfg.database_world.pool_size,
                    std::chrono::seconds(cfg.database_world.acquire_timeout_secs));
                tloginsvr::db::ValidateWorldSchema(*world_pool);
            }

            soci_auth = std::make_unique<tloginsvr::services::SociAuthService>(
                *global_pool);
            soci_term = std::make_unique<tloginsvr::services::SociSessionTerminator>(
                *global_pool);
            soci_map  = std::make_unique<tloginsvr::services::SociMapServerLocator>(
                *global_pool, world_pool.get());

            // Crash-recovery: legacy CTLoginSvrModule::OnEnter calls
            // CSPClearLoginUser here so a previous process's stale
            // session rows don't lock every account into LR_DUPLICATE.
            // The new server runs the same one-shot DELETE.
            soci_term->ClearStaleSessions();

            cfg.server.auth_service        = soci_auth.get();
            cfg.server.map_server_locator  = soci_map.get();
            cfg.server.session_terminator  = soci_term.get();
            cfg.server.audit_logger        = audit.get();
            cfg.server.db_pool             = db_pool.get();

            // Char service needs both pools; only wire it when the world
            // DB is configured. Without TGAME we'd be unable to do real
            // create/list/delete — fall back to in-memory so the lobby
            // path still works for smoke tests.
            if (world_pool != nullptr)
            {
                soci_char = std::make_unique<tloginsvr::services::SociCharService>(
                    *global_pool, *world_pool);
                cfg.server.char_service = soci_char.get();

                // Periodic 30s refresh — currently just bumps the
                // veteran-chart cache so admins can hot-edit the
                // table without a server restart.
                auto refresher = fourstory::ops::RegistryRefresher::Make(
                    io, std::chrono::seconds(30));
                tloginsvr::services::SociCharService* svc = soci_char.get();
                refresher->AddHook([svc]() { svc->RefreshVeteranChart(); });
                refresher->Start();
                // Keep it alive for the duration of the process.
                static std::shared_ptr<fourstory::ops::RegistryRefresher> s_refresher;
                s_refresher = std::move(refresher);
                spdlog::info("services: SOCI ({}+{}) — auth + char + map + terminator",
                    fourstory::db::BackendName(backend),
                    fourstory::db::BackendName(
                        fourstory::db::ParseBackend(cfg.database_world.backend)));
            }
            else
            {
                spdlog::warn("services: SOCI ({}) — auth + map + terminator. "
                             "char_service stays in-memory: [database.world] "
                             "not configured",
                    fourstory::db::BackendName(backend));
            }
        }
        else
        {
            cfg.server.audit_logger = audit.get();
            spdlog::info("services: in-memory (no [database] configured)");
        }

        tloginsvr::LoginServer server(io, cfg.server);
        spdlog::info("login server listening on 0.0.0.0:{} (RC4: {})",
            server.Port(),
            cfg.server.rc4_secret_key.empty() ? "disabled" : "enabled");
        boost::asio::co_spawn(io, server.Run(), boost::asio::detached);

        // No per-server admin shell. Operator commands (status, kick,
        // ban, log-level) enter the cluster via TControlSvrAsio's
        // AdminShell and reach this server through the peer-forwarder
        // pipeline. Centralizing avoids the legacy footgun where each
        // daemon exposed its own localhost shell with diverging
        // command sets.

        // Optional health endpoint on a separate port.
        if (cfg.health_port != 0)
        {
            try
            {
                auto health = std::make_unique<fourstory::ops::HealthEndpoint>(io, cfg.health_port);
                spdlog::info("health endpoint listening on 0.0.0.0:{}", health->Port());
                boost::asio::co_spawn(io, health->Run(), boost::asio::detached);
                static std::unique_ptr<fourstory::ops::HealthEndpoint> s_health;
                s_health = std::move(health);
            }
            catch (const std::exception& ex)
            {
                spdlog::warn("health endpoint failed to bind on port {}: {}",
                    cfg.health_port, ex.what());
            }
        }

        io.run();

        // Drain the SOCI worker pool so an in-flight handler call
        // doesn't lose its session lease on shutdown.
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
