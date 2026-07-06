// W6-44 wire tests: castle-war info engine.
//
//   Report for castle 1 (G1 defend+accept, G2 2×accept) →
//     aggregation (21/20 points), country points D=21/C=20, top-3
//     rows for both countries, defender G1 (top points), attacker G2
//     (≠ defender country).
//   Second report for castle 2 where G1 scores higher → the
//     recursive cross-castle steal: castle 2 keeps G1 as defender,
//     castle 1 re-elects G2 (attacker slots empty out).
//   castle==0 → replay of the current scoreboard to the asking map
//     only.

#include "../handlers/handlers.h"
#include "../services/castle_war_registry.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/guild_registry.h"
#include "../services/peer_registry.h"
#include "../wire_codec.h"
#include "../world_server.h"
#include "../world_session.h"

#include "MessageId.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <utility>
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

// One-local occupation report: 6 slots of (guild, occupy_type).
std::vector<std::byte>
ReportBody(std::uint16_t castle,
           const std::vector<std::pair<std::uint32_t, std::uint8_t>>& slots)
{
    namespace wire = tworldsvr::wire;
    std::vector<std::byte> b;
    wire::WritePOD<std::uint16_t>(b, castle);
    wire::WritePOD<std::uint32_t>(b, 0);          // dwGuild (unused)
    wire::WritePOD<std::uint8_t>(b, 1);           // local count
    wire::WritePOD<std::uint16_t>(b, 501);        // local id
    for (std::uint8_t j = 0; j < 6; ++j)
    {
        const auto& s = j < slots.size()
                            ? slots[j]
                            : std::pair<std::uint32_t, std::uint8_t>{0, 0};
        wire::WritePOD<std::uint32_t>(b, s.first);
        wire::WritePOD<std::uint8_t>(b, s.second);
    }
    return b;
}

struct WarInfoRow
{
    std::uint16_t castle = 0;
    std::uint32_t def_id = 0;
    std::string   def_name;
    std::uint8_t  def_country = 0xFF;
    std::uint16_t point_d = 0;
    std::uint32_t atk_id = 0;
    std::string   atk_name;
    std::uint16_t point_c = 0;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> guilds;
    std::vector<std::tuple<std::uint8_t, std::string, std::uint16_t>> top3;
};

