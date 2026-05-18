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

// Same byte sequence as Session.cpp:16 — the wire-format secret the
// shipped legacy client encrypts with. Reproduced here so a default
// build of tloginsvr_asio can accept a real client without external
// config. Override via --no-rc4 (server-server compatible mode) or
// --secret-key FILE.
constexpr unsigned char kDefaultLegacySecret[] =
    "A5$$8AFS13A1::-11#!..'\x92" "19716AC&\x94" "/D1;;1#";
constexpr std::size_t   kDefaultLegacySecretLen = sizeof(kDefaultLegacySecret) - 1;

void Usage()
{
    std::printf(
        "tloginsvr_asio — modernized 4Story login server (work-in-progress)\n"
        "Usage: tloginsvr_asio [--port N] [--no-rc4]\n"
        "  --port N   listen on TCP port N (default: %u)\n"
        "  --no-rc4   disable RC4 wire-decrypt layer (server-server test mode);\n"
        "             default ON with the legacy wire-format secret so a real\n"
        "             4Story client connection can decrypt end-to-end.\n",
        kDefaultPort);
}

} // namespace

int main(int argc, char** argv)
{
    tloginsvr::LoginServerConfig cfg;
    cfg.port = kDefaultPort;
    cfg.rc4_secret_key.assign(
        reinterpret_cast<const std::byte*>(kDefaultLegacySecret),
        reinterpret_cast<const std::byte*>(kDefaultLegacySecret) + kDefaultLegacySecretLen);

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc)
        {
            cfg.port = static_cast<std::uint16_t>(std::atoi(argv[++i]));
        }
        else if (std::strcmp(argv[i], "--no-rc4") == 0)
        {
            cfg.rc4_secret_key.clear();
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

        tloginsvr::LoginServer server(io, cfg);
        std::printf("[tloginsvr_asio] listening on 0.0.0.0:%u (RC4: %s)\n",
            server.Port(),
            cfg.rc4_secret_key.empty() ? "disabled (server-server test mode)" : "enabled");

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
