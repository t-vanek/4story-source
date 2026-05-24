// W4-21 wire test: friend-protected presence sync (MW_PROTECTEDCHECK_ACK).
//
// Two map peers (svr 0x42 = Alice's main, 0x43 = Bob's main). Alice
// and Bob hold each other as FT_FRIENDFRIEND. PROTECTEDCHECK fires
// from Alice's map driving connect / disconnect / silent-drop cases:
//
//   * Connect — both edges flip to connected; Alice's edge picks up
//     Bob's region (777), Bob's edge picks up Alice's region (555);
//     a FRIENDCONNECTION(CONN) relay lands on Bob's map naming
//     Alice with Alice's region.
//   * Disconnect — both edges drop connected; a FRIENDCONNECTION
//     (DISC) relay lands on Bob's map with region=0 (legacy:
//     `!bConnect ? region : 0`).
//   * Unknown friend name — silent drop (no relay).
//   * Unknown char_id — silent drop.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/guild_registry.h"
#include "../services/peer_registry.h"
#include "../wire_codec.h"
#include "../world_server.h"
#include "../world_session.h"

#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

void SendFramed(boost::asio::ip::tcp::socket& sock, std::uint16_t wId,
                const std::vector<std::byte>& body)
{
    tworldsvr::PacketHeader hdr{};
    hdr.wSize    = static_cast<std::uint16_t>(
        tworldsvr::kPacketHeaderSize + body.size());
    hdr.wID      = wId;
    hdr.dwChkSum = tworldsvr::ComputeChecksum(body.data(), body.size());
    std::vector<std::byte> buf(hdr.wSize);
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    if (!body.empty())
        std::memcpy(buf.data() + sizeof(hdr), body.data(), body.size());
    boost::asio::write(sock, boost::asio::buffer(buf.data(), buf.size()));
}

std::pair<std::uint16_t, std::vector<std::byte>>
ReadFramed(boost::asio::ip::tcp::socket& sock)
{
    tworldsvr::PacketHeader hdr{};
    boost::asio::read(sock, boost::asio::buffer(&hdr, sizeof(hdr)));
    const std::size_t body_size = hdr.wSize - tworldsvr::kPacketHeaderSize;
    std::vector<std::byte> body(body_size);
    if (body_size > 0)
        boost::asio::read(sock, boost::asio::buffer(body.data(), body_size));
    return {hdr.wID, std::move(body)};
}

std::vector<std::byte> RelaysvrBody(std::uint16_t wid)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint16_t>(b, wid);
    return b;
}

std::vector<std::byte> AddCharBody(std::uint32_t char_id, std::uint32_t key)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, 0x7f000001);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, 33500);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, /*user_id=*/100);
    return b;
}

