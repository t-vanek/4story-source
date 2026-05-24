// W4-4 wire test: MW_FRIENDLIST_ACK → MW_FRIENDLIST_REQ.
//
// Alice 42/peer1 0x42 with a pre-seeded friend list:
//   Bob   200 (online on peer2, lvl25/cls4/region22, group 1)
//   Carol 300 (offline — never registered, group 0)
//   Pending 999 (FT_TARGET — must be excluded)
// + one friend group {1, "Buddies"}.
//
// Asserts the reply carries the soulmate sentinel, the group, and
// both non-pending friends with online friends resolved live
// (connected + level/class/region) and offline ones zeroed.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
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
#include <string>
#include <thread>
#include <vector>

namespace frnd = tworldsvr::frnd;

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

std::vector<std::byte> ListBody(std::uint32_t cid, std::uint32_t key)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, cid);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    return b;
}

struct Row { std::uint32_t id = 0, region = 0; std::string name;
             std::uint8_t level = 0, group = 0, klass = 0, connected = 0; };
struct List {
    std::uint32_t char_id = 0, key = 0, soulmate = 0;
    std::vector<std::pair<std::uint8_t, std::string>> groups;
    std::vector<Row> friends;
};
List DecodeList(const std::vector<std::byte>& b)
{
    List L{};
    tworldsvr::wire::Reader r(b);
    r.Read(L.char_id); r.Read(L.key); r.Read(L.soulmate);
    std::uint8_t gc = 0; r.Read(gc);
    for (std::uint8_t i = 0; i < gc; ++i)
    { std::uint8_t id = 0; std::string n; r.Read(id); r.ReadString(n);
      L.groups.emplace_back(id, n); }
    std::uint8_t fc = 0; r.Read(fc);
    for (std::uint8_t i = 0; i < fc; ++i)
    {
        Row row;
        r.Read(row.id); r.ReadString(row.name); r.Read(row.level);
        r.Read(row.group); r.Read(row.klass); r.Read(row.connected);
        r.Read(row.region);
        L.friends.push_back(std::move(row));
    }
    return L;
}

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;

    boost::asio::io_context io;
    tworldsvr::CharRegistry   chars;
    tworldsvr::GuildRegistry  guilds;
    tworldsvr::PeerRegistry   peers;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.nation = 0;

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

    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, 0xA1));
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(200, 0xB0));
    for (int i = 0; i < 100; ++i)
    { if (chars.Find(42) && chars.Find(200)) break;
      std::this_thread::sleep_for(10ms); }
    EXPECT(chars.Find(42) && chars.Find(200));

    { auto c = chars.Find(200); std::lock_guard g(c->lock);
      c->level = 25; c->klass = 4; c->region = 22; }
    {
        auto c = chars.Find(42); std::lock_guard g(c->lock);
        c->friend_groups.emplace_back(1, "Buddies");
        c->friends.push_back({200, "Bob",   frnd::kTypeFriendFriend, false,
                              0, 1});
        c->friends.push_back({300, "Carol", frnd::kTypeFriendFriend, false,
                              0, 0});
        c->friends.push_back({999, "Pending", frnd::kTypeTarget, false, 0, 0});
    }

    SendFramed(p1, ToUint16(MessageId::MW_FRIENDLIST_ACK), ListBody(42, 0xA1));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_FRIENDLIST_REQ));
        auto L = DecodeList(b);
        EXPECT(L.char_id == 42); EXPECT(L.key == 0xA1);
        EXPECT(L.soulmate == 0);
        EXPECT(L.groups.size() == 1);
        if (L.groups.size() == 1)
        { EXPECT(L.groups[0].first == 1);
          EXPECT(L.groups[0].second == "Buddies"); }
        EXPECT(L.friends.size() == 2);   // Pending (FT_TARGET) excluded
        if (L.friends.size() == 2)
        {
            const auto& bob = L.friends[0];   // insertion order
            EXPECT(bob.id == 200); EXPECT(bob.name == "Bob");
            EXPECT(bob.group == 1); EXPECT(bob.connected == 1);
            EXPECT(bob.level == 25); EXPECT(bob.klass == 4);
            EXPECT(bob.region == 22);
            const auto& carol = L.friends[1];
            EXPECT(carol.id == 300); EXPECT(carol.name == "Carol");
            EXPECT(carol.connected == 0);     // offline
            EXPECT(carol.level == 0); EXPECT(carol.region == 0);
        }
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_friend_list_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_friend_list_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
