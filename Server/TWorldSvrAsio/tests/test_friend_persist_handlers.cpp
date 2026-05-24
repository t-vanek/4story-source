// W4-16 wire test: friend-group write-back persistence.
//
// With a FakeFriendRepository wired in, the four W4-3 group handlers
// (MAKE / NAME / CHANGE / DELETE) persist their mutation through the
// repo in addition to the in-memory change. We drive each over the
// wire and then read the repo back to confirm the write landed.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/fake_friend_repository.h"
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

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
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

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    namespace wire = tworldsvr::wire;

    boost::asio::io_context io;
    tworldsvr::CharRegistry         chars;
    tworldsvr::GuildRegistry        guilds;
    tworldsvr::PeerRegistry         peers;
    tworldsvr::FakeFriendRepository friend_repo;

    // Alice (42) already has Bob (200) as a friend in group 0.
    friend_repo.AddForward(42, tworldsvr::FriendRow{200, "Bob", 10, 2, 0});

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

    const std::uint32_t kKey = 0xA1;
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, kKey));
    for (int i = 0; i < 100; ++i)
    {
        auto c = chars.Find(42);
        if (c) { std::lock_guard g(c->lock); if (!c->friends.empty()) break; }
        std::this_thread::sleep_for(10ms);
    }

    auto group_body = [&](std::uint8_t group, const std::string& name)
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 42);
        wire::WritePOD<std::uint32_t>(b, kKey);
        wire::WritePOD<std::uint8_t>(b, group);
        wire::WriteString(b, name);
        return b;
    };

    // MAKE group 1 "Buddies".
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPMAKE_ACK),
               group_body(1, "Buddies"));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_FRIENDGROUPMAKE_REQ)); }

    // NAME group 1 → "Pals".
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPNAME_ACK),
               group_body(1, "Pals"));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_FRIENDGROUPNAME_REQ)); }

    // CHANGE Bob (200) → group 1.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 42);
        wire::WritePOD<std::uint32_t>(b, kKey);
        wire::WritePOD<std::uint32_t>(b, 200);
        wire::WritePOD<std::uint8_t>(b, 1);
        SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPCHANGE_ACK), b);
    }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_FRIENDGROUPCHANGE_REQ)); }

    // MAKE group 2 "Temp", then DELETE it (empty → success).
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPMAKE_ACK),
               group_body(2, "Temp"));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_FRIENDGROUPMAKE_REQ)); }
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 42);
        wire::WritePOD<std::uint32_t>(b, kKey);
        wire::WritePOD<std::uint8_t>(b, 2);
        SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPDELETE_ACK), b);
    }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_FRIENDGROUPDELETE_REQ)); }

    // Let the last inline persist settle, then read the repo back.
    std::this_thread::sleep_for(30ms);
    {
        auto fl = friend_repo.LoadForChar(42);
        // group 2 was made then deleted; group 1 made then renamed.
        EXPECT(fl.groups.size() == 1);
        bool has_pals = false;
        for (const auto& [g, n] : fl.groups)
            if (g == 1 && n == "Pals") has_pals = true;
        EXPECT(has_pals);
        // Bob's persisted group bucket moved to 1.
        bool bob_in_1 = false;
        for (const auto& f : fl.forward)
            if (f.id == 200) bob_in_1 = (f.group == 1);
        EXPECT(bob_in_1);
    }

    p1.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_friend_persist_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_friend_persist_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
