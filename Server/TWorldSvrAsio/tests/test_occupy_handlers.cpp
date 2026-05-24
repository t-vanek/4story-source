// W5-1 wire test: territory occupation broadcasts.
//
// CASTLE / LOCAL / MISSION occupy each fan the new owner+flag to
// every map peer. We register two peers and confirm both receive
// each broadcast with the right fields, including the LOCAL
// B-country display flip (a B-country guild's capture shows as the
// opposing flag and reports guild-less).

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
#include <memory>
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

    // A B-country guild, to exercise the LOCAL display flip.
    {
        auto g = std::make_shared<tworldsvr::TGuild>();
        g->id = 10; g->name = "Knights"; g->country = 2; // TCONTRY_B
        guilds.Insert(g);
    }

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

    // --- MISSION occupy → broadcast to both peers -----------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 1);    // type
        wire::WritePOD<std::uint16_t>(b, 50);  // local
        wire::WritePOD<std::uint8_t>(b, 1);    // country
        SendFramed(p1, ToUint16(MessageId::MW_MISSIONOCCUPY_ACK), b);
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_MISSIONOCCUPY_REQ));
        wire::Reader r(b);
        std::uint8_t type = 0, country = 0; std::uint16_t local = 0;
        r.Read(type); r.Read(local); r.Read(country);
        EXPECT(type == 1); EXPECT(local == 50); EXPECT(country == 1);
    }

    // --- CASTLE occupy → broadcast with the guild name ------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 1);    // type
        wire::WritePOD<std::uint16_t>(b, 7);   // castle
        wire::WritePOD<std::uint32_t>(b, 10);  // guild
        wire::WritePOD<std::uint8_t>(b, 1);    // country
        wire::WritePOD<std::uint32_t>(b, 0);   // lose_guild
        SendFramed(p1, ToUint16(MessageId::MW_CASTLEOCCUPY_ACK), b);
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CASTLEOCCUPY_REQ));
        wire::Reader r(b);
        std::uint8_t type = 0, country = 0; std::uint16_t castle = 0;
        std::uint32_t guild = 0; std::string name;
        r.Read(type); r.Read(castle); r.Read(guild); r.Read(country);
        r.ReadString(name);
        EXPECT(type == 1); EXPECT(castle == 7); EXPECT(guild == 10);
        EXPECT(country == 1); EXPECT(name == "Knights");
    }

    // --- LOCAL occupy by a B-country guild → flag flipped, guildless
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 1);    // type
        wire::WritePOD<std::uint16_t>(b, 60);  // local
        wire::WritePOD<std::uint8_t>(b, 0);    // country (will flip → 1)
        wire::WritePOD<std::uint32_t>(b, 10);  // guild (B-country → 0)
        wire::WritePOD<std::uint8_t>(b, 1);    // cur_country (C, != N)
        SendFramed(p1, ToUint16(MessageId::MW_LOCALOCCUPY_ACK), b);
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_LOCALOCCUPY_REQ));
        wire::Reader r(b);
        std::uint8_t type = 0, country = 0; std::uint16_t local = 0;
        std::uint32_t guild = 0; std::string name;
        r.Read(type); r.Read(local); r.Read(country); r.Read(guild);
        r.ReadString(name);
        EXPECT(type == 1); EXPECT(local == 60);
        EXPECT(country == 1);            // flipped from 0
        EXPECT(guild == 0);              // B-country → reported guild-less
        EXPECT(name == "Knights");       // name still carried (legacy)
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_occupy_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_occupy_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
