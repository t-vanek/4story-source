// Entry point for the modernized TMapSvrAsio binary.
//
// Phase F2: config + schema validator. Loads a TOML config file
// (default: tmapsvr.toml in the working directory), spins up the
// io_context, opens an optional SOCI pool against TUSER, runs the
// boot-time schema check, and then idles on signals. F3 will wire in
// the MapServer accept loop here.

#include "config.h"
#include "db/schema_validator.h"

#include "fourstory/db/session_pool.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <string>

namespace {

void Usage()
{
    std::printf(
        "tmapsvr_asio — modernized 4Story map server (phase F2 scaffold)\n"
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
        // database (dev runs, smoke tests) we skip it and just idle.
        // The F3 listener will still come up; F4 handshake refuses
        // connections until a real pool + validator pass land.
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

        spdlog::info("tmapsvr_asio: F2 scaffold up (mode={}, port={}) — "
                     "no listener yet; send SIGINT/SIGTERM to exit",
                     tmapsvr::ModeName(cfg.mode), cfg.port);

        io.run();
    }
    catch (const std::exception& ex)
    {
        spdlog::critical("fatal: {}", ex.what());
        return 1;
    }
    return 0;
}
