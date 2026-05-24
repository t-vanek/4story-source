// W4-2 wire test: MW_FRIENDREPLY_ACK (accept/reject) +
// MW_FRIENDERASE_ACK over a three-peer loopback session.
//
// Alice 42/peer1 0x42 (lvl20/cls3/region11), Bob 200/peer2 0x43
// (lvl25/cls4/region22), Carol 400/peer3 0x44.
//
// Scenarios:
//   1. Bob accepts Alice's request → both get FRIENDADD SUCCESS +
//      mutual FT_FRIENDFRIEND entries (regions swapped in).
//   2. Bob declines Carol → Carol gets the reply code.
//   3. Inviter offline → answerer gets FRIEND_NOTFOUND.
//   4. Alice erases Bob (mutual) → demote: Alice→FT_TARGET, Bob→
//      FT_FRIEND; Alice gets FRIEND_SUCCESS.
//   5. Erase a non-friend → FRIEND_NOTFOUND.
//   6. Bob erases Alice (one-way FT_FRIEND) → both entries removed.

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

std::vector<std::byte> ReplyBody(std::uint32_t char_id, std::uint32_t key,
                                  const std::string& inviter, std::uint8_t rep)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, char_id);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    tworldsvr::wire::WriteString(b, inviter);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, rep);
    return b;
}

std::vector<std::byte> EraseBody(std::uint32_t char_id, std::uint32_t key,
                                  std::uint32_t target)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, char_id);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, target);
    return b;
}

struct AddReq {
    std::uint32_t char_id = 0, key = 0, friend_id = 0, region = 0;
    std::uint8_t  result = 0, level = 0, group = 0, klass = 0;
    std::string   name;
};
AddReq DecodeAdd(const std::vector<std::byte>& b)
{
    AddReq a{};
    tworldsvr::wire::Reader r(b);
    r.Read(a.char_id); r.Read(a.key); r.Read(a.result); r.Read(a.friend_id);
    r.ReadString(a.name); r.Read(a.level); r.Read(a.group); r.Read(a.klass);
    r.Read(a.region);
    return a;
}

struct EraseReq { std::uint32_t char_id = 0, key = 0, target = 0;
                  std::uint8_t result = 0; };
EraseReq DecodeErase(const std::vector<std::byte>& b)
{
    EraseReq a{};
    tworldsvr::wire::Reader r(b);
    r.Read(a.char_id); r.Read(a.key); r.Read(a.result); r.Read(a.target);
    return a;
}

const std::uint16_t kAddReq =
    tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_FRIENDADD_REQ);
const std::uint16_t kEraseReq = tnetlib::protocol::ToUint16(
    tnetlib::protocol::MessageId::MW_FRIENDERASE_REQ);

