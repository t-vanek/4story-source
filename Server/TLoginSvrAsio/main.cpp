// Entry point for the modernized TLoginSvrAsio binary. PCH-free,
// portable C++20 + Boost.Asio.

#include "config.h"
#include "health_endpoint.h"
#include "login_server.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
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
        const auto cfg = tloginsvr::LoadConfig(config_path);
        spdlog::set_level(cfg.log_level);

        boost::asio::io_context io;

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](auto, int sig) {
            spdlog::info("received signal {}, shutting down", sig);
            io.stop();
        });

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
