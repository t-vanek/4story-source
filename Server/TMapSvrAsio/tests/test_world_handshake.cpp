// End-to-end cross-server harness: modern TMap ↔ modern TWorld.
//
// Stands up a real tworldsvr::WorldServer on an ephemeral port, points a
// tmapsvr::AsioWorldClient at it, and asserts the map's RW_RELAYSVR_REQ
// registration handshake lands — i.e. TWorld's PeerRegistry ends up
// holding exactly one peer keyed by the wID the map advertised. This is
// the first proof that the two modernized servers speak the same wire
// over a real socket, and the template every later cross-server slice
// (char-enter, party, …) reuses.
//
// No DB, no gameplay — pure transport + dispatch + registration.

#include "world_server.h"             // tworldsvr::WorldServer / WorldServerConfig
#include "services/peer_registry.h"   // tworldsvr::PeerRegistry
#include "services/world_client.h"    // tmapsvr::AsioWorldClient

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>
#include <cstdio>

namespace {
int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)
} // namespace

int main()
{
    using namespace std::chrono_literals;

    boost::asio::io_context io;

    // --- TWorld side: ephemeral listener + a PeerRegistry to observe ---
    tworldsvr::PeerRegistry registry;
    tworldsvr::WorldServerConfig wcfg;
    wcfg.port           = 0;          // ephemeral — ctor binds + reports
    wcfg.max_connections = 16;
    wcfg.ctx.io         = &io;
    wcfg.ctx.peers      = &registry;  // OnRelaysvrReq registers here
    wcfg.ctx.nation     = 0;
    tworldsvr::WorldServer world(io, std::move(wcfg));
    const std::uint16_t port = world.Port();
    EXPECT(port != 0);

    boost::asio::co_spawn(io, world.Run(), boost::asio::detached);

    // --- TMap side: world client that auto-registers on connect -------
    constexpr std::uint16_t kWid = 0x0105;  // group_id=1, server_id=5
    tmapsvr::AsioWorldClient client(
        io, "127.0.0.1", port, /*on_packet=*/nullptr,
        /*backoff_initial=*/20ms, /*backoff_max=*/100ms);
    client.SetRelayWid(kWid);
    boost::asio::co_spawn(io, client.Run(), boost::asio::detached);

    // --- Drive the loop until the map registers (or ~2s timeout) ------
    boost::asio::co_spawn(io,
        [&]() -> boost::asio::awaitable<void> {
            boost::asio::steady_timer t(io);
            for (int i = 0; i < 200 && registry.Size() == 0; ++i)
            {
                t.expires_after(10ms);
                co_await t.async_wait(boost::asio::use_awaitable);
            }
            io.stop();
        },
        boost::asio::detached);

    io.run();

    // --- Assertions ---------------------------------------------------
    EXPECT(registry.Size() == 1);
    if (registry.Size() == 1)
    {
        auto peer = registry.Find(kWid);
        EXPECT(peer != nullptr);
        if (peer)
            EXPECT(peer->Wid() == kWid);
    }

    if (g_fails == 0)
        std::printf("test_world_handshake: map↔world registration OK "
                    "(wid=0x%04X)\n", kWid);
    return g_fails == 0 ? 0 : 1;
}
