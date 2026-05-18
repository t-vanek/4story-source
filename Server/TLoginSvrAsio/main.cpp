// Entry point for the modernized TLoginSvrAsio binary. PCH-free,
// portable C++20 + Boost.Asio.

#include "login_server.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr std::uint16_t kDefaultPort = 4815;

void Usage()
{
    std::printf(
        "tloginsvr_asio — modernized 4Story login server (work-in-progress)\n"
        "Usage: tloginsvr_asio [--port N]\n"
        "  --port N   listen on TCP port N (default: %u)\n",
        kDefaultPort);
}

} // namespace

int main(int argc, char** argv)
{
    std::uint16_t port = kDefaultPort;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc)
        {
            port = static_cast<std::uint16_t>(std::atoi(argv[++i]));
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
        boost::asio::io_context io;

        // Graceful shutdown on SIGINT / SIGTERM. Don't try to drain
        // in-flight sessions in this scaffold; just stop the executor
        // and let process teardown close sockets.
        boost::asio::signal_set signals(io, SIGINT, SIGTERM);
        signals.async_wait([&io](auto, int sig) {
            std::printf("[tloginsvr_asio] received signal %d, shutting down\n", sig);
            io.stop();
        });

        tloginsvr::LoginServer server(io, port);
        std::printf("[tloginsvr_asio] listening on 0.0.0.0:%u\n", server.Port());

        boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
        io.run();
    }
    catch (const std::exception& ex)
    {
        std::fprintf(stderr, "[tloginsvr_asio] fatal: %s\n", ex.what());
        return 1;
    }
    return 0;
}
