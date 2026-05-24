// W6-20 wire test: connection-completion sub-flow.
//
// Three map peers (svr 0x42 main, 0x43, 0x44). One char on a single
// initial con (0x42) drives the four ROUTE_ACK→ADDCONNECT→ENTERCHAR_ACK
// →CHARDATA_ACK transitions that close the W6-13/W6-18 reconcile loop:
//
//   * ROUTE_ACK count=0  → world asks the main for CHARDATA_REQ
//                          (no new cons needed; loop closes via the
//                          subsequent CHARDATA_ACK).
//   * ROUTE_ACK count=1  → world registers the new (ip/port/svr) as a
//                          pending TCharCon (valid=false, ready=false)
//                          and forwards ADDCONNECT_REQ to the reporter.
//   * ENTERCHAR_ACK      → world flips that con's ready bit; once every
//                          con is ready, fires the CHECKMAIN sweep.
//   * CHARDATA_ACK       → world refreshes the char's level + HP/MP,
//                          then fires CHECKMAIN if every con is ready.
//   * error              → ROUTE_ACK for an unknown char replies
//                          DELCHAR(logout=1,save=0) on the reporter.

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

// MW_ROUTE_ACK: DWORD char_id, key, BYTE count, × (DWORD ip, WORD port,
// BYTE server_id).
struct RouteEntry
{
    std::uint32_t ip;
    std::uint16_t port;
    std::uint8_t  server_id;
};

std::vector<std::byte> RouteBody(std::uint32_t char_id, std::uint32_t key,
                                 const std::vector<RouteEntry>& entries)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint8_t>(
        b, static_cast<std::uint8_t>(entries.size()));
    for (const auto& e : entries)
    {
        tworldsvr::wire::WritePOD(b, e.ip);
        tworldsvr::wire::WritePOD(b, e.port);
        tworldsvr::wire::WritePOD(b, e.server_id);
    }
    return b;
}

std::vector<std::byte> EnterCharBody(std::uint32_t char_id, std::uint32_t key)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    return b;
}

