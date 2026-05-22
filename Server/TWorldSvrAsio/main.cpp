// Entry point for the modernized TWorldSvrAsio binary.
//
// W2 (this revision) adds:
//   - SessionPool + DB worker thread_pool when [database] is set
//     (used by W3+ guild / friend / soulmate SOCI handlers)
//   - CharRegistry — the partitioned in-memory char index that
//     replaces the legacy `m_mapTCHAR` single-lock map. Closes
//     the in-memory half of PATCH_README §6 W-2.
//   - First batch of char-lifecycle handlers
//     (MW_ADDCHAR_ACK, MW_CLOSECHAR_ACK)
//
// W3+ layers on guild / party / corps services + the peer dialer
// that lets handlers send replies back to the right map server.
// See Server/TWorldSvrAsio/README.md for the phase plan.

#include "config.h"
#include "services/char_registry.h"
#include "world_server.h"

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
        "tworldsvr_asio — modernized 4Story cluster coordinator (W2)\n"
        "Usage: tworldsvr_asio [--config FILE]\n");
}
} // namespace

int main(int argc, char** argv)
{
    std::string config_path = "tworldsvr.toml";
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
        auto cfg = tworldsvr::LoadConfig(config_path);
        spdlog::set_level(cfg.log_level);

        boost::asio::io_context io;

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](auto, int sig) {
            spdlog::info("received signal {}, shutting down", sig);
            io.stop();
        });

        // --- DB infrastructure (W2) -------------------------------
        //
        // Pool sizing follows TLoginSvrAsio / TControlSvrAsio: N
        // SOCI sessions + a worker thread_pool that handlers offload
        // synchronous SOCI calls onto via fourstory::db::CoOffloadIf.
        // Both are nullable — no [database] section keeps the boot
        // path light for dev runs (just the in-memory char registry,
        // no DB roundtrips).
        std::unique_ptr<fourstory::db::SessionPool>      db_pool_owner;
        std::unique_ptr<boost::asio::thread_pool>        worker_pool;

        if (!cfg.database.connection_string.empty())
        {
            if (cfg.database.backend.empty())
                throw std::runtime_error(
                    "database.connection_string set but database.backend empty");
            const auto backend =
                fourstory::db::ParseBackend(cfg.database.backend);
            db_pool_owner = std::make_unique<fourstory::db::SessionPool>(
                backend, cfg.database.connection_string,
                cfg.database.pool_size);

            if (cfg.database.worker_threads > 0)
            {
                worker_pool = std::make_unique<boost::asio::thread_pool>(
                    cfg.database.worker_threads);
                spdlog::info("db worker pool: {} threads",
                    cfg.database.worker_threads);
            }
            else
            {
                spdlog::info("db worker pool: disabled "
                             "(SOCI runs in-line on io_context — dev only)");
            }
            spdlog::info("db: {} pool_size={} ready",
                fourstory::db::BackendName(backend), cfg.database.pool_size);
        }
        else
        {
            spdlog::info("no [database] — char registry is the only "
                         "persistence layer (legacy parity for W2)");
        }

        // --- Char registry (W2 — closes the in-memory half of W-2) -
        //
        // Per-shard locking + per-char actor model. Stays alive for
        // the entire process lifetime; sessions hold raw pointers
        // through HandlerContext.
        tworldsvr::CharRegistry chars;

        tworldsvr::HandlerContext ctx{};
        ctx.io      = &io;
        ctx.db_pool = worker_pool.get();
        ctx.chars   = &chars;

        tworldsvr::WorldServerConfig svr_cfg{};
        svr_cfg.port            = cfg.port;
        svr_cfg.max_connections = cfg.max_connections;
        svr_cfg.ctx             = ctx;

        tworldsvr::WorldServer server(io, svr_cfg);
        spdlog::info("world server listening on 0.0.0.0:{} (max_connections={})",
            server.Port(), cfg.max_connections);
        boost::asio::co_spawn(io, server.Run(), boost::asio::detached);

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

        // Drain in-flight SOCI work before tearing the pool down so
        // a query doesn't keep a session reference alive past the
        // pool's destructor.
        if (worker_pool)
        {
            worker_pool->stop();
            worker_pool->join();
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::critical("fatal: {}", ex.what());
        return 1;
    }
    return 0;
}
