// W4-9 wire test: MW_LEVELUP_ACK — level store + soulmate sync /
// auto-dissolve + multi-connection broadcast.
//
// Alice 42/peer1 0x42 (soulmate of Bob 200/peer2 0x43); Carol
// 400/peer3 0x44 with a second connection on server 0x43.
//
// Scenarios:
//   1. Alice +5 levels (gap ≤ window) → level stored, Bob's
//      soulmate view updated, pairing intact.
//   2. Alice jumps to 35 (gap > window) → soulmate auto-dissolves
//      both sides.
//   3. Carol levels up → MW_LEVELUP_REQ fanned to her non-main
//      connection's map (peer2).

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

std::vector<std::byte> LevelBody(std::uint32_t cid, std::uint32_t key,
                                  std::uint8_t level)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, cid);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, level);
    return b;
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
    { if (chars.Find(42) && chars.Find(200) && chars.Find(400)) break;
      std::this_thread::sleep_for(10ms); }
    EXPECT(chars.Find(42) && chars.Find(200) && chars.Find(400));

    auto with = [&](std::uint32_t id, auto fn) {
        auto c = chars.Find(id);
        EXPECT(c != nullptr);
        if (c) { std::lock_guard g(c->lock); fn(*c); }
    };
    using S = tworldsvr::TSoulmate;
    with(42,  [](tworldsvr::TChar& c) { c.level = 20;
        c.soulmate = S{200, "Bob", 20, 0, true, 0}; });
    with(200, [](tworldsvr::TChar& c) { c.level = 20;
        c.soulmate = S{42, "Alice", 20, 0, true, 0}; });
    // Carol: a second connection on server 0x43 (peer2).
    with(400, [](tworldsvr::TChar& c) {
        tworldsvr::TCharCon con; con.server_id = 0x43; con.valid = true;
        c.cons.push_back(con); });

    auto level_of = [&](std::uint32_t id) -> int {
        auto c = chars.Find(id); std::lock_guard g(c->lock); return c->level; };
    auto sm_target = [&](std::uint32_t id) -> std::uint32_t {
        auto c = chars.Find(id); std::lock_guard g(c->lock);
        return c->soulmate.target; };
    auto sm_level = [&](std::uint32_t id) -> int {
        auto c = chars.Find(id); std::lock_guard g(c->lock);
        return c->soulmate.level; };
    auto poll = [&](auto pred) {
        for (int i = 0; i < 200; ++i)
        { if (pred()) return true; std::this_thread::sleep_for(5ms); }
        return pred();
    };

    // --- Scenario 1: +5 levels, gap ok → sync, pairing intact -------
    SendFramed(p1, ToUint16(MessageId::MW_LEVELUP_ACK), LevelBody(42, 0xA1, 25));
    EXPECT(poll([&] { return level_of(42) == 25; }));
    EXPECT(poll([&] { return sm_level(200) == 25; }));   // Bob's view updated
    EXPECT(sm_target(42) == 200 && sm_target(200) == 42); // intact

    // --- Scenario 2: jump to 35 (gap > window) → auto-dissolve ------
    SendFramed(p1, ToUint16(MessageId::MW_LEVELUP_ACK), LevelBody(42, 0xA1, 35));
    EXPECT(poll([&] { return level_of(42) == 35; }));
    EXPECT(poll([&] { return sm_target(42) == 0 && sm_target(200) == 0; }));

    // --- Scenario 3: Carol level-up → fanned to her 0x43 connection -
    SendFramed(p3, ToUint16(MessageId::MW_LEVELUP_ACK), LevelBody(400, 0xCA, 40));
    {
        auto [w, b] = ReadFramed(p2);   // Carol's non-main connection
        EXPECT(w == ToUint16(MessageId::MW_LEVELUP_REQ));
        tworldsvr::wire::Reader r(b);
        std::uint32_t cid = 0, key = 0; std::uint8_t lvl = 0;
        r.Read(cid); r.Read(key); r.Read(lvl);
        EXPECT(cid == 400); EXPECT(lvl == 40);
    }
    EXPECT(poll([&] { return level_of(400) == 40; }));

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_levelup_handlers (3 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_levelup_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