// friend entry type for `id` in char `cid` (255 = absent).
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
    svr_cfg.port = 0; svr_cfg.max_connections = 6; svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket p1(client_io), p2(client_io), p3(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep); p2.connect(ep); p3.connect(ep);
    std::this_thread::sleep_for(20ms);

    auto reg = [&](tcp::socket& s, std::uint16_t wid) {
        SendFramed(s, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(wid));
        auto [w, _] = ReadFramed(s);
        EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK));
    };
    auto drain = [&](tcp::socket& s) {
        auto [w, _] = ReadFramed(s);
        EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ));
    };
    reg(p1, 0x0042);
    reg(p2, 0x0043); drain(p1);
    reg(p3, 0x0044); drain(p1); drain(p2);

    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, 0xA1));
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(200, 0xB0));
    SendFramed(p3, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(400, 0xCA));
    for (int i = 0; i < 100; ++i)
    {
        if (chars.Find(42) && chars.Find(200) && chars.Find(400)) break;
        std::this_thread::sleep_for(10ms);
    }
    EXPECT(chars.Find(42) && chars.Find(200) && chars.Find(400));

    auto with = [&](std::uint32_t id, auto fn) {
        auto c = chars.Find(id);
        EXPECT(c != nullptr);
        if (c) { std::lock_guard g(c->lock); fn(*c); }
    };
    chars.Rename(42, "Alice"); chars.Rename(200, "Bob");
    chars.Rename(400, "Carol");
    with(42,  [](tworldsvr::TChar& c) { c.level = 20; c.klass = 3;
                                        c.region = 11; });
    with(200, [](tworldsvr::TChar& c) { c.level = 25; c.klass = 4;
                                        c.region = 22; });

    auto entry_type = [&](std::uint32_t cid, std::uint32_t fid) -> int {
        auto c = chars.Find(cid);
        if (!c) return -1;
        std::lock_guard g(c->lock);
        for (const auto& f : c->friends) if (f.id == fid) return f.type;
        return -1;   // absent
    };
    auto poll = [&](auto pred) {
        for (int i = 0; i < 200; ++i)
        { if (pred()) return true; std::this_thread::sleep_for(5ms); }
        return pred();
    };

    // --- Scenario 1: Bob accepts Alice → mutual friends -------------
    SendFramed(p2, ToUint16(MessageId::MW_FRIENDREPLY_ACK),
        ReplyBody(200, 0xB0, "Alice", /*ASK_YES=*/0));
    {
        auto [w, b] = ReadFramed(p1);   // inviter Alice
        EXPECT(w == kAddReq);
        auto a = DecodeAdd(b);
        EXPECT(a.char_id == 42); EXPECT(a.result == frnd::kSuccess);
        EXPECT(a.friend_id == 200); EXPECT(a.name == "Bob");
        EXPECT(a.level == 25); EXPECT(a.klass == 4); EXPECT(a.region == 22);
    }
    {
        auto [w, b] = ReadFramed(p2);   // answerer Bob
        EXPECT(w == kAddReq);
        auto a = DecodeAdd(b);
        EXPECT(a.char_id == 200); EXPECT(a.result == frnd::kSuccess);
        EXPECT(a.friend_id == 42); EXPECT(a.name == "Alice");
        EXPECT(a.level == 20); EXPECT(a.region == 11);
    }
    EXPECT(poll([&] {
        return entry_type(42, 200) == frnd::kTypeFriendFriend &&
               entry_type(200, 42) == frnd::kTypeFriendFriend; }));

    // --- Scenario 2: Bob declines Carol → Carol gets the code -------
    SendFramed(p2, ToUint16(MessageId::MW_FRIENDREPLY_ACK),
        ReplyBody(200, 0xB0, "Carol", /*ASK_NO=*/1));
    {
        auto [w, b] = ReadFramed(p3);   // inviter Carol
        EXPECT(w == kAddReq);
        auto a = DecodeAdd(b);
        EXPECT(a.char_id == 400); EXPECT(a.result == 1);
        EXPECT(a.friend_id == 200);
    }

    // --- Scenario 3: inviter offline → answerer NOTFOUND ------------
    SendFramed(p2, ToUint16(MessageId::MW_FRIENDREPLY_ACK),
        ReplyBody(200, 0xB0, "Ghost", 0));
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == kAddReq);
        EXPECT(DecodeAdd(b).result == frnd::kNotFound);
    }

    // --- Scenario 4: Alice erases Bob (mutual) → demote -------------
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDERASE_ACK),
        EraseBody(42, 0xA1, 200));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kEraseReq);
        auto a = DecodeErase(b);
        EXPECT(a.char_id == 42); EXPECT(a.result == frnd::kSuccess);
        EXPECT(a.target == 200);
    }
    EXPECT(poll([&] {
        return entry_type(42, 200) == frnd::kTypeTarget &&
               entry_type(200, 42) == frnd::kTypeFriend; }));

    // --- Scenario 5: erase a non-friend → NOTFOUND ------------------
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDERASE_ACK),
        EraseBody(42, 0xA1, 999));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kEraseReq);
        EXPECT(DecodeErase(b).result == frnd::kNotFound);
    }

    // --- Scenario 6: Bob erases Alice (one-way) → removed both ------
    SendFramed(p2, ToUint16(MessageId::MW_FRIENDERASE_ACK),
        EraseBody(200, 0xB0, 42));
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == kEraseReq);
        EXPECT(DecodeErase(b).result == frnd::kSuccess);
    }
    EXPECT(poll([&] {
        return entry_type(200, 42) == -1 && entry_type(42, 200) == -1; }));

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_friend_reply_handlers "
                    "(6 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_friend_reply_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
