// W4-3 wire test: friend-group handlers (MAKE / DELETE / CHANGE /
// NAME) over a single-peer loopback session — groups are per-char.
//
// Alice 42/peer1 0x42, pre-seeded with one friend Bob (id 200).
//
// Scenarios: MAKE success / id-taken ALREADY / group-0 MAX;
// CHANGE a friend into the group; DELETE an occupied group (REFUSE)
// then ungroup + DELETE empty (SUCCESS); MAKE + NAME rename success
// / name-taken REFUSE / unknown-group NOTFOUND.

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

std::vector<std::byte> MakeBody(std::uint32_t cid, std::uint32_t key,
                                 std::uint8_t group, const std::string& name)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, cid);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, group);
    tworldsvr::wire::WriteString(b, name);
    return b;
}

std::vector<std::byte> DelBody(std::uint32_t cid, std::uint32_t key,
                                std::uint8_t group)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, cid);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, group);
    return b;
}

std::vector<std::byte> ChangeBody(std::uint32_t cid, std::uint32_t key,
                                   std::uint32_t fid, std::uint8_t group)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, cid);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, fid);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, group);
    return b;
}

struct GroupReq {
    std::uint32_t char_id = 0, key = 0, friend_id = 0;
    std::uint8_t  result = 0, group = 0;
    std::string   name;
};
GroupReq DecodeMakeName(const std::vector<std::byte>& b)
{
    GroupReq a{};
    tworldsvr::wire::Reader r(b);
    r.Read(a.char_id); r.Read(a.key); r.Read(a.result); r.Read(a.group);
    r.ReadString(a.name);
    return a;
}
GroupReq DecodeDelete(const std::vector<std::byte>& b)
{
    GroupReq a{};
    tworldsvr::wire::Reader r(b);
    r.Read(a.char_id); r.Read(a.key); r.Read(a.result); r.Read(a.group);
    return a;
}
GroupReq DecodeChange(const std::vector<std::byte>& b)
{
    GroupReq a{};
    tworldsvr::wire::Reader r(b);
    r.Read(a.char_id); r.Read(a.key); r.Read(a.result); r.Read(a.group);
    r.Read(a.friend_id);
    return a;
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
    tcp::socket p1(client_io);
    p1.connect(tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port));
    std::this_thread::sleep_for(20ms);

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, 0xA1));
    for (int i = 0; i < 100; ++i)
    { if (chars.Find(42)) break; std::this_thread::sleep_for(10ms); }
    EXPECT(chars.Find(42));
    { auto c = chars.Find(42); std::lock_guard g(c->lock);
      c->friends.push_back({200, "Bob", frnd::kTypeFriendFriend, true, 0, 0}); }

    auto poll = [&](auto pred) {
        for (int i = 0; i < 200; ++i)
        { if (pred()) return true; std::this_thread::sleep_for(5ms); }
        return pred();
    };
    auto group_count = [&]() -> std::size_t {
        auto c = chars.Find(42); std::lock_guard g(c->lock);
        return c->friend_groups.size();
    };
    auto bob_group = [&]() -> int {
        auto c = chars.Find(42); std::lock_guard g(c->lock);
        for (const auto& f : c->friends) if (f.id == 200) return f.group;
        return -1;
    };

    const std::uint16_t kMake = ToUint16(MessageId::MW_FRIENDGROUPMAKE_REQ);
    const std::uint16_t kDel  = ToUint16(MessageId::MW_FRIENDGROUPDELETE_REQ);
    const std::uint16_t kChg  = ToUint16(MessageId::MW_FRIENDGROUPCHANGE_REQ);
    const std::uint16_t kName = ToUint16(MessageId::MW_FRIENDGROUPNAME_REQ);

    // --- MAKE success -----------------------------------------------
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPMAKE_ACK),
        MakeBody(42, 0xA1, 1, "Buddies"));
    { auto [w, b] = ReadFramed(p1); EXPECT(w == kMake);
      auto a = DecodeMakeName(b); EXPECT(a.result == frnd::kSuccess);
      EXPECT(a.group == 1); EXPECT(a.name == "Buddies"); }
    EXPECT(poll([&] { return group_count() == 1; }));

    // --- MAKE id-taken → ALREADY ------------------------------------
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPMAKE_ACK),
        MakeBody(42, 0xA1, 1, "Other"));
    { auto [w, b] = ReadFramed(p1); EXPECT(w == kMake);
      EXPECT(DecodeMakeName(b).result == frnd::kAlready); }

    // --- MAKE group 0 → MAX -----------------------------------------
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPMAKE_ACK),
        MakeBody(42, 0xA1, 0, "Zero"));
    { auto [w, b] = ReadFramed(p1); EXPECT(w == kMake);
      EXPECT(DecodeMakeName(b).result == frnd::kMax); }

    // --- CHANGE Bob into group 1 → SUCCESS --------------------------
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPCHANGE_ACK),
        ChangeBody(42, 0xA1, 200, 1));
    { auto [w, b] = ReadFramed(p1); EXPECT(w == kChg);
      auto a = DecodeChange(b); EXPECT(a.result == frnd::kSuccess);
      EXPECT(a.group == 1); EXPECT(a.friend_id == 200); }
    EXPECT(poll([&] { return bob_group() == 1; }));

    // --- DELETE occupied group 1 → REFUSE ---------------------------
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPDELETE_ACK),
        DelBody(42, 0xA1, 1));
    { auto [w, b] = ReadFramed(p1); EXPECT(w == kDel);
      EXPECT(DecodeDelete(b).result == frnd::kRefuse); }
    EXPECT(group_count() == 1);   // still there

    // --- CHANGE Bob to ungrouped (0), then DELETE empty → SUCCESS ---
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPCHANGE_ACK),
        ChangeBody(42, 0xA1, 200, 0));
    { auto [w, b] = ReadFramed(p1); EXPECT(w == kChg);
      EXPECT(DecodeChange(b).result == frnd::kSuccess); }
    EXPECT(poll([&] { return bob_group() == 0; }));
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPDELETE_ACK),
        DelBody(42, 0xA1, 1));
    { auto [w, b] = ReadFramed(p1); EXPECT(w == kDel);
      EXPECT(DecodeDelete(b).result == frnd::kSuccess); }
    EXPECT(poll([&] { return group_count() == 0; }));

    // --- NAME: make group 2, rename, name-taken, unknown-group ------
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPMAKE_ACK),
        MakeBody(42, 0xA1, 2, "Foes"));
    { auto [w, b] = ReadFramed(p1); EXPECT(w == kMake);
      EXPECT(DecodeMakeName(b).result == frnd::kSuccess); }
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPNAME_ACK),
        MakeBody(42, 0xA1, 2, "Pals"));
    { auto [w, b] = ReadFramed(p1); EXPECT(w == kName);
      auto a = DecodeMakeName(b); EXPECT(a.result == frnd::kSuccess);
      EXPECT(a.group == 2); EXPECT(a.name == "Pals"); }
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPNAME_ACK),
        MakeBody(42, 0xA1, 2, "Pals"));   // same name now taken (by itself)
    { auto [w, b] = ReadFramed(p1); EXPECT(w == kName);
      EXPECT(DecodeMakeName(b).result == frnd::kRefuse); }
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDGROUPNAME_ACK),
        MakeBody(42, 0xA1, 9, "Solo"));   // group 9 doesn't exist
    { auto [w, b] = ReadFramed(p1); EXPECT(w == kName);
      EXPECT(DecodeMakeName(b).result == frnd::kNotFound); }

    p1.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_friend_group_handlers "
                    "(11 checks)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_friend_group_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
