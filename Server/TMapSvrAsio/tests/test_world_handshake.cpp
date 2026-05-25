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
#include "services/world_senders.h"   // tmapsvr::EncodeEnterSvrAck
#include "domain/character.h"         // tmapsvr::CharSnapshot

#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>
#include <cstdio>
#include <span>
#include <string>
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

// Captures the RW_ENTERCHAR_ACK the map receives back from TWorld.
struct EnterCharAck
{
    bool          got     = false;
    std::uint32_t char_id = 0;
    std::uint8_t  result  = 0xFF;
    std::string   name;
};

// Decode the RW_ENTERCHAR_ACK prefix: u32 char_id, length-prefixed name,
// u8 result (the rest — guild/party/etc. — isn't needed for the proof).
void DecodeEnterCharAck(std::span<const std::byte> b, EnterCharAck& out)
{
    auto u8 = [](std::byte x) { return std::to_integer<std::uint32_t>(x); };
    std::size_t off = 0;
    auto need = [&](std::size_t n) { return off + n <= b.size(); };
    if (!need(4)) return;
    out.char_id = u8(b[0]) | (u8(b[1]) << 8) | (u8(b[2]) << 16) | (u8(b[3]) << 24);
    off = 4;
    if (!need(4)) return;
    const std::int32_t len = static_cast<std::int32_t>(
        u8(b[off]) | (u8(b[off+1]) << 8) | (u8(b[off+2]) << 16) | (u8(b[off+3]) << 24));
    off += 4;
    if (len < 0 || !need(static_cast<std::size_t>(len))) return;
    out.name.assign(reinterpret_cast<const char*>(b.data() + off),
                    static_cast<std::size_t>(len));
    off += static_cast<std::size_t>(len);
    if (!need(1)) return;
    out.result = static_cast<std::uint8_t>(u8(b[off]));
    out.got = true;
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
    EnterCharAck ack;
    tmapsvr::AsioWorldClient client(
        io, "127.0.0.1", port,
        // Capture the World→Map reply we care about; ignore the rest
        // (RW_RELAYSVR_ACK + the CharInfo/Route/FriendList fan-out
        // OnEnterSvrAck pushes — those still prove the reverse wire
        // decodes cleanly by not erroring the read loop).
        [&ack](std::uint16_t wId, std::span<const std::byte> body) {
            if (wId == static_cast<std::uint16_t>(
                    MessageId::RW_ENTERCHAR_ACK))
                DecodeEnterCharAck(body, ack);
        },
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
            // Phase 4 — char fully enters: send MW_ENTERSVR_ACK carrying
            // the loaded identity. OnEnterSvrAck fills level/class/map/pos
            // (and re-indexes the name). Built via the production encoder.
            if (auto ch = chars.Find(kCharId))
            {
                tmapsvr::CharSnapshot snap;
                snap.dwCharID = kCharId;
                snap.szNAME   = "Hero";
                snap.bLevel   = 77;
                snap.bClass   = 3;
                snap.bRace    = 2;
                snap.wMapID   = 60;
                snap.fPosX    = 100.5f;
                snap.fPosY    = 7.0f;
                snap.fPosZ    = -50.25f;
                co_await client.SendPacket(
                    static_cast<std::uint16_t>(MessageId::MW_ENTERSVR_ACK),
                    tmapsvr::EncodeEnterSvrAck(
                        snap, kKey, /*aid_country=*/0, /*channel=*/1,
                        /*logout=*/0, /*save=*/0, /*result=*/0,
                        /*title_id=*/0, /*rank_point=*/0, /*user_ip=*/0));
                // Phase 5 — wait for the identity to land.
                for (int i = 0; i < 200; ++i)
                {
                    { std::lock_guard g(ch->lock);
                      if (ch->level == 77) break; }
                    t.expires_after(10ms);
                    co_await t.async_wait(boost::asio::use_awaitable);
                }
                // Phase 6 — relay char lookup round-trip: ask TWorld to
                // resolve the char by name; OnEnterCharReq replies
                // RW_ENTERCHAR_ACK(result=1) since the name now indexes.
                // This exercises the World→Map reply direction too.
                co_await client.SendPacket(
                    static_cast<std::uint16_t>(MessageId::RW_ENTERCHAR_REQ),
                    tmapsvr::EncodeEnterCharReq(kCharId, "Hero"));
                for (int i = 0; i < 200 && !ack.got; ++i)
                {
                    t.expires_after(10ms);
                    co_await t.async_wait(boost::asio::use_awaitable);
                }
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
        // Step 3: MW_ENTERSVR_ACK filled the loaded identity.
        std::lock_guard g(ch->lock);
        EXPECT(ch->level == 77);
        EXPECT(ch->klass == 3);
        EXPECT(ch->map_id == 60);
        EXPECT(ch->pos_x == 100.5f);
        EXPECT(ch->name == "Hero");
    }

    // Step 4: RW_ENTERCHAR round-trip — the map decoded TWorld's reply.
    EXPECT(ack.got);
    EXPECT(ack.result == 1);
    EXPECT(ack.char_id == kCharId);
    EXPECT(ack.name == "Hero");

    if (g_fails == 0)
        std::printf("test_world_handshake: register + char-add + entersvr + "
                    "entercharack OK (wid=0x%04X char=0x%08X)\n",
                    kWid, kCharId);
    return g_fails == 0 ? 0 : 1;
}
