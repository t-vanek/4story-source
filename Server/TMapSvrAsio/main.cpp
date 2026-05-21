// Entry point for the modernized TMapSvrAsio binary.
//
// Phase F3: MapServer accept loop. Loads config, opens an optional
// SOCI pool against TUSER, boots the schema check, spins up the
// MapServer (TCP listener with AsioSession-per-connection + RC4
// inbound when configured), and idles on signals. Per-packet handler
// dispatch is still a stub — F4 wires in the real CS_*/MW_*/DM_*
// handlers.

#include "config.h"
#include "map_server.h"
#include "db/schema_validator.h"

#include "fourstory/db/session_pool.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace {

void Usage()
{
    std::printf(
        "tmapsvr_asio — modernized 4Story map server (phase F3 scaffold)\n"
        "Usage: tmapsvr_asio [--config FILE] [--help]\n"
        "  --config FILE   TOML config (default: tmapsvr.toml)\n");
}

} // namespace

int main(int argc, char** argv)
{
    std::string config_path = "tmapsvr.toml";
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc)
            config_path = argv[++i];
        else if (std::strcmp(argv[i], "--help") == 0 ||
                 std::strcmp(argv[i], "-h")     == 0)
        {
            Usage();
            return 0;
        }
    }

    try
    {
        auto cfg = tmapsvr::LoadConfig(config_path);
        spdlog::set_level(cfg.log_level);

        boost::asio::io_context io;

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](auto, int sig) {
            spdlog::info("received signal {}, shutting down", sig);
            io.stop();
        });

        // Optional SOCI pool — when the operator hasn't configured a
        // database (dev runs, smoke tests) we skip it. The listener
        // still comes up; F4 handshake will refuse traffic for lack
        // of a session lookup path.
        std::unique_ptr<fourstory::db::SessionPool> pool;
        if (!cfg.database.connection_string.empty())
        {
            if (cfg.database.backend.empty())
                throw std::runtime_error("database.connection_string set but database.backend empty");
            const auto backend = fourstory::db::ParseBackend(cfg.database.backend);
            pool = std::make_unique<fourstory::db::SessionPool>(
                backend, cfg.database.connection_string, cfg.database.pool_size);
            tmapsvr::db::ValidateUserSchema(*pool);
            spdlog::info("schema: TCURRENTUSER columns OK ({})",
                fourstory::db::BackendName(backend));
        }
        else
        {
            spdlog::warn("no [database] configured — handshake/load paths "
                         "will fail when F4 lands");
        }

        // MapServer takes a moved-from copy of cfg.server (port, RC4
        // key, max_connections). Capture the log fields before the
        // move so we don't read moved-from state. The MapServer
        // instance lives in this stack frame for the duration of
        // io.run().
        const bool crypto_on = !cfg.server.rc4_secret_key.empty();
        const auto mode_name = tmapsvr::ModeName(cfg.mode);
        tmapsvr::MapServer server(io, std::move(cfg.server));
        spdlog::info("tmapsvr_asio: F3 listener on 0.0.0.0:{} (mode={}, crypto={}) — "
                     "send SIGINT/SIGTERM to exit",
                     server.Port(), mode_name,
                     crypto_on ? "on" : "off");
        boost::asio::co_spawn(io, server.Run(), boost::asio::detached);

        io.run();
    }
    catch (const std::exception& ex)
    {
        spdlog::critical("fatal: {}", ex.what());
        return 1;
    }
    return 0;
}
