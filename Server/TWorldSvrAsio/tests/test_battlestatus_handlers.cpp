// W5-4 wire test: war-window enable broadcast (SM_BATTLESTATUS_REQ).
//
// The scheduler opens a LOCAL / CASTLE / MISSION war window; world
// fans the matching enable packet to every map peer.

#include "../handlers/handlers.h"
#include "../services/battle_constants.h"
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

std::vector<std::byte> BattleBody(std::uint8_t type, std::uint8_t status,
                                  std::uint32_t start, std::uint32_t second)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint8_t>(b, type);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, status);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, start);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, second);
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
    namespace battle = tworldsvr::battle;

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

    // --- CASTLE window → MW_CASTLEENABLE_REQ to both peers --------
    SendFramed(p1, ToUint16(MessageId::SM_BATTLESTATUS_REQ),
               BattleBody(battle::kTypeCastle, 7, 100, 300));
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CASTLEENABLE_REQ));
        wire::Reader r(b);
        std::uint8_t status = 0; std::uint32_t second = 0;
        r.Read(status); r.Read(second);
        EXPECT(status == 7); EXPECT(second == 300);
    }

    // --- LOCAL window → MW_LOCALENABLE_REQ ------------------------
    SendFramed(p1, ToUint16(MessageId::SM_BATTLESTATUS_REQ),
               BattleBody(battle::kTypeLocal, 5, 100, 200));
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_LOCALENABLE_REQ));
        wire::Reader r(b);
        std::uint8_t status = 0, castle_day = 0;
        std::uint32_t second = 0, local_start = 0, castle_start = 0;
        r.Read(status); r.Read(second); r.Read(local_start);
        r.Read(castle_day); r.Read(castle_start);
        EXPECT(status == 5); EXPECT(second == 200);
    }

    // --- MISSION window → MW_MISSIONENABLE_REQ --------------------
    SendFramed(p1, ToUint16(MessageId::SM_BATTLESTATUS_REQ),
               BattleBody(battle::kTypeMission, 3, 50, 150));
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_MISSIONENABLE_REQ));
        wire::Reader r(b);
        std::uint8_t status = 0; std::uint32_t start = 0, second = 0;
        r.Read(status); r.Read(start); r.Read(second);
        EXPECT(status == 3); EXPECT(start == 50); EXPECT(second == 150);
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_battlestatus_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_battlestatus_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
