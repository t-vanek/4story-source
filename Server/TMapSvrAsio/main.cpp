// Entry point for the modernized TMapSvrAsio binary.
//
// Phase F7: movement + flat-list AOI broadcast. The handler dispatch
// now covers CS_CONREADY_REQ (stub — full enter-map flood lands with
// F8) and CS_MOVE_REQ (decodes 26-byte position update, persists to
// the per-channel presence map, broadcasts a 27-byte CS_MOVE_ACK to
// every other session on the same channel). The MapServer teardown
// hook now unbinds both the session registry and presence map so
// dropped sockets don't leak entries or attract broadcasts.

#include "config.h"
#include "handlers_world.h"
#include "map_server.h"
#include "db/schema_validator.h"
#include "services/channel_presence.h"
#include "services/session_registry.h"
#include "services/session_validator.h"
#include "services/soci_session_validator.h"
#include "services/world_client.h"

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
        "tmapsvr_asio — modernized 4Story map server (phase F7 scaffold)\n"
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

        // char_id → AsioSession map. Lives for the io.run() duration,
        // bound at CS_CONNECT_REQ success, unbound by the MapServer
        // per-connection teardown. Consulted by handlers_world to
        // route DM_/MW_ inbound traffic back to the right client.
        tmapsvr::InMemorySessionRegistry session_reg;

        // Per-channel presence (char_id → channel + position + session).
        // Driven by the same handshake / teardown pair as session_reg.
        // CS_MOVE_REQ updates position and broadcasts to other
        // channel members.
        tmapsvr::InMemoryChannelPresence presence;

        // Build the HandlerContext now so the world inbound dispatch
        // lambda can capture it by reference (the context's pointer
        // fields are filled in below as each service comes online).
        tmapsvr::HandlerContext ctx{};
        ctx.validator    = validator.get();
        ctx.session_reg  = &session_reg;
        ctx.presence     = &presence;

        // Optional World peer — only spun up when [world] port is set
        // in the TOML. Without it, MW_ADDCHAR_ACK after a clean
        // handshake gets logged as deferred and never sent (no buffer
        // in F5). Dev runs that only need to exercise the local
        // dispatch can leave [world].port = 0.
        std::unique_ptr<tmapsvr::AsioWorldClient> world_client;
        if (cfg.world.port != 0)
        {
            // Inbound dispatch — synchronous callback fired from the
            // world's read loop; copy the body (the span is only
            // valid during the callback) and co_spawn the awaitable
            // DispatchWorld detached so SendPacket calls don't block
            // the read.
            auto on_world_packet =
                [&io, &ctx](std::uint16_t wId,
                            std::span<const std::byte> body)
                {
                    std::vector<std::byte> owned(body.begin(), body.end());
                    boost::asio::co_spawn(
                        io,
                        tmapsvr::DispatchWorld(wId, std::move(owned), ctx),
                        [wId](std::exception_ptr ep) {
                            if (!ep) return;
                            try { std::rethrow_exception(ep); }
                            catch (const std::exception& ex) {
                                spdlog::error("world dispatch threw (wId=0x{:04X}): {}",
                                    wId, ex.what());
                            }
                            catch (...) {
                                spdlog::error("world dispatch threw unknown (wId=0x{:04X})",
                                    wId);
                            }
                        });
                };
            world_client = std::make_unique<tmapsvr::AsioWorldClient>(
                io, cfg.world.host, cfg.world.port,
                std::move(on_world_packet));
            boost::asio::co_spawn(io, world_client->Run(), boost::asio::detached);
            spdlog::info("world_client: dialing {}:{} (background)",
                cfg.world.host, cfg.world.port);
        }
        else
        {
            spdlog::warn("no [world] port configured — MW_ADDCHAR_ACK and "
                         "future DM_/MW_ traffic will be skipped");
        }

        // Now that the world client (if any) is built we can fill the
        // last pointer; assigning before construction would have
        // captured a nullptr by reference into the dispatch lambda.
        ctx.world_client = world_client.get();
        cfg.server.handlers = ctx;

        const bool crypto_on = !cfg.server.rc4_secret_key.empty();
        const auto mode_name = tmapsvr::ModeName(cfg.mode);
        tmapsvr::MapServer server(io, std::move(cfg.server));
        spdlog::info("tmapsvr_asio: F7 listener on 0.0.0.0:{} (mode={}, crypto={}) — "
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
