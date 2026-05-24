// W5-3 wire test: CASTLEOCCUPY resets castle applications.
//
// Guild Knights (id 10) has member Bob (200) applied to castle 7.
// When castle 7 is occupied, world clears Bob's application (telling
// his map via CASTLEAPPLY_REQ with castle=0) and still broadcasts the
// occupation to every peer.

#include "../handlers/handlers.h"
#include "../services/castle_constants.h"
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

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    namespace wire = tworldsvr::wire;
    namespace castle = tworldsvr::castle;

    boost::asio::io_context io;
    tworldsvr::CharRegistry   chars;
    tworldsvr::GuildRegistry  guilds;
    tworldsvr::PeerRegistry   peers;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.nation = 0;

    // Knights with Bob (200) applied to castle 7.
    {
        auto g = std::make_shared<tworldsvr::TGuild>();
        g->id = 10; g->name = "Knights"; g->chief_char_id = 42;
        tworldsvr::TGuildMember bob;
        bob.char_id = 200; bob.guild_id = 10; bob.castle = 7; bob.camp = 1;
        g->members.push_back(bob);
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

    // Bob online on p2 (so his reset routes there).
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(200, 0xB0));
    for (int i = 0; i < 100; ++i)
    { if (chars.Find(200)) break; std::this_thread::sleep_for(10ms); }
    EXPECT(chars.Find(200));

    // Castle 7 occupied by Knights → reset Bob + broadcast.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 1);    // type
        wire::WritePOD<std::uint16_t>(b, 7);   // castle
        wire::WritePOD<std::uint32_t>(b, 10);  // guild
        wire::WritePOD<std::uint8_t>(b, 1);    // country
        wire::WritePOD<std::uint32_t>(b, 0);   // lose_guild
        SendFramed(p1, ToUint16(MessageId::MW_CASTLEOCCUPY_ACK), b);
    }

    // p2 first sees Bob's application reset, then the occupy broadcast.
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_CASTLEAPPLY_REQ));
        wire::Reader r(b);
        std::uint32_t cid = 0, key = 0, target = 0;
        std::uint16_t cas = 0; std::uint8_t result = 0, camp = 0;
        r.Read(cid); r.Read(key); r.Read(result); r.Read(cas);
        r.Read(target); r.Read(camp);
        EXPECT(cid == 200); EXPECT(result == castle::kSuccess);
        EXPECT(cas == 0); EXPECT(target == 200); EXPECT(camp == 0);
    }
    {
        auto [w, _] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CASTLEOCCUPY_REQ));
    }
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_CASTLEOCCUPY_REQ));
        wire::Reader r(b);
        std::uint8_t type = 0, country = 0; std::uint16_t cas = 0;
        std::uint32_t guild = 0; std::string name;
        r.Read(type); r.Read(cas); r.Read(guild); r.Read(country);
        r.ReadString(name);
        EXPECT(cas == 7); EXPECT(guild == 10); EXPECT(name == "Knights");
    }

    // Bob's application is cleared in the registry.
    {
        auto g = guilds.Find(10);
        if (g)
        {
            std::lock_guard lk(g->lock);
            auto* m = g->FindMember(200);
            EXPECT(m != nullptr);
            if (m) { EXPECT(m->castle == 0); EXPECT(m->camp == 0); }
        }
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_castle_occupy_reset_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_castle_occupy_reset_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
