// TMapSvrAsio entry point (F1 scaffold).
//
// What this binary does TODAY:
//   * Loads TOML config (`tmapsvr.toml` by default).
//   * Binds the configured TCP port (default 5815, legacy TSERVER row
//     for bType=TMAP=4) and runs the accept loop.
//   * For each accepted client: wraps the socket in an AsioSession with
//     inbound RC4 enabled, runs the packet-decode loop, dispatches each
//     decoded packet through `tmapsvr::Dispatch`.
//   * The dispatcher implements exactly one handler — `CS_CONNECT_REQ`
//     (version + dwKEY token check → `CS_CONNECT_ACK`). Every other
//     legacy wire id is logged at debug level and dropped.
//   * `/healthz` HTTP endpoint exposes the live session count.
//
// What this binary DOESN'T do yet:
//   * Gameplay handlers (movement, combat, items, skills, NPCs, quests,
//     AI). All ~620 remaining CS_* / MW_* / DM_* / SM_* handlers are
//     wired in F2..Fn — see `README.md` for the phased plan.
//
// PCH-free; only depends on FourStoryCommon + TNetLib + Boost.Asio +
// spdlog + tomlplusplus, so it compiles unmodified on Linux too.

#include "config.h"
#include "map_server.h"
#include "db/schema_validator.h"
#include "services/fake_session_validator.h"
#include "services/session_validator.h"
#include "services/soci_session_validator.h"

#include "fourstory/db/session_pool.h"
#include "fourstory/ops/admin_shell.h"
#include "fourstory/ops/health_endpoint.h"
#include "fourstory/ops/rate_limiter.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstdio>
#include <exception>
#include <memory>
#include <string>

int main(int argc, char** argv)
{
    std::string cfg_path = "tmapsvr.toml";
    for (int i = 1; i + 1 < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--config" || a == "-c")
            cfg_path = argv[++i];
    }

    try
    {
        const auto cfg = tmapsvr::LoadConfig(cfg_path);
        spdlog::set_level(cfg.log_level);

        boost::asio::io_context io;

        // F2: when [database] is configured the production validator
        // queries TCURRENTUSER for the (uid, char, dwKEY) tuple
        // TLoginSvr wrote during CS_START_REQ. Without [database] the
        // fake accept-all dev validator runs so a smoke test against a
        // bare binary still works (no Login peer required).
        std::unique_ptr<fourstory::db::SessionPool>     db_pool;
        std::unique_ptr<tmapsvr::IMapSessionValidator>  validator;
        if (!cfg.database.connection_string.empty())
        {
            if (cfg.database.backend.empty())
                throw std::runtime_error(
                    "database.connection_string set but database.backend empty");
            const auto backend = fourstory::db::ParseBackend(cfg.database.backend);
            db_pool = std::make_unique<fourstory::db::SessionPool>(
                backend, cfg.database.connection_string,
                cfg.database.pool_size);
            // Fail-fast on missing TCURRENTUSER columns before the
            // listener opens — surfaces schema drift at boot rather
            // than on the first client connection.
            tmapsvr::db::ValidateGlobalSchema(*db_pool);
            validator = std::make_unique<tmapsvr::SociMapSessionValidator>(
                *db_pool);
            spdlog::info("session_validator: SOCI ({}) — TCURRENTUSER",
                fourstory::db::BackendName(backend));
        }
        else
        {
            auto fake = std::make_unique<tmapsvr::FakeMapSessionValidator>();
            fake->SetAcceptAll(true);
            spdlog::warn("no [database] configured — using accept-all fake "
                         "validator (dev / smoke mode only)");
            validator = std::move(fake);
        }

        std::unique_ptr<fourstory::ops::LoginRateLimiter> connect_rate;
        if (cfg.connect_rate_burst > 0)
        {
            fourstory::ops::RateLimitConfig rl{};
            rl.burst           = cfg.connect_rate_burst;
            rl.refill_interval = std::chrono::seconds(
                cfg.connect_rate_refill_seconds);
            connect_rate =
                std::make_unique<fourstory::ops::LoginRateLimiter>(rl);
            spdlog::info("connect rate limit: burst={} refill={}s",
                cfg.connect_rate_burst, cfg.connect_rate_refill_seconds);
        }

        tmapsvr::MapServerConfig svr_cfg{};
        svr_cfg.port              = cfg.port;
        svr_cfg.rc4_secret_key    = cfg.rc4_secret_key;
        svr_cfg.accepted_versions = cfg.accepted_versions;
        svr_cfg.validator         = validator.get();
        svr_cfg.connect_rate      = connect_rate.get();
        svr_cfg.pre_auth_timeout  = std::chrono::seconds(
            cfg.pre_auth_timeout_seconds);
        svr_cfg.max_connections   = cfg.max_connections;
        svr_cfg.on_quit_request   = [&io]() {
            spdlog::info("SM_QUITSERVICE_REQ → io.stop()");
            io.stop();
        };

        tmapsvr::MapServer server(io, svr_cfg);
        spdlog::info("tmapsvr_asio listening on :{}", server.Port());

        // /healthz on a separate port for ops liveness checks. The
        // shared endpoint returns a static OK; load balancers that
        // want to drain a hot shard can read the live session count
        // via the admin shell instead.
        std::unique_ptr<fourstory::ops::HealthEndpoint> health;
        if (cfg.health_port != 0)
        {
            try
            {
                health = std::make_unique<fourstory::ops::HealthEndpoint>(
                    io, cfg.health_port);
                boost::asio::co_spawn(io, health->Run(),
                    boost::asio::detached);
                spdlog::info("/healthz on :{}", cfg.health_port);
            }
            catch (const std::exception& ex)
            {
                spdlog::warn("health endpoint disabled: {}", ex.what());
            }
        }

        std::shared_ptr<fourstory::ops::AdminShell> admin;
        const auto started_at = std::chrono::steady_clock::now();
        if (cfg.admin_port != 0)
        {
            try
            {
                admin = std::make_shared<fourstory::ops::AdminShell>(
                    io, cfg.admin_bind, cfg.admin_port,
                    [&server]() -> std::size_t {
                        return server.LiveSessions();
                    },
                    started_at);
                boost::asio::co_spawn(io, admin->Run(),
                    boost::asio::detached);
                spdlog::info("admin shell on {}:{}",
                    cfg.admin_bind, cfg.admin_port);
            }
            catch (const std::exception& ex)
            {
                spdlog::warn("admin shell disabled: {}", ex.what());
            }
        }

        boost::asio::co_spawn(io, server.Run(), boost::asio::detached);

        // SIGINT / SIGTERM → graceful stop. The acceptor closes from
        // inside the listener's accept coroutine; per-session
        // coroutines tear down on the next read error.
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](const boost::system::error_code&, int sig) {
            spdlog::info("received signal {} — shutting down", sig);
            io.stop();
        });

        io.run();
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "fatal: %s\n", ex.what());
        return 1;
    }
    return 0;
}
