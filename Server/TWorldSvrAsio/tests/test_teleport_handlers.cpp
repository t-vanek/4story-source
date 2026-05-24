// W6-17 wire test: teleport begin + cession queue.
//
// Two map peers (svr 0x42 main / 0x43). A char connected to both:
//   * same-channel BEGINTELEPORT → just records the channel, no fan-out.
//   * a fresh BEGINTELEPORT → STARTTELEPORT broadcast to every con.
//   * a second BEGINTELEPORT while the first is in flight → deferred on
//     the cession queue; it replays (broadcasts) when CHECKMAIN_ACK
//     confirms the main session and pops the first entry.

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

// MW_BEGINTELEPORT_ACK body (SSHandler.cpp:8554 + OnBeginTeleport).
std::vector<std::byte> BeginTeleportBody(std::uint32_t char_id,
                                         std::uint32_t key,
                                         std::uint8_t same_channel,
                                         std::uint8_t channel,
                                         std::uint16_t map_id,
                                         float px, float py, float pz)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, same_channel);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, channel);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, map_id);
    tworldsvr::wire::WritePOD<float>(b, px);
    tworldsvr::wire::WritePOD<float>(b, py);
    tworldsvr::wire::WritePOD<float>(b, pz);
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

    auto cons_size = [&](std::uint32_t id) -> std::size_t {
        auto c = chars.Find(id);
        if (!c) return 0;
        std::lock_guard g(c->lock);
        return c->cons.size();
    };
    // Char 100, main 0x42, con on 0x43 (serialise the inserts).
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(100, 0xA1));
    for (int i = 0; i < 200 && !chars.Find(100); ++i)
        std::this_thread::sleep_for(10ms);
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(100, 0xA1));
    for (int i = 0; i < 200 && cons_size(100) != 2; ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(cons_size(100) == 2);

    auto read_startteleport = [](tcp::socket& s, std::uint8_t exp_ch,
                                 std::uint16_t exp_map) {
        auto [w, got] = ReadFramed(s);
        EXPECT(w == ToUint16(MessageId::MW_STARTTELEPORT_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0; std::uint8_t channel = 0;
        std::uint16_t map_id = 0; float x = 0, y = 0, z = 0;
        r.Read(cid); r.Read(key); r.Read(channel); r.Read(map_id);
        r.Read(x); r.Read(y); r.Read(z);
        EXPECT(cid == 100); EXPECT(key == 0xA1);
        EXPECT(channel == exp_ch); EXPECT(map_id == exp_map);
    };
    auto cess_size = [&]() -> std::size_t {
        auto c = chars.Find(100);
        std::lock_guard g(c->lock);
        return c->con_cess.size();
    };
    auto cur_channel = [&]() -> std::uint8_t {
        auto c = chars.Find(100);
        std::lock_guard g(c->lock);
        return c->channel;
    };

    // --- same-channel: records channel, no fan-out ------------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_BEGINTELEPORT_ACK),
                   BeginTeleportBody(100, 0xA1, /*same=*/1, /*channel=*/7,
                                     0, 0, 0, 0));
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        { if (cur_channel() == 7) { ok = true; break; }
          std::this_thread::sleep_for(10ms); }
        EXPECT(ok);
        EXPECT(cess_size() == 0);   // same-channel never queues
    }

    // --- teleport A: broadcast STARTTELEPORT to both cons -----------
    {
        SendFramed(p1, ToUint16(MessageId::MW_BEGINTELEPORT_ACK),
                   BeginTeleportBody(100, 0xA1, /*same=*/0, /*channel=*/2,
                                     /*map=*/300, 1.0f, 2.0f, 3.0f));
        read_startteleport(p1, 2, 300);
        read_startteleport(p2, 2, 300);
    }

    // --- teleport B while A in flight: deferred on the queue --------
    {
        SendFramed(p1, ToUint16(MessageId::MW_BEGINTELEPORT_ACK),
                   BeginTeleportBody(100, 0xA1, /*same=*/0, /*channel=*/3,
                                     /*map=*/400, 4.0f, 5.0f, 6.0f));
        bool deferred = false;
        for (int i = 0; i < 200; ++i)
        { if (cess_size() == 2) { deferred = true; break; }
          std::this_thread::sleep_for(10ms); }
        EXPECT(deferred);   // A (in flight) + B (waiting)
    }

    // --- CHECKMAIN confirms main → pops A, replays B ----------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_CHECKMAIN_ACK),
                   CharKeyBody(100, 0xA1));
        // Main (p1) gets CONRESULT first, then B's STARTTELEPORT.
        auto [w, _] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CONRESULT_REQ));
        read_startteleport(p1, 3, 400);
        read_startteleport(p2, 3, 400);

        // A popped; B now the in-flight front.
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        { if (cess_size() == 1) { ok = true; break; }
          std::this_thread::sleep_for(10ms); }
        EXPECT(ok);
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_teleport_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_teleport_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