// MW_PROTECTEDCHECK_ACK: DWORD char_id, key, BYTE connect, STRING name.
std::vector<std::byte> ProtectedCheckBody(std::uint32_t char_id, std::uint32_t key,
                                          std::uint8_t connect,
                                          const std::string& name)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, connect);
    tworldsvr::wire::WriteString(b, name);
    return b;
}

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    namespace wire = tworldsvr::wire;

    boost::asio::io_context io;
    tworldsvr::CharRegistry   chars;
    tworldsvr::GuildRegistry  guilds;
    tworldsvr::PeerRegistry   peers;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.nation = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0; svr_cfg.max_connections = 8; svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket p1(client_io), p2(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep); p2.connect(ep);
    std::this_thread::sleep_for(20ms);

    // p1=0x42 (Alice's main), p2=0x43 (Bob's main).
    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0043));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    // Alice (100, key=0xA1) on p1; Bob (200, key=0xB0) on p2.
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK),
               AddCharBody(/*char_id=*/100, /*key=*/0xA1));
    for (int i = 0; i < 200 && !chars.Find(100); ++i)
        std::this_thread::sleep_for(10ms);
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK),
               AddCharBody(200, 0xB0));
    for (int i = 0; i < 200 && !chars.Find(200); ++i)
        std::this_thread::sleep_for(10ms);

    // Wire up the mutual friendship (FT_FRIENDFRIEND on both sides),
    // names + regions. Register the names with the registry so
    // FindByName can resolve them.
    EXPECT(chars.Rename(100, "Alice"));
    EXPECT(chars.Rename(200, "Bob"));
    {
        auto a = chars.Find(100);
        std::lock_guard g(a->lock);
        a->name = "Alice";
        a->region = 555;
        a->friends.push_back(tworldsvr::TFriend{
            /*id=*/200, /*name=*/"Bob", /*type=*/2 /*FT_FRIENDFRIEND*/,
            /*connected=*/false, /*region=*/0, /*group=*/0});
    }
    {
        auto b = chars.Find(200);
        std::lock_guard g(b->lock);
        b->name = "Bob";
        b->region = 777;
        b->friends.push_back(tworldsvr::TFriend{
            /*id=*/100, /*name=*/"Alice", /*type=*/2 /*FT_FRIENDFRIEND*/,
            /*connected=*/false, /*region=*/0, /*group=*/0});
    }

    auto edge_state = [&](std::uint32_t self_id, std::uint32_t friend_id) {
        struct { bool connected; std::uint32_t region; } out{false, 0};
        auto c = chars.Find(self_id);
        std::lock_guard g(c->lock);
        for (const auto& f : c->friends)
            if (f.id == friend_id)
            {
                out.connected = f.connected;
                out.region    = f.region;
                break;
            }
        return out;
    };

    // --- Test A: connect — relay lands on Bob's map; both edges flip
    {
        SendFramed(p1, ToUint16(MessageId::MW_PROTECTEDCHECK_ACK),
                   ProtectedCheckBody(100, 0xA1,
                       /*connect=*/0 /*FRIEND_CONNECTION*/, "Bob"));
        auto [w, got] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_FRIENDCONNECTION_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0;
        std::uint8_t  result = 0xFF;
        std::string   name_in;
        std::uint32_t region = 0;
        r.Read(cid); r.Read(key); r.Read(result);
        r.ReadString(name_in); r.Read(region);
        EXPECT(cid == 200);            // Bob
        EXPECT(key == 0xB0);
        EXPECT(result == 0);           // FRIEND_CONNECTION
        EXPECT(name_in == "Alice");
        EXPECT(region == 555);         // Alice's region

        // Edges updated cross-wise (Alice's edge gets Bob's region,
        // Bob's edge gets Alice's). Poll to outlast any handler
        // re-ordering of the two locks.
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        {
            auto a_to_b = edge_state(100, 200);
            auto b_to_a = edge_state(200, 100);
            if (a_to_b.connected && a_to_b.region == 777 &&
                b_to_a.connected && b_to_a.region == 555)
            { ok = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(ok);
    }

    // --- Test B: disconnect — relay has region=0; both edges drop
    {
        SendFramed(p1, ToUint16(MessageId::MW_PROTECTEDCHECK_ACK),
                   ProtectedCheckBody(100, 0xA1,
                       /*connect=*/1 /*FRIEND_DISCONNECTION*/, "Bob"));
        auto [w, got] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_FRIENDCONNECTION_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0;
        std::uint8_t  result = 0;
        std::string   name_in;
        std::uint32_t region = 0xDEADBEEF;
        r.Read(cid); r.Read(key); r.Read(result);
        r.ReadString(name_in); r.Read(region);
        EXPECT(cid == 200);
        EXPECT(result == 1);           // FRIEND_DISCONNECTION
        EXPECT(name_in == "Alice");
        EXPECT(region == 0);           // legacy: !bConnect → region : 0

        bool ok = false;
        for (int i = 0; i < 200; ++i)
        {
            auto a_to_b = edge_state(100, 200);
            auto b_to_a = edge_state(200, 100);
            if (!a_to_b.connected && !b_to_a.connected)
            { ok = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(ok);
    }

    // --- Test C: unknown friend name — silent drop ------------------
    //  Verify by sending an unknown name and then a known name; if
    //  the unknown packet had emitted anything, it would arrive on p2
    //  ahead of the known one.
    {
        SendFramed(p1, ToUint16(MessageId::MW_PROTECTEDCHECK_ACK),
                   ProtectedCheckBody(100, 0xA1,
                       /*connect=*/0, "Charlie"));   // no such friend
        SendFramed(p1, ToUint16(MessageId::MW_PROTECTEDCHECK_ACK),
                   ProtectedCheckBody(100, 0xA1,
                       /*connect=*/0, "Bob"));        // emits a relay
        auto [w, got] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_FRIENDCONNECTION_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0;
        std::uint8_t  result = 0xFF;
        std::string   name_in;
        std::uint32_t region = 0;
        r.Read(cid); r.Read(key); r.Read(result);
        r.ReadString(name_in); r.Read(region);
        // The relay we read is from the known-name packet, not the
        // unknown one — confirms the unknown one didn't emit anything.
        EXPECT(cid == 200);
        EXPECT(name_in == "Alice");
    }

    // --- Test D: unknown char_id — silent drop ----------------------
    //  Same gate as Test C: send a no-op packet, then a known one,
    //  and confirm the relay we read is the known one's.
    {
        SendFramed(p1, ToUint16(MessageId::MW_PROTECTEDCHECK_ACK),
                   ProtectedCheckBody(/*char_id=*/999, /*key=*/0xCC,
                       /*connect=*/0, "Bob"));
        SendFramed(p1, ToUint16(MessageId::MW_PROTECTEDCHECK_ACK),
                   ProtectedCheckBody(100, 0xA1,
                       /*connect=*/1, "Bob"));   // disconnect relay
        auto [w, got] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_FRIENDCONNECTION_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0;
        std::uint8_t  result = 0;
        std::string   name_in;
        std::uint32_t region = 0;
        r.Read(cid); r.Read(key); r.Read(result);
        r.ReadString(name_in); r.Read(region);
        EXPECT(cid == 200);
        EXPECT(result == 1);           // disconnect (from the known one)
        EXPECT(name_in == "Alice");
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_protected_check_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_protected_check_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
