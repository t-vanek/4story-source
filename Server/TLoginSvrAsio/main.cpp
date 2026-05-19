// Entry point for the modernized TLoginSvrAsio binary. PCH-free,
// portable C++20 + Boost.Asio.

#include "fourstory/ops/admin_shell.h"
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

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](auto, int sig) {
            spdlog::info("received signal {}, shutting down", sig);
            io.stop();
        });

        // Wire SM_QUITSERVICE_REQ → io.stop() so wire-protocol
        // shutdown matches the SIGINT path.
        cfg.server.on_quit_request = [&io]() { io.stop(); };

        // Wire DB-backed services. Two pools mirror the legacy schema split:
        //   * global_pool → TGLOBAL (accounts, sessions, server registry)
        //   * world_pool  → TGAME   (per-world chars + items + guilds)
        // The pools throw soci::soci_error if the DB is unreachable — we
        // surface that as a fatal startup error (failing fast is correct:
        // running with a connection_string set but no DB would silently
        // downgrade to no-auth, which is worse).
        std::unique_ptr<fourstory::db::SessionPool>                  global_pool;
        std::unique_ptr<fourstory::db::SessionPool>                  world_pool;
        std::unique_ptr<tloginsvr::services::SociAuthService>        soci_auth;
        std::unique_ptr<tloginsvr::services::SociCharService>        soci_char;
        std::unique_ptr<tloginsvr::services::SociMapServerLocator>   soci_map;
        std::unique_ptr<tloginsvr::services::SociSessionTerminator>  soci_term;
        if (!cfg.database.connection_string.empty())
        {
            if (cfg.database.backend.empty())
                throw std::runtime_error(
                    "database.connection_string is set but database.backend "
                    "is empty — pick one of: postgresql | sqlite3 | odbc");
            const auto backend = fourstory::db::ParseBackend(cfg.database.backend);
            global_pool = std::make_unique<fourstory::db::SessionPool>(
                backend, cfg.database.connection_string, cfg.database.pool_size);
            tloginsvr::db::ValidateGlobalSchema(*global_pool);

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
                    cfg.database_world.pool_size);
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

        // Optional admin TCP shell — opt-in via [admin] port > 0.
        if (cfg.admin_port != 0)
        {
            try
            {
                auto* reg_raw = registry.get();
                auto admin = std::make_shared<fourstory::ops::AdminShell>(
                    io, cfg.admin_bind, cfg.admin_port,
                    [reg_raw]() -> std::size_t {
                        return reg_raw ? reg_raw->Count() : std::size_t{0};
                    },
                    std::chrono::steady_clock::now());
                spdlog::info("admin shell listening on {}:{}",
                    cfg.admin_bind, admin->Port());
                boost::asio::co_spawn(io, admin->Run(), boost::asio::detached);
                static std::shared_ptr<fourstory::ops::AdminShell> s_admin;
                s_admin = std::move(admin);
            }
            catch (const std::exception& ex)
            {
                spdlog::warn("admin shell failed to bind on {}:{} — {}",
                    cfg.admin_bind, cfg.admin_port, ex.what());
            }
        }

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
    }
    catch (const std::exception& ex)
    {
        spdlog::critical("fatal: {}", ex.what());
        return 1;
    }
    return 0;
}
