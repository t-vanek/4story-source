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
#include "db/schema_validator.h"
#include "services/char_registry.h"
#include "services/fake_guild_repository.h"
#include "services/guild_registry.h"
#include "services/peer_registry.h"
#include "services/soci_guild_repository.h"
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

        // --- DB infrastructure (W2 + W3a-1) -----------------------
        //
        // Pool sizing follows TLoginSvrAsio / TControlSvrAsio: N
        // SOCI sessions + a worker thread_pool that handlers offload
        // synchronous SOCI calls onto via fourstory::db::CoOffloadIf.
        // Both are nullable — no [database] section keeps the boot
        // path light for dev runs (the in-memory registries + the
        // fake guild repo cover every code path that doesn't actually
        // need persistence).
        std::unique_ptr<fourstory::db::SessionPool>      db_pool_owner;
        std::unique_ptr<boost::asio::thread_pool>        worker_pool;
        std::unique_ptr<tworldsvr::IGuildRepository>     guild_repo;

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

            // Fail-fast on missing TGUILD* columns before the
            // listener accepts traffic.
            tworldsvr::db::ValidateWorldSchema(*db_pool_owner);
            guild_repo = std::make_unique<tworldsvr::SociGuildRepository>(
                *db_pool_owner);
        }
        else
        {
            spdlog::info("no [database] — registries are the only "
                         "persistence layer; using FakeGuildRepository");
            guild_repo = std::make_unique<tworldsvr::FakeGuildRepository>();
        }

        // --- Char + guild registries ------------------------------
        //
        // Both use the same 16-shard partitioning + per-entry mutex
        // model. The char half closes W-2's in-memory bottleneck;
        // the guild half closes the m_mapTGuild bottleneck the same
        // way. Both live for the process lifetime.
        tworldsvr::CharRegistry  chars;
        tworldsvr::GuildRegistry guilds;
        tworldsvr::PeerRegistry  peers;

        // Warm the guild cache from the backing store. Empty for
        // the no-DB / fake repo path; a real DB returns every
        // non-disbanded TGUILDTABLE row.
        if (guild_repo)
        {
            const auto preloaded = guild_repo->LoadAll();
            for (auto& g : preloaded)
                guilds.Insert(g);
            spdlog::info("guild registry: {} guild(s) preloaded",
                guilds.Size());
        }

        tworldsvr::HandlerContext ctx{};
        ctx.io         = &io;
        ctx.db_pool    = worker_pool.get();
        ctx.chars      = &chars;
        ctx.guilds     = &guilds;
        ctx.peers      = &peers;
        ctx.guild_repo = guild_repo.get();
        ctx.nation     = cfg.nation;

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
