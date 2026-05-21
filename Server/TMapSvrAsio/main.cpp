// Entry point for the modernized TMapSvrAsio binary.
//
// Phase F1: minimal Asio scaffold. Starts an io_context, installs
// SIGINT/SIGTERM handlers for graceful shutdown, and exits cleanly.
// No listener, no config loading, no DB — those land in F2 (config +
// schema validator) and F3 (MapServer accept loop) respectively.
//
// The shape matches TPatchSvrAsio / TLoginSvrAsio so subsequent phases
// can grow it in place.

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <spdlog/spdlog.h>

#include <cstdio>
#include <cstring>
#include <exception>

namespace {

void Usage()
{
    std::printf(
        "tmapsvr_asio — modernized 4Story map server (phase F1 scaffold)\n"
        "Usage: tmapsvr_asio [--help]\n");
}

} // namespace

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--help") == 0 ||
            std::strcmp(argv[i], "-h")     == 0)
        {
            Usage();
            return 0;
        }
    }

    try
    {
        boost::asio::io_context io;

        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](auto, int sig) {
            spdlog::info("received signal {}, shutting down", sig);
            io.stop();
        });

        spdlog::info("tmapsvr_asio: F1 scaffold up (no listener yet) — "
                     "send SIGINT/SIGTERM to exit");

        io.run();
    }
    catch (const std::exception& ex)
    {
        spdlog::critical("fatal: {}", ex.what());
        return 1;
    }
    return 0;
}