// MW_CHARDATA_ACK: DWORD char_id, key, BYTE start_act, level, DWORD
// max_hp, hp, max_mp, mp, BYTE country, mode (+ opaque tail; not used).
std::vector<std::byte> CharDataBody(std::uint32_t char_id, std::uint32_t key,
                                    std::uint8_t level,
                                    std::uint32_t max_hp, std::uint32_t hp,
                                    std::uint32_t max_mp, std::uint32_t mp)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, /*start_act=*/0);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, level);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, max_hp);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, hp);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, max_mp);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, mp);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, /*country=*/1);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, /*mode=*/0);
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

    // Three map peers — p1=0x42 (main), p2=0x43, p3=0x44. Drain the
    // RELAYCONNECT fan-out each registration causes on the already-
    // joined peers.
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

    // Char A (100) on p1's connection (main=0x42).
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK),
               AddCharBody(/*char_id=*/100, /*key=*/0xA1));
    for (int i = 0; i < 200 && !chars.Find(100); ++i)
        std::this_thread::sleep_for(10ms);
    {
        auto a = chars.Find(100);
        EXPECT(a != nullptr);
        if (a)
        {
            std::lock_guard g(a->lock);
            EXPECT(a->main_server_id == 0x42);
            EXPECT(a->cons.size() == 1);
            EXPECT(a->cons.front().server_id == 0x42);
            EXPECT(a->cons.front().ready == false);
        }
    }

    // --- Test A: ROUTE_ACK count=0 → CHARDATA_REQ -------------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_ROUTE_ACK),
                   RouteBody(100, 0xA1, {}));
        auto [w, got] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CHARDATA_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0;
        r.Read(cid); r.Read(key);
        EXPECT(cid == 100);
        EXPECT(key == 0xA1);
    }

    // --- Test B: ROUTE_ACK count=1 (new server 0x44) → ADDCONNECT --
    {
        const std::uint32_t client_ip   = 0x0A000001;   // 10.0.0.1
        const std::uint16_t client_port = 33501;
        SendFramed(p1, ToUint16(MessageId::MW_ROUTE_ACK),
                   RouteBody(100, 0xA1,
                       {{client_ip, client_port, /*server_id=*/0x44}}));
        auto [w, got] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_ADDCONNECT_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0; std::uint8_t cnt = 0;
        std::uint32_t ip = 0; std::uint16_t pp = 0; std::uint8_t sid = 0;
        r.Read(cid); r.Read(key); r.Read(cnt);
        r.Read(ip); r.Read(pp); r.Read(sid);
        EXPECT(cid == 100);
        EXPECT(key == 0xA1);
        EXPECT(cnt == 1);
        EXPECT(ip == client_ip);
        EXPECT(pp == client_port);
        EXPECT(sid == 0x44);

        // Char A's cons now carries the pending 0x44 con.
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        {
            auto a = chars.Find(100);
            std::lock_guard g(a->lock);
            auto it = std::find_if(a->cons.begin(), a->cons.end(),
                [](const tworldsvr::TCharCon& c) {
                    return c.server_id == 0x44;
                });
            if (it != a->cons.end() && !it->ready && !it->valid &&
                it->ip_addr == client_ip && it->port == client_port)
            {
                ok = true;
                break;
            }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(ok);
    }

    // --- Test C: ENTERCHAR_ACK from p1 (0x42) — partial, no broadcast
    {
        SendFramed(p1, ToUint16(MessageId::MW_ENTERCHAR_ACK),
                   EnterCharBody(100, 0xA1));
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        {
            auto a = chars.Find(100);
            std::lock_guard g(a->lock);
            auto it = std::find_if(a->cons.begin(), a->cons.end(),
                [](const tworldsvr::TCharCon& c) {
                    return c.server_id == 0x42;
                });
            if (it != a->cons.end() && it->ready) { ok = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(ok);
        // 0x44 stays not-ready; no broadcast yet.
        {
            auto a = chars.Find(100);
            std::lock_guard g(a->lock);
            auto it = std::find_if(a->cons.begin(), a->cons.end(),
                [](const tworldsvr::TCharCon& c) {
                    return c.server_id == 0x44;
                });
            EXPECT(it != a->cons.end());
            if (it != a->cons.end()) EXPECT(it->ready == false);
        }
    }

    // --- Test D: ENTERCHAR_ACK from p3 (0x44) — all ready → CHECKMAIN
    {
        // Set the char's channel/map/pos so the CHECKMAIN broadcast
        // carries something specific we can verify.
        {
            auto a = chars.Find(100);
            std::lock_guard g(a->lock);
            a->channel = 3; a->map_id = 770;
            a->pos_x = 11.0f; a->pos_y = 22.0f; a->pos_z = 33.0f;
        }
        SendFramed(p3, ToUint16(MessageId::MW_ENTERCHAR_ACK),
                   EnterCharBody(100, 0xA1));
        // The broadcast hits both cons (0x42 → p1, 0x44 → p3). Order
        // matches con-insertion order (0x42 first, then 0x44).
        auto [w1, got1] = ReadFramed(p1);
        EXPECT(w1 == ToUint16(MessageId::MW_CHECKMAIN_REQ));
        {
            wire::Reader r(got1);
            std::uint32_t cid = 0, key = 0; std::uint8_t ch = 0;
            std::uint16_t map_id = 0; float x = 0, y = 0, z = 0;
            r.Read(cid); r.Read(key); r.Read(ch); r.Read(map_id);
            r.Read(x); r.Read(y); r.Read(z);
            EXPECT(cid == 100);
            EXPECT(key == 0xA1);
            EXPECT(ch == 3);
            EXPECT(map_id == 770);
            EXPECT(x == 11.0f);
            EXPECT(y == 22.0f);
            EXPECT(z == 33.0f);
        }
        auto [w3, _] = ReadFramed(p3);
        EXPECT(w3 == ToUint16(MessageId::MW_CHECKMAIN_REQ));
    }

    // --- Test E: CHARDATA_ACK with every con ready → CHECKMAIN sweep
    //             + level/HP/MP refresh.
    {
        SendFramed(p1, ToUint16(MessageId::MW_CHARDATA_ACK),
                   CharDataBody(100, 0xA1, /*level=*/42,
                       /*max_hp=*/1000, /*hp=*/950,
                       /*max_mp=*/500,  /*mp=*/480));
        auto [w1, _1] = ReadFramed(p1);
        EXPECT(w1 == ToUint16(MessageId::MW_CHECKMAIN_REQ));
        auto [w3, _3] = ReadFramed(p3);
        EXPECT(w3 == ToUint16(MessageId::MW_CHECKMAIN_REQ));

        bool ok = false;
        for (int i = 0; i < 200; ++i)
        {
            auto a = chars.Find(100);
            std::lock_guard g(a->lock);
            if (a->level == 42 && a->max_hp == 1000 && a->hp == 950 &&
                a->max_mp == 500 && a->mp == 480) { ok = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(ok);
    }

    // --- Test F: ROUTE_ACK for an unknown char → DELCHAR ------------
    {
        SendFramed(p2, ToUint16(MessageId::MW_ROUTE_ACK),
                   RouteBody(/*char_id=*/999, /*key=*/0xCC, {}));
        auto [w, got] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_DELCHAR_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0;
        std::uint8_t logout = 0, save = 1;
        r.Read(cid); r.Read(key); r.Read(logout); r.Read(save);
        EXPECT(cid == 999);
        EXPECT(logout == 1);
        EXPECT(save == 0);
    }

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_route_completion_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_route_completion_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