WarInfoRow ParseRow(const std::vector<std::byte>& body)
{
    namespace wire = tworldsvr::wire;
    wire::Reader r(body);
    WarInfoRow out{};
    std::uint32_t gcount = 0;
    std::uint8_t  tcount = 0;
    r.Read(out.castle);
    r.Read(out.def_id); r.ReadString(out.def_name);
    r.Read(out.def_country); r.Read(out.point_d);
    r.Read(out.atk_id); r.ReadString(out.atk_name);
    r.Read(out.point_c); r.Read(gcount);
    for (std::uint32_t i = 0; i < gcount; ++i)
    {
        std::uint32_t g = 0, p = 0;
        r.Read(g); r.Read(p);
        out.guilds.emplace_back(g, p);
    }
    r.Read(tcount);
    for (std::uint8_t i = 0; i < tcount; ++i)
    {
        std::uint8_t c = 0; std::string n; std::uint16_t pt = 0;
        r.Read(c); r.ReadString(n); r.Read(pt);
        out.top3.emplace_back(c, n, pt);
    }
    return out;
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
    tworldsvr::CharRegistry      chars;
    tworldsvr::GuildRegistry     guilds;
    tworldsvr::PeerRegistry      peers;
    tworldsvr::CtrlSvrSlot       ctrl_svr;
    tworldsvr::CastleWarRegistry castle_war;

    // G1 "Alpha" — war country D(0); G2 "Bravo" — war country C(1).
    {
        auto g1 = std::make_shared<tworldsvr::TGuild>();
        g1->id = 11; g1->name = "Alpha"; g1->country = 0;
        g1->pvp_total_point = 100; g1->establish_time = 1000;
        guilds.Insert(g1);
        auto g2 = std::make_shared<tworldsvr::TGuild>();
        g2->id = 22; g2->name = "Bravo"; g2->country = 1;
        g2->pvp_total_point = 200; g2->establish_time = 2000;
        guilds.Insert(g2);
    }

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.ctrl_svr = &ctrl_svr;
    ctx.castle_war = &castle_war;
    ctx.castle_war_day = 7;
    ctx.nation = 0;

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

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0442));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0443));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    // --- W6-44a: castle 1 report → aggregation + election ----------
    SendFramed(p1, ToUint16(MessageId::MW_CASTLEWARINFO_ACK),
        ReportBody(1, {{11, 0}, {11, 1}, {22, 1}, {22, 1}}));
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CASTLEWARINFO_REQ));
        const auto row = ParseRow(b);
        EXPECT(row.castle == 1);
        EXPECT(row.def_id == 11);
        EXPECT(row.def_name == "Alpha");
        EXPECT(row.def_country == 0);
        EXPECT(row.point_d == 21);      // 11 (defend) + 10 (accept)
        EXPECT(row.atk_id == 22);       // country C ≠ def country D
        EXPECT(row.atk_name == "Bravo");
        EXPECT(row.point_c == 20);
        EXPECT(row.guilds.size() == 2);
        EXPECT(row.guilds[0].first == 11 && row.guilds[0].second == 21);
        EXPECT(row.guilds[1].first == 22 && row.guilds[1].second == 20);
        EXPECT(row.top3.size() == 2);
        EXPECT(std::get<0>(row.top3[0]) == 0);
        EXPECT(std::get<1>(row.top3[0]) == "Alpha");
        EXPECT(std::get<2>(row.top3[0]) == 21);
        EXPECT(std::get<0>(row.top3[1]) == 1);
        EXPECT(std::get<1>(row.top3[1]) == "Bravo");
        EXPECT(std::get<2>(row.top3[1]) == 20);
    }

    // --- W6-44b: cross-castle defender steal ------------------------
    // Castle 2: G1 alone with 3×ACCEPT = 30 points. G1 must keep
    // castle 2 (higher score) and castle 1 re-elects G2 as defender;
    // both attacker slots empty out (all guilds consumed).
    SendFramed(p2, ToUint16(MessageId::MW_CASTLEWARINFO_ACK),
        ReportBody(2, {{11, 1}, {11, 1}, {11, 1}}));
    for (auto* s : {&p1, &p2})
    {
        WarInfoRow c1{}, c2{};
        for (int k = 0; k < 2; ++k)
        {
            auto [w, b] = ReadFramed(*s);
            EXPECT(w == ToUint16(MessageId::MW_CASTLEWARINFO_REQ));
            const auto row = ParseRow(b);
            if (row.castle == 1) c1 = row; else c2 = row;
        }
        EXPECT(c1.castle == 1);
        EXPECT(c1.def_id == 22);        // G2 inherits castle 1
        EXPECT(c1.def_country == 1);
        EXPECT(c1.atk_id == 0);         // nobody left to attack
        EXPECT(c2.castle == 2);
        EXPECT(c2.def_id == 11);        // G1 keeps the richer castle
        EXPECT(c2.def_country == 0);
        EXPECT(c2.atk_id == 0);
        EXPECT(c2.point_d == 30);
    }

    // --- W6-44c: replay on request (castle == 0) --------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint16_t>(b, 0);
        SendFramed(p2, ToUint16(MessageId::MW_CASTLEWARINFO_ACK), b);
    }
    {
        WarInfoRow c1{}, c2{};
        for (int k = 0; k < 2; ++k)
        {
            auto [w, b] = ReadFramed(p2);
            EXPECT(w == ToUint16(MessageId::MW_CASTLEWARINFO_REQ));
            const auto row = ParseRow(b);
            if (row.castle == 1) c1 = row; else c2 = row;
        }
        EXPECT(c1.def_id == 22);
        EXPECT(c2.def_id == 11);
    }
    // p1 saw nothing from the replay — sentinel via ENDWAR.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint16_t>(b, 9);
        SendFramed(p2, ToUint16(MessageId::MW_ENDWAR_ACK), b);
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_ENDWAR_REQ));
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_castlewarinfo_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_castlewarinfo_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
