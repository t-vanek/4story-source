// W6-2 wire test: combat / taming cross-server relays.
//
// Alice (42) is on peer1. A combat/taming effect resolved on peer2
// (where the monster/target lives) names Alice as the attacker; world
// routes the result back to Alice's map (peer1):
//   MAGICMIRROR (forwarded verbatim) / MONTEMPT / MONTEMPTEVO.

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
    svr_cfg.port = 0; svr_cfg.max_connections = 4; svr_cfg.ctx = ctx;
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

    const std::uint32_t kKey = 0xA1;
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, kKey));
    for (int i = 0; i < 100; ++i)
    { if (chars.Find(42)) break; std::this_thread::sleep_for(10ms); }
    EXPECT(chars.Find(42));

    // --- MAGICMIRROR: routed verbatim to Alice's map (p1) ---------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 999);  // host
        wire::WritePOD<std::uint32_t>(b, 42);   // attack (Alice)
        wire::WritePOD<std::uint32_t>(b, 888);  // target
        wire::WritePOD<std::uint8_t>(b, 0);     // attack_type
        wire::WritePOD<std::uint8_t>(b, 1);     // target_type
        SendFramed(p2, ToUint16(MessageId::MW_MAGICMIRROR_ACK), b);
        auto [w, got] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_MAGICMIRROR_REQ));
        EXPECT(got == b);   // forwarded verbatim
    }

    // --- MONTEMPT: routed to Alice's map --------------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 42);     // attacker
        wire::WritePOD<std::uint16_t>(b, 0x1234); // mon id
        SendFramed(p2, ToUint16(MessageId::MW_MONTEMPT_ACK), b);
        auto [w, got] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_MONTEMPT_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0; std::uint16_t mon = 0;
        r.Read(cid); r.Read(key); r.Read(mon);
        EXPECT(cid == 42); EXPECT(key == kKey); EXPECT(mon == 0x1234);
    }

    // --- MONTEMPTEVO: routed to Alice's map -----------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 42);   // attacker
        wire::WritePOD<std::uint32_t>(b, 777);  // host
        wire::WritePOD<std::uint8_t>(b, 2);     // host type
        SendFramed(p2, ToUint16(MessageId::MW_MONTEMPTEVO_ACK), b);
        auto [w, got] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_MONTEMPTEVO_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0, host = 0; std::uint8_t htype = 0;
        r.Read(cid); r.Read(key); r.Read(host); r.Read(htype);
        EXPECT(cid == 42); EXPECT(key == kKey);
        EXPECT(host == 777); EXPECT(htype == 2);
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_combat_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_combat_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
