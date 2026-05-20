#include "config.h"
#include "db/schema_validator.h"
#include "log_server.h"
#include "services/log_sink.h"

#include "fourstory/db/session_pool.h"
#include "fourstory/ops/health_endpoint.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

namespace {
void Usage()
{
    std::printf(
        "tlogsvr_asio — modernized 4Story audit log collector\n"
        "Usage: tlogsvr_asio [--config FILE]\n");
}
} // namespace

int main(int argc, char** argv)
{
    std::string config_path = "tlogsvr.toml";
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
        auto cfg = tlogsvr::LoadConfig(config_path);
        spdlog::set_level(cfg.log_level);

        boost::asio::io_context io;
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](auto, int sig) {
            spdlog::info("received signal {}, shutting down", sig);
            io.stop();
        });

        std::unique_ptr<fourstory::db::SessionPool> pool;
        std::unique_ptr<tlogsvr::ILogSink>           sink;
        if (!cfg.database.connection_string.empty())
        {
            if (cfg.database.backend.empty())
                throw std::runtime_error("database.connection_string set but database.backend empty");
            const auto backend = fourstory::db::ParseBackend(cfg.database.backend);
            pool = std::make_unique<fourstory::db::SessionPool>(
                backend, cfg.database.connection_string, cfg.database.pool_size);
            // Fail-fast on missing LT_* columns before we bind the UDP
            // socket (F5 in SQL_AUDIT). Datagrams that arrive after a
            // misconfigured target_table would otherwise silently drop
            // at INSERT time with no visibility.
            tlogsvr::db::ValidateAuditSchema(*pool, cfg.target_table);
            sink = std::make_unique<tlogsvr::SociLogSink>(*pool, cfg.target_table);
            spdlog::info("log_sink: SOCI → {}.{}", cfg.database.backend, cfg.target_table);
        }
        else
        {
            sink = std::make_unique<tlogsvr::StdoutLogSink>();
            spdlog::warn("log_sink: stdout (no [database] configured)");
        }

        tlogsvr::LogServerConfig srv_cfg{
            .bind_address = cfg.bind_address,
            .port         = cfg.port,
            .sink         = sink.get(),
        };
        tlogsvr::LogServer server(io, srv_cfg);
        spdlog::info("log server listening UDP {}:{}",
            cfg.bind_address, server.Port());
        boost::asio::co_spawn(io, server.Run(), boost::asio::detached);

        // Optional /healthz endpoint on a separate port. Matches the
        // wiring in TLoginSvrAsio + TPatchSvrAsio — silently warn if
        // the port is in use rather than aborting the UDP listener.
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
        spdlog::info("totals: received={} drops_bad_format={}",
            server.PacketsReceived(), server.DropsBadFormat());
    }
    catch (const std::exception& ex)
    {
        spdlog::critical("fatal: {}", ex.what());
        return 1;
    }
    return 0;
}
