// W6-13 wire test: connection-list reconcile.
//
// Three map peers (svr 0x42/0x43/0x44). A char connected to 0x42+0x43
// drives the two reconcile outcomes:
//   * CONLIST reporting a *new* server (0x44) → world drops the
//     no-longer-reported 0x43 to dead_cons and asks the main map
//     (0x42) to ROUTELIST the char to 0x44.
//   * CONLIST reporting only existing servers → world re-confirms the
//     main session on every remaining connection (CHECKMAIN broadcast).
// A CONLIST for an unknown char → DELCHAR back to the reporting map.

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

#include <algorithm>
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

std::vector<std::byte> ConListBody(std::uint32_t char_id, std::uint32_t key,
                                   const std::vector<std::uint8_t>& servers)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint8_t>(
        b, static_cast<std::uint8_t>(servers.size()));
    for (std::uint8_t s : servers)
        tworldsvr::wire::WritePOD<std::uint8_t>(b, s);
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
    tcp::socket p1(client_io), p2(client_io), p3(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep); p2.connect(ep); p3.connect(ep);
    std::this_thread::sleep_for(20ms);

    // Register three map peers; drain the RELAYCONNECT fan-out the
    // world emits to each already-registered peer as the next joins.
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

    // Char A (100) and Char B (200), each connected to 0x42 + 0x43.
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(100, 0xA1));
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(100, 0xA1));
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(200, 0xB0));
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(200, 0xB0));
    auto cons_size = [&](std::uint32_t id) -> std::size_t {
        auto c = chars.Find(id);
        if (!c) return 0;
        std::lock_guard g(c->lock);
        return c->cons.size();
    };
    for (int i = 0; i < 200; ++i)
    { if (cons_size(100) == 2 && cons_size(200) == 2) break;
      std::this_thread::sleep_for(10ms); }
    EXPECT(cons_size(100) == 2);
    EXPECT(cons_size(200) == 2);

    // Give Char B a meaningful main-session position for CHECKMAIN.
    if (auto b = chars.Find(200))
    {
        std::lock_guard g(b->lock);
        b->channel = 2; b->map_id = 500;
        b->pos_x = 10.0f; b->pos_y = 20.0f; b->pos_z = 30.0f;
    }

    // --- ROUTELIST branch: CONLIST reports a new server 0x44 -------
    {
        SendFramed(p1, ToUint16(MessageId::MW_CONLIST_ACK),
                   ConListBody(100, 0xA1, {0x44}));
        auto [w, got] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_ROUTELIST_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0; std::uint8_t cnt = 0, sid = 0;
        r.Read(cid); r.Read(key); r.Read(cnt); r.Read(sid);
        EXPECT(cid == 100); EXPECT(key == 0xA1);
        EXPECT(cnt == 1);   EXPECT(sid == 0x44);

        // 0x43 was dropped to dead_cons; only 0x42 remains a con.
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        {
            auto c = chars.Find(100);
            std::lock_guard g(c->lock);
            const bool live_ok = c->cons.size() == 1
                && c->cons.front().server_id == 0x42;
            const bool dead_ok = std::find(c->dead_cons.begin(),
                c->dead_cons.end(), 0x43) != c->dead_cons.end();
            if (live_ok && dead_ok) { ok = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(ok);
    }

    // --- CHECKMAIN branch: CONLIST reports only an existing con ----
    {
        SendFramed(p1, ToUint16(MessageId::MW_CONLIST_ACK),
                   ConListBody(200, 0xB0, {0x43}));
        // Main (0x42 = p1) then 0x43 (p2) each get CHECKMAIN.
        auto [w1, got1] = ReadFramed(p1);
        EXPECT(w1 == ToUint16(MessageId::MW_CHECKMAIN_REQ));
        wire::Reader r(got1);
        std::uint32_t cid = 0, key = 0; std::uint8_t channel = 0;
        std::uint16_t map_id = 0; float x = 0, y = 0, z = 0;
        r.Read(cid); r.Read(key); r.Read(channel); r.Read(map_id);
        r.Read(x); r.Read(y); r.Read(z);
        EXPECT(cid == 200); EXPECT(key == 0xB0);
        EXPECT(channel == 2); EXPECT(map_id == 500);
        EXPECT(x == 10.0f); EXPECT(z == 30.0f);

        auto [w2, _] = ReadFramed(p2);
        EXPECT(w2 == ToUint16(MessageId::MW_CHECKMAIN_REQ));
    }

    // --- error branch: CONLIST for an unknown char → DELCHAR -------
    {
        SendFramed(p3, ToUint16(MessageId::MW_CONLIST_ACK),
                   ConListBody(999, 0xCC, {}));
        auto [w, got] = ReadFramed(p3);
        EXPECT(w == ToUint16(MessageId::MW_DELCHAR_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0; std::uint8_t logout = 0, save = 1;
        r.Read(cid); r.Read(key); r.Read(logout); r.Read(save);
        EXPECT(cid == 999); EXPECT(logout == 1); EXPECT(save == 0);
    }

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_conn_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_conn_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
