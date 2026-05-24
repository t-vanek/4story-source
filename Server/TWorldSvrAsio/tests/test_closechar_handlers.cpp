// W6-19 wire test: CloseChar full teardown (MW_CLOSECHAR_ACK).
//
// Three map peers (svr 0x42 main / 0x43 / 0x44). Closing a char:
//   * DELCHARs every connection — dead cons first, then live — with the
//     logout/save flags set only on the main connection.
//   * if a main-session handoff was in flight (chg_main_id), voids it on
//     the would-be new main first (INVALIDCHAR, release_main=1).
//   * a CLOSECHAR for an unknown / wrong-key char replies DELCHAR.

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

std::vector<std::byte> CharKeyBody(std::uint32_t char_id, std::uint32_t key)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    return b;
}

// Read a DELCHAR and check its char id + logout/save flags.
void ExpectDelChar(boost::asio::ip::tcp::socket& s, std::uint32_t exp_id,
                   std::uint8_t exp_logout, std::uint8_t exp_save)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    auto [w, got] = ReadFramed(s);
    EXPECT(w == ToUint16(MessageId::MW_DELCHAR_REQ));
    tworldsvr::wire::Reader r(got);
    std::uint32_t cid = 0, key = 0; std::uint8_t logout = 0xFF, save = 0xFF;
    r.Read(cid); r.Read(key); r.Read(logout); r.Read(save);
    EXPECT(cid == exp_id);
    EXPECT(logout == exp_logout);
    EXPECT(save == exp_save);
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
    tcp::socket p1(client_io), p2(client_io), p3(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep); p2.connect(ep); p3.connect(ep);
    std::this_thread::sleep_for(20ms);

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0043));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }
    SendFramed(p3, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0044));
    { auto [w, _] = ReadFramed(p3);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    auto cons_size = [&](std::uint32_t id) -> std::size_t {
        auto c = chars.Find(id);
        if (!c) return 0;
        std::lock_guard g(c->lock);
        return c->cons.size();
    };

    // Char 100: main 0x42 + con 0x43, a dead con on 0x44, logout+save set.
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(100, 0xA1));
    for (int i = 0; i < 200 && !chars.Find(100); ++i)
        std::this_thread::sleep_for(10ms);
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(100, 0xA1));
    for (int i = 0; i < 200 && cons_size(100) != 2; ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(cons_size(100) == 2);
    if (auto a = chars.Find(100))
    {
        std::lock_guard g(a->lock);
        a->dead_cons.push_back(0x44);
        a->logout = true; a->saving = true;
    }

    // Char 200: main 0x42 only, with a handoff in flight (chg_main_id=0x43).
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(200, 0xB0));
    for (int i = 0; i < 200 && cons_size(200) != 1; ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(cons_size(200) == 1);
    if (auto b = chars.Find(200))
    { std::lock_guard g(b->lock); b->chg_main_id = 0x43; }

    // --- close char 100: DELCHAR fan-out, main-only flags -----------
    {
        SendFramed(p1, ToUint16(MessageId::MW_CLOSECHAR_ACK),
                   CharKeyBody(100, 0xA1));
        ExpectDelChar(p3, 100, /*logout=*/0, /*save=*/0);  // dead con 0x44
        ExpectDelChar(p1, 100, /*logout=*/1, /*save=*/1);  // main 0x42
        ExpectDelChar(p2, 100, /*logout=*/0, /*save=*/0);  // con 0x43
        bool gone = false;
        for (int i = 0; i < 200; ++i)
        { if (!chars.Find(100)) { gone = true; break; }
          std::this_thread::sleep_for(10ms); }
        EXPECT(gone);
    }

    // --- close char 200: INVALIDCHAR the would-be new main first ----
    {
        SendFramed(p1, ToUint16(MessageId::MW_CLOSECHAR_ACK),
                   CharKeyBody(200, 0xB0));
        // chg_main_id = 0x43 (p2) gets INVALIDCHAR(release_main=1).
        auto [w, got] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_INVALIDCHAR_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0; std::uint8_t release_main = 0;
        r.Read(cid); r.Read(key); r.Read(release_main);
        EXPECT(cid == 200); EXPECT(release_main == 1);
        // main 0x42 (p1) gets DELCHAR (logout/save 0 — char not flagged).
        ExpectDelChar(p1, 200, /*logout=*/0, /*save=*/0);
        bool gone = false;
        for (int i = 0; i < 200; ++i)
        { if (!chars.Find(200)) { gone = true; break; }
          std::this_thread::sleep_for(10ms); }
        EXPECT(gone);
    }

    // --- unknown char → DELCHAR to the reporting map ----------------
    {
        SendFramed(p3, ToUint16(MessageId::MW_CLOSECHAR_ACK),
                   CharKeyBody(999, 0xCC));
        ExpectDelChar(p3, 999, /*logout=*/1, /*save=*/0);
    }

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_closechar_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_closechar_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
