// W4-20 wire test: login finalization (MW_ENTERSVR_ACK).
//
// Bob (200/peer2) is online with Alice as a mutual friend. Alice
// (42/peer1) finishes loading and sends ENTERSVR: world indexes her
// name, stores her identity/region, and — now that the name/region
// are known — fans a FRIEND_CONNECTION toast to Bob.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/fake_friend_repository.h"
#include "../services/friend_constants.h"
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

// The 25-field MW_ENTERSVR_ACK packet (SSHandler.cpp:1218).
std::vector<std::byte> EnterSvrBody(std::uint32_t char_id, std::uint32_t key,
                                    const std::string& name, std::uint8_t level,
                                    std::uint32_t region)
{
    using namespace tworldsvr::wire;
    std::vector<std::byte> b;
    WritePOD<std::uint32_t>(b, char_id);
    WritePOD<std::uint32_t>(b, key);
    WriteString(b, name);
    WritePOD<std::uint8_t>(b, level);
    WritePOD<std::uint8_t>(b, 0);          // real_sex
    WritePOD<std::uint8_t>(b, 1);          // class
    WritePOD<std::uint8_t>(b, 0);          // race
    WritePOD<std::uint8_t>(b, 0);          // sex
    WritePOD<std::uint8_t>(b, 0);          // face
    WritePOD<std::uint8_t>(b, 0);          // hair
    WritePOD<std::uint8_t>(b, 0);          // helmet_hide
    WritePOD<std::uint8_t>(b, 0);          // country
    WritePOD<std::uint8_t>(b, 0);          // aid_country
    WritePOD<std::uint32_t>(b, region);    // region
    WritePOD<std::uint8_t>(b, 0);          // channel
    WritePOD<std::uint16_t>(b, 1000);      // map_id
    WritePOD<float>(b, 1.0f);              // pos_x
    WritePOD<float>(b, 2.0f);              // pos_y
    WritePOD<float>(b, 3.0f);              // pos_z
    WritePOD<std::uint8_t>(b, 0);          // logout
    WritePOD<std::uint8_t>(b, 0);          // save
    WritePOD<std::uint8_t>(b, 0);          // result (0 = ok)
    WritePOD<std::uint16_t>(b, 0);         // title_id
    WritePOD<std::uint32_t>(b, 0);         // rank_point
    WritePOD<std::uint32_t>(b, 0x7f000001);// user_ip
    return b;
}

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    namespace frnd = tworldsvr::frnd;

    boost::asio::io_context io;
    tworldsvr::CharRegistry         chars;
    tworldsvr::GuildRegistry        guilds;
    tworldsvr::PeerRegistry         peers;
    tworldsvr::FakeFriendRepository friend_repo;

    // Alice (42) and Bob (200) are mutual friends.
    friend_repo.AddForward(42, tworldsvr::FriendRow{200, "Bob", 0, 0, 0});
    friend_repo.AddReverse(42, tworldsvr::FriendRow{200, "Bob", 0, 0, 0});
    friend_repo.AddForward(200, tworldsvr::FriendRow{42, "Alice", 0, 0, 0});
    friend_repo.AddReverse(200, tworldsvr::FriendRow{42, "Alice", 0, 0, 0});

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.friend_repo = &friend_repo; ctx.nation = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0; svr_cfg.max_connections = 4; svr_cfg.ctx = ctx;
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

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0043));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    // Bob online first, then Alice (so her hydrate sees Bob).
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(200, 0xB0));
    for (int i = 0; i < 100; ++i)
    { if (chars.Find(200)) break; std::this_thread::sleep_for(10ms); }
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, 0xA1));
    for (int i = 0; i < 100; ++i)
    {
        auto c = chars.Find(42);
        if (c) { std::lock_guard g(c->lock); if (!c->friends.empty()) break; }
        std::this_thread::sleep_for(10ms);
    }

    // Alice finishes login → name indexed + Bob notified.
    SendFramed(p1, ToUint16(MessageId::MW_ENTERSVR_ACK),
               EnterSvrBody(42, 0xA1, "Alice", 10, /*region=*/5));
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_FRIENDCONNECTION_REQ));
        tworldsvr::wire::Reader r(b);
        std::uint32_t cid = 0, key = 0, region = 0;
        std::uint8_t  flag = 0;
        std::string   name;
        r.Read(cid); r.Read(key); r.Read(flag); r.ReadString(name);
        r.Read(region);
        EXPECT(cid == 200);                 // delivered to Bob
        EXPECT(flag == frnd::kConnection);
        EXPECT(name == "Alice");
        EXPECT(region == 5);
    }

    // Name index now resolves Alice; Bob's entry for her is connected.
    {
        auto a = chars.FindByName("Alice");
        EXPECT(a != nullptr);
        if (a) EXPECT(a->char_id == 42);
    }
    {
        auto bob = chars.Find(200);
        EXPECT(bob != nullptr);
        if (bob)
        {
            std::lock_guard g(bob->lock);
            bool found = false;
            for (const auto& f : bob->friends)
                if (f.id == 42) { found = true; EXPECT(f.connected); }
            EXPECT(found);
        }
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_entersvr_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_entersvr_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
