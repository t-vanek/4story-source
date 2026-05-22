// Entry point for the modernized TWorldSvrAsio binary.
//
// W1 ships the scaffold:
//   - TOML config (server port, max_connections, health port, log level)
//   - io_context + signal_set graceful-shutdown
//   - WorldServer accept loop on plain TCP (no RC4 on SS link)
//   - Empty HandlerContext — dispatch logs + drops every packet
//   - HealthEndpoint on the configured port
//
// W2 layers on the SOCI [database] config, the char persistence
// repository, and the first batch of CHAR / USER / OnRW handlers.
// See Server/TWorldSvrAsio/README.md for the full phasing.

#include "config.h"
#include "world_server.h"

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
        "tworldsvr_asio — modernized 4Story cluster coordinator (W1 scaffold)\n"
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

        // W1 ships an empty HandlerContext — the io_context is the
        // only wire that handlers can reach back through (none do
        // yet). W2 fills char_repo / chars; W3 fills guild / party /
        // corps; … see README.md.
        tworldsvr::HandlerContext ctx{};
        ctx.io = &io;

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
    }
    catch (const std::exception& ex)
    {
        spdlog::critical("fatal: {}", ex.what());
        return 1;
    }
    return 0;
}
