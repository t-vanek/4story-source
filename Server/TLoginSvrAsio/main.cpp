// Entry point for the modernized TLoginSvrAsio binary. PCH-free,
// portable C++20 + Boost.Asio.

#include "config.h"
#include "health_endpoint.h"
#include "login_server.h"
#include "db/session_pool.h"
#include "services/soci_auth_service.h"
#include "services/soci_char_service.h"
#include "services/soci_map_server_locator.h"
#include "services/soci_session_terminator.h"

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

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](auto, int sig) {
            spdlog::info("received signal {}, shutting down", sig);
            io.stop();
        });

        // Wire DB-backed services when a connection string is configured.
        // Lifetime: both the pool and the auth service live for the
        // duration of the io_context — kept on the stack here so they
        // outlive LoginServer's borrowed pointers. The pool itself
        // throws soci::soci_error if the DB is unreachable, which we
        // surface as a fatal startup error (failing fast is correct:
        // running with `database.connection_string` set but no DB
        // would silently downgrade to no-auth, which is worse).
        std::unique_ptr<tloginsvr::db::SessionPool>                db_pool;
        std::unique_ptr<tloginsvr::services::SociAuthService>      soci_auth;
        std::unique_ptr<tloginsvr::services::SociCharService>      soci_char;
        std::unique_ptr<tloginsvr::services::SociMapServerLocator> soci_map;
        std::unique_ptr<tloginsvr::services::SociSessionTerminator> soci_term;
        if (!cfg.database.connection_string.empty())
        {
            if (cfg.database.backend.empty())
                throw std::runtime_error(
                    "database.connection_string is set but database.backend "
                    "is empty — pick one of: postgresql | sqlite3 | odbc");
            const auto backend = tloginsvr::db::ParseBackend(cfg.database.backend);
            db_pool = std::make_unique<tloginsvr::db::SessionPool>(
                backend, cfg.database.connection_string, cfg.database.pool_size);
            soci_auth = std::make_unique<tloginsvr::services::SociAuthService>(*db_pool);
            soci_char = std::make_unique<tloginsvr::services::SociCharService>(*db_pool);
            soci_map  = std::make_unique<tloginsvr::services::SociMapServerLocator>(*db_pool);
            soci_term = std::make_unique<tloginsvr::services::SociSessionTerminator>(*db_pool);
            cfg.server.auth_service        = soci_auth.get();
            cfg.server.char_service        = soci_char.get();
            cfg.server.map_server_locator  = soci_map.get();
            cfg.server.session_terminator  = soci_term.get();
            spdlog::info("services: SOCI ({}) — auth + char + map_locator + session_terminator",
                tloginsvr::db::BackendName(backend));
        }
        else
        {
            spdlog::info("services: in-memory (no [database] configured)");
        }

        tloginsvr::LoginServer server(io, cfg.server);
        spdlog::info("login server listening on 0.0.0.0:{} (RC4: {})",
            server.Port(),
            cfg.server.rc4_secret_key.empty() ? "disabled" : "enabled");
        boost::asio::co_spawn(io, server.Run(), boost::asio::detached);

        // Optional health endpoint on a separate port.
        if (cfg.health_port != 0)
        {
            try
            {
                auto health = std::make_unique<tloginsvr::HealthEndpoint>(io, cfg.health_port);
                spdlog::info("health endpoint listening on 0.0.0.0:{}", health->Port());
                boost::asio::co_spawn(io, health->Run(), boost::asio::detached);
                // Hand ownership to the io_context for the duration; the
                // simplest approach is to leak through a static — daemon
                // lifetime is process lifetime.
                static std::unique_ptr<tloginsvr::HealthEndpoint> s_health;
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
