// Entry point for the modernized TMapSvrAsio binary.
//
// Phase F4: first real handler (CS_CONNECT_REQ → CS_CONNECT_ACK).
// Boot sequence: load config → set log level → install signals →
// (optional) open SOCI pool + run schema validator + build the
// SOCI session validator → instantiate MapServer with the validator
// in its HandlerContext → co_spawn accept loop → io.run().
//
// Without a [database] section main() still comes up, but the
// dispatch path refuses CS_CONNECT_REQ with INTERNAL — F4 needs
// the validator to clear the handshake.

#include "config.h"
#include "map_server.h"
#include "db/schema_validator.h"
#include "services/session_validator.h"
#include "services/soci_session_validator.h"

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
        "tmapsvr_asio — modernized 4Story map server (phase F4 scaffold)\n"
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

        // Optional SOCI pool — without one, no session validator is
        // wired in and CS_CONNECT_REQ is refused. The listener still
        // comes up so dev runs can netcat-poke the wire codec.
        std::unique_ptr<fourstory::db::SessionPool>     pool;
        std::unique_ptr<tmapsvr::IMapSessionValidator>  validator;
        if (!cfg.database.connection_string.empty())
        {
            if (cfg.database.backend.empty())
                throw std::runtime_error("database.connection_string set but database.backend empty");
            const auto backend = fourstory::db::ParseBackend(cfg.database.backend);
            pool = std::make_unique<fourstory::db::SessionPool>(
                backend, cfg.database.connection_string, cfg.database.pool_size);
            tmapsvr::db::ValidateUserSchema(*pool);
            validator = std::make_unique<tmapsvr::SociMapSessionValidator>(*pool);
            spdlog::info("schema: TCURRENTUSER columns OK ({}) — session "
                         "validator ready",
                fourstory::db::BackendName(backend));
        }
        else
        {
            spdlog::warn("no [database] configured — CS_CONNECT_REQ will "
                         "refuse with INTERNAL");
        }

        // Wire the validator pointer into the MapServer's handler
        // context. Pointer is non-owning; the unique_ptr above keeps
        // the storage alive for the io.run() lifetime.
        cfg.server.handlers.validator = validator.get();

        const bool crypto_on = !cfg.server.rc4_secret_key.empty();
        const auto mode_name = tmapsvr::ModeName(cfg.mode);
        tmapsvr::MapServer server(io, std::move(cfg.server));
        spdlog::info("tmapsvr_asio: F4 listener on 0.0.0.0:{} (mode={}, crypto={}) — "
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
