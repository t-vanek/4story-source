// Entry point for the modernized TPatchSvrAsio binary.

#include "config.h"
#include "patch_server.h"
#include "services/patch_repository.h"
#include "db/schema_validator.h"

// Reuse Login server's pool wrapper.
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
#include <memory>
#include <string>

namespace {
void Usage()
{
    std::printf(
        "tpatchsvr_asio — modernized 4Story patch metadata server\n"
        "Usage: tpatchsvr_asio [--config FILE]\n");
}
} // namespace

int main(int argc, char** argv)
{
    std::string config_path = "tpatchsvr.toml";
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
        auto cfg = tpatchsvr::LoadConfig(config_path);
        spdlog::set_level(cfg.log_level);

        boost::asio::io_context io;

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](auto, int sig) {
            spdlog::info("received signal {}, shutting down", sig);
            io.stop();
        });

        std::unique_ptr<fourstory::db::SessionPool> pool;
        std::unique_ptr<boost::asio::thread_pool>   db_pool;
        std::unique_ptr<tpatchsvr::PatchRepository> repo;
        if (!cfg.database.connection_string.empty())
        {
            if (cfg.database.backend.empty())
                throw std::runtime_error("database.connection_string set but database.backend empty");
            const auto backend = fourstory::db::ParseBackend(cfg.database.backend);
            pool = std::make_unique<fourstory::db::SessionPool>(
                backend, cfg.database.connection_string, cfg.database.pool_size);
            // Fail-fast on missing patch-metadata columns before we
            // accept traffic (F5 in SQL_AUDIT). Optional UI table
            // logs a warning rather than aborting — see validator
            // notes.
            tpatchsvr::db::ValidateGlobalSchema(*pool);
            repo = std::make_unique<tpatchsvr::PatchRepository>(*pool);

            // Worker pool for off-loop SOCI. The hot site is
            // MarkPreVersionComplete (MERGE+DELETE txn).
            if (cfg.database.worker_threads > 0)
            {
                db_pool = std::make_unique<boost::asio::thread_pool>(
                    cfg.database.worker_threads);
                spdlog::info("db worker pool: {} thread(s)",
                    cfg.database.worker_threads);
            }
            spdlog::info("patch_repo: SOCI ({}) ready",
                fourstory::db::BackendName(backend));
        }
        else
        {
            spdlog::warn("no [database] configured — all queries return empty");
        }

        tpatchsvr::PatchServerConfig srv_cfg{};
        srv_cfg.port        = cfg.port;
        srv_cfg.repo        = repo.get();
        srv_cfg.ftp_url     = cfg.ftp_url;
        srv_cfg.pre_ftp_url = cfg.pre_ftp_url;
        srv_cfg.login_host  = cfg.login_host;
        srv_cfg.login_port  = cfg.login_port;
        srv_cfg.db_pool     = db_pool.get();

        tpatchsvr::PatchServer server(io, srv_cfg);
        spdlog::info("patch server listening on 0.0.0.0:{}", server.Port());
        boost::asio::co_spawn(io, server.Run(), boost::asio::detached);

        // Optional /healthz endpoint on a separate port. Matches the
        // wiring in TLoginSvrAsio — silently warn if the port is in
        // use rather than aborting the main listener.
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

        io.run();

        // Drain in-flight worker tasks before exit so a posted
        // MarkPreVersionComplete doesn't lose its session lease.
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
