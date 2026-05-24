// W4-15 wire test: friend / group / soulmate load-at-login.
//
// A FakeFriendRepository is seeded for Alice (42):
//   - group (1, "Buddies")
//   - forward friend Bob (200) — she added him
//   - reverse edge Carol (300) — Carol added Alice (one-way)
// Bob is brought online first, then Alice's MW_ADDCHAR_ACK triggers
// the hydrate. We assert the in-memory TChar reflects the loaded
// graph with the right derived types + live connected flags.

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

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    namespace frnd = tworldsvr::frnd;

    boost::asio::io_context io;
    tworldsvr::CharRegistry      chars;
    tworldsvr::GuildRegistry     guilds;
    tworldsvr::PeerRegistry      peers;
    tworldsvr::FakeFriendRepository friend_repo;

    // Seed Alice's persistent social graph.
    friend_repo.AddGroup(42, 1, "Buddies");
    friend_repo.AddForward(42, tworldsvr::FriendRow{200, "Bob", 10, 2, 1});
    friend_repo.AddReverse(42, tworldsvr::FriendRow{300, "Carol", 0, 0, 0});

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
    tcp::socket p1(client_io);
    p1.connect(tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port));
    std::this_thread::sleep_for(20ms);

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }

    // Bob online first, so Alice's load sees him connected.
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(200, 0xB0));
    for (int i = 0; i < 100; ++i)
    { if (chars.Find(200)) break; std::this_thread::sleep_for(10ms); }
    EXPECT(chars.Find(200));

    // Alice online → hydrate her friends/groups from the repo.
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, 0xA1));
    for (int i = 0; i < 100; ++i)
    {
        auto c = chars.Find(42);
        if (c) { std::lock_guard g(c->lock); if (!c->friends.empty()) break; }
        std::this_thread::sleep_for(10ms);
    }

    auto alice = chars.Find(42);
    EXPECT(alice != nullptr);
    if (alice)
    {
        std::lock_guard g(alice->lock);

        // Group loaded.
        EXPECT(alice->friend_groups.size() == 1);
        if (!alice->friend_groups.empty())
        {
            EXPECT(alice->friend_groups[0].first == 1);
            EXPECT(alice->friend_groups[0].second == "Buddies");
        }

        // Two friend entries: Bob (forward) + Carol (reverse target).
        EXPECT(alice->friends.size() == 2);
        const tworldsvr::TFriend* bob = nullptr;
        const tworldsvr::TFriend* carol = nullptr;
        for (const auto& f : alice->friends)
        {
            if (f.id == 200) bob = &f;
            if (f.id == 300) carol = &f;
        }
        EXPECT(bob != nullptr);
        if (bob)
        {
            EXPECT(bob->name == "Bob");
            EXPECT(bob->type == frnd::kTypeFriend);  // forward-only
            EXPECT(bob->group == 1);
            EXPECT(bob->connected == true);          // Bob is online
        }
        EXPECT(carol != nullptr);
        if (carol)
        {
            EXPECT(carol->name == "Carol");
            EXPECT(carol->type == frnd::kTypeTarget);  // reverse-only
            EXPECT(carol->connected == false);         // Carol offline
        }
    }

    p1.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_friend_load_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_friend_load_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
