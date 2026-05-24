// W4-1 wire test: MW_FRIENDASK_ACK friend-invite gate over a
// three-peer loopback session.
//
// Alice 42/peer1 0x42, Bob 200/peer2 0x43, Carol 400/peer3 0x44.
//
// Scenarios (replies land on the requester's peer1; the relay on
// the target's peer2):
//   1. Ask an unknown name        → FRIEND_NOTFOUND
//   2. Ask Bob (clean)            → MW_FRIENDASK_REQ forwarded to Bob
//   3. Ask an existing friend     → FRIEND_ALREADY
//   4. Both hold a pending FT_TARGET → instant mutual friend
//      (FRIEND_SUCCESS + both entries upgraded + regions swapped)
//   5. Ask a different-country char → FRIEND_NOTFOUND

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

std::vector<std::byte> AskBody(std::uint32_t char_id, std::uint32_t key,
                                const std::string& target)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, char_id);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    tworldsvr::wire::WriteString(b, target);
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

struct AskReq { std::uint32_t char_id = 0, key = 0, inviter_id = 0;
                std::string inviter_name; };
AskReq DecodeAsk(const std::vector<std::byte>& b)
{
    AskReq a{};
    tworldsvr::wire::Reader r(b);
    r.Read(a.char_id); r.Read(a.key); r.ReadString(a.inviter_name);
    r.Read(a.inviter_id);
    return a;
}

const std::uint16_t kAddReq =
    tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_FRIENDADD_REQ);
const std::uint16_t kAskReq =
    tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_FRIENDASK_REQ);

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
    with(42,  [](tworldsvr::TChar& c) { c.country = 0; c.region = 11; });
    with(200, [](tworldsvr::TChar& c) { c.country = 0; c.level = 20;
                                        c.klass = 3; });
    with(400, [](tworldsvr::TChar& c) { c.country = 0; c.level = 30;
                                        c.klass = 4; c.region = 22; });

    // --- Scenario 1: unknown target → NOTFOUND ----------------------
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDASK_ACK),
        AskBody(42, 0xA1, "Ghost"));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kAddReq);
        auto a = DecodeAdd(b);
        EXPECT(a.char_id == 42); EXPECT(a.result == frnd::kNotFound);
        EXPECT(a.name == "Ghost");
    }

    // --- Scenario 2: clean ask Bob → FRIENDASK_REQ to Bob's map -----
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDASK_ACK),
        AskBody(42, 0xA1, "Bob"));
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == kAskReq);
        auto a = DecodeAsk(b);
        EXPECT(a.char_id == 200); EXPECT(a.key == 0xB0);
        EXPECT(a.inviter_name == "Alice"); EXPECT(a.inviter_id == 42);
    }

    // --- Scenario 3: already a friend → ALREADY ---------------------
    with(42, [](tworldsvr::TChar& c) {
        c.friends.push_back({200, "Bob", frnd::kTypeFriendFriend, true, 0, 0});
    });
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDASK_ACK),
        AskBody(42, 0xA1, "Bob"));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kAddReq);
        EXPECT(DecodeAdd(b).result == frnd::kAlready);
    }

    // --- Scenario 4: mutual pending FT_TARGET → instant friend ------
    with(42,  [](tworldsvr::TChar& c) {
        c.friends.push_back({400, "Carol", frnd::kTypeTarget, false, 0, 0});
    });
    with(400, [](tworldsvr::TChar& c) {
        c.friends.push_back({42, "Alice", frnd::kTypeTarget, false, 0, 0});
    });
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDASK_ACK),
        AskBody(42, 0xA1, "Carol"));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kAddReq);
        auto a = DecodeAdd(b);
        EXPECT(a.result == frnd::kSuccess);
        EXPECT(a.friend_id == 400); EXPECT(a.name == "Carol");
        EXPECT(a.level == 30); EXPECT(a.klass == 4);
        EXPECT(a.region == 22);          // Carol's region
    }
    // Both entries upgraded + regions swapped in.
    with(42, [](tworldsvr::TChar& c) {
        for (const auto& f : c.friends) if (f.id == 400) {
            EXPECT(f.type == frnd::kTypeFriendFriend);
            EXPECT(f.connected); EXPECT(f.region == 22); }
    });
    with(400, [](tworldsvr::TChar& c) {
        for (const auto& f : c.friends) if (f.id == 42) {
            EXPECT(f.type == frnd::kTypeFriendFriend);
            EXPECT(f.connected); EXPECT(f.region == 11); }
    });

    // --- Scenario 5: different country → NOTFOUND (before ALREADY) --
    with(200, [](tworldsvr::TChar& c) { c.country = 1; });
    SendFramed(p1, ToUint16(MessageId::MW_FRIENDASK_ACK),
        AskBody(42, 0xA1, "Bob"));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kAddReq);
        EXPECT(DecodeAdd(b).result == frnd::kNotFound);
    }

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_friend_handlers (5 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_friend_handlers (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
