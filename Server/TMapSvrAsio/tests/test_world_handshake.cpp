// End-to-end cross-server harness: modern TMap ↔ modern TWorld.
//
// Stands up a real tworldsvr::WorldServer on an ephemeral port, points a
// tmapsvr::AsioWorldClient at it, and drives the first two char-lifecycle
// hops over a real socket:
//
//   1. RW_RELAYSVR_REQ — the map registers; TWorld's PeerRegistry ends up
//      holding the wID the map advertised.
//   2. MW_ADDCHAR_ACK  — a char comes online on the map; TWorld's
//      OnAddCharAck registers it in CharRegistry keyed to the map's
//      server_id (= LOBYTE of the registered wID).
//
// This is the proof that the two modernized servers speak the same wire
// and that the Map→World char-registration contract works, plus the
// template every later cross-server slice reuses. No DB, no gameplay.

#include "world_server.h"             // tworldsvr::WorldServer / WorldServerConfig
#include "services/peer_registry.h"   // tworldsvr::PeerRegistry
#include "services/char_registry.h"   // tworldsvr::CharRegistry / TChar
#include "services/world_client.h"    // tmapsvr::AsioWorldClient

#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>
#include <cstdio>
#include <vector>

namespace {
int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

void AppendU32(std::vector<std::byte>& v, std::uint32_t x)
{
    v.push_back(static_cast<std::byte>( x        & 0xFF));
    v.push_back(static_cast<std::byte>((x >>  8) & 0xFF));
    v.push_back(static_cast<std::byte>((x >> 16) & 0xFF));
    v.push_back(static_cast<std::byte>((x >> 24) & 0xFF));
}
void AppendU16(std::vector<std::byte>& v, std::uint16_t x)
{
    v.push_back(static_cast<std::byte>( x       & 0xFF));
    v.push_back(static_cast<std::byte>((x >> 8) & 0xFF));
}
} // namespace

int main()
{
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;

    boost::asio::io_context io;

    // --- TWorld side: ephemeral listener + the registries to observe --
    tworldsvr::PeerRegistry registry;
    tworldsvr::CharRegistry chars;
    tworldsvr::WorldServerConfig wcfg;
    wcfg.port            = 0;          // ephemeral — ctor binds + reports
    wcfg.max_connections = 16;
    wcfg.ctx.io          = &io;
    wcfg.ctx.peers       = &registry; // OnRelaysvrReq registers here
    wcfg.ctx.chars       = &chars;    // OnAddCharAck registers here
    wcfg.ctx.nation      = 0;
    tworldsvr::WorldServer world(io, std::move(wcfg));
    const std::uint16_t port = world.Port();
    EXPECT(port != 0);

    boost::asio::co_spawn(io, world.Run(), boost::asio::detached);

    // --- TMap side: world client that auto-registers on connect -------
    constexpr std::uint16_t kWid       = 0x0105;     // group_id=1, server_id=5
    constexpr std::uint8_t  kServerId  = 0x05;
    constexpr std::uint32_t kCharId    = 0xABCDEF01;
    constexpr std::uint32_t kKey       = 0x12345678;
    constexpr std::uint32_t kUserId    = 4242;
    tmapsvr::AsioWorldClient client(
        io, "127.0.0.1", port, /*on_packet=*/nullptr,
        /*backoff_initial=*/20ms, /*backoff_max=*/100ms);
    client.SetRelayWid(kWid);
    boost::asio::co_spawn(io, client.Run(), boost::asio::detached);

    // --- Driver: wait for registration, push a char, wait for it ------
    boost::asio::co_spawn(io,
        [&]() -> boost::asio::awaitable<void> {
            boost::asio::steady_timer t(io);
            // Phase 1 — map registers.
            for (int i = 0; i < 200 && registry.Size() == 0; ++i)
            {
                t.expires_after(10ms);
                co_await t.async_wait(boost::asio::use_awaitable);
            }
            // Phase 2 — a char comes online: send MW_ADDCHAR_ACK
            // (char_id, key, ip, port, user_id), the 18-byte body
            // OnAddCharAck parses.
            if (registry.Size() == 1)
            {
                std::vector<std::byte> body;
                AppendU32(body, kCharId);
                AppendU32(body, kKey);
                AppendU32(body, 0x0100007F);    // 127.0.0.1
                AppendU16(body, 12345);          // client port
                AppendU32(body, kUserId);
                co_await client.SendPacket(
                    static_cast<std::uint16_t>(MessageId::MW_ADDCHAR_ACK),
                    std::move(body));
            }
            // Phase 3 — World registers the char.
            for (int i = 0; i < 200 && chars.Size() == 0; ++i)
            {
                t.expires_after(10ms);
                co_await t.async_wait(boost::asio::use_awaitable);
            }
            io.stop();
        },
        boost::asio::detached);

    io.run();

    // --- Assertions ---------------------------------------------------
    // Step 1: registration handshake.
    EXPECT(registry.Size() == 1);
    if (registry.Size() == 1)
    {
        auto peer = registry.Find(kWid);
        EXPECT(peer != nullptr);
        if (peer)
            EXPECT(peer->Wid() == kWid);
    }

    // Step 2: char registration keyed to the map's server_id.
    EXPECT(chars.Size() == 1);
    auto ch = chars.Find(kCharId);
    EXPECT(ch != nullptr);
    if (ch)
    {
        EXPECT(ch->char_id == kCharId);
        EXPECT(ch->key == kKey);
        EXPECT(ch->main_server_id == kServerId);
    }

    if (g_fails == 0)
        std::printf("test_world_handshake: register + char-add OK "
                    "(wid=0x%04X char=0x%08X)\n", kWid, kCharId);
    return g_fails == 0 ? 0 : 1;
}
