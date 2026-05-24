// W3c-6 wire test: MW_CORPSCMD_ACK command broadcast.
//
// Corps 500 = [party10 (Alice 42/peer1 chief, Bob 200/peer2),
// party20 (Carol 400/peer3 chief)], commander/general = Alice. Dan
// 600/peer2 is the chief of party30 (no corps).
//
// Scenarios:
//   1. Alice (general) issues a command → every corps member
//      (42/p1, 200/p2, 400/p3) gets MW_CORPSCMD_REQ.
//   2. Dan (corps-less) issues a command → only his own party
//      member (600/p2) gets it.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/corps_registry.h"
#include "../services/guild_registry.h"
#include "../services/party_registry.h"
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

std::vector<std::byte> CmdBody(std::uint32_t general, std::uint32_t key,
                                std::uint16_t map_id, std::uint16_t squad_id,
                                std::uint32_t char_id, std::uint8_t cmd,
                                std::uint32_t target_id, std::uint8_t tg_type,
                                std::uint16_t pos_x, std::uint16_t pos_z)
{
    std::vector<std::byte> b;
    using namespace tworldsvr::wire;
    WritePOD<std::uint32_t>(b, general);
    WritePOD<std::uint32_t>(b, key);
    WritePOD<std::uint16_t>(b, map_id);
    WritePOD<std::uint16_t>(b, squad_id);
    WritePOD<std::uint32_t>(b, char_id);
    WritePOD<std::uint8_t>(b, cmd);
    WritePOD<std::uint32_t>(b, target_id);
    WritePOD<std::uint8_t>(b, tg_type);
    WritePOD<std::uint16_t>(b, pos_x);
    WritePOD<std::uint16_t>(b, pos_z);
    return b;
}

struct CmdReq {
    std::uint32_t member = 0, key = 0, commander = 0, target = 0;
    std::uint16_t squad = 0, map = 0, pos_x = 0, pos_z = 0;
    std::uint8_t  cmd = 0, tg_type = 0;
};
CmdReq DecodeCmdReq(const std::vector<std::byte>& b)
{
    CmdReq c{};
    tworldsvr::wire::Reader r(b);
    r.Read(c.member); r.Read(c.key); r.Read(c.squad); r.Read(c.commander);
    r.Read(c.map); r.Read(c.cmd); r.Read(c.target); r.Read(c.tg_type);
    r.Read(c.pos_x); r.Read(c.pos_z);
    return c;
}

CmdReq ReadCmd(boost::asio::ip::tcp::socket& s)
{
    auto [w, b] = ReadFramed(s);
    EXPECT(w == tnetlib::protocol::ToUint16(
        tnetlib::protocol::MessageId::MW_CORPSCMD_REQ));
    return DecodeCmdReq(b);
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
    tworldsvr::PartyRegistry  parties;
    tworldsvr::CorpsRegistry  corps_reg;
    tworldsvr::PeerRegistry   peers;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.parties = &parties; ctx.corps = &corps_reg; ctx.peers = &peers;
    ctx.nation = 0;

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
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(600, 0xDD));
    for (int i = 0; i < 100; ++i)
    {
        if (chars.Find(42) && chars.Find(200) && chars.Find(400) &&
            chars.Find(600)) break;
        std::this_thread::sleep_for(10ms);
    }
    EXPECT(chars.Find(42) && chars.Find(600));

    auto set_party = [&](std::uint32_t id, std::uint16_t pid) {
        auto c = chars.Find(id);
        if (c) { std::lock_guard g(c->lock); c->party_id = pid; }
    };
    auto mk = [&](std::uint16_t pid, std::uint32_t chief,
                  std::vector<std::uint32_t> mem, std::uint16_t corps_id) {
        auto p = std::make_shared<tworldsvr::TParty>();
        p->id = pid; p->chief_char_id = chief;
        for (auto m : mem) p->AddMember(m);
        p->corps_id = corps_id;
        EXPECT(parties.Insert(p));
    };
    set_party(42, 10); set_party(200, 10); set_party(400, 20);
    set_party(600, 30);
    mk(10, 42, {42, 200}, 500);
    mk(20, 400, {400}, 500);
    mk(30, 600, {600}, 0);
    {
        auto c = std::make_shared<tworldsvr::TCorps>();
        c->id = 500; c->commander_party_id = 10; c->general_char_id = 42;
        c->AddParty(10); c->AddParty(20);
        EXPECT(corps_reg.Insert(c));
    }

    // --- Scenario 1: general's command → all corps members ----------
    SendFramed(p1, ToUint16(MessageId::MW_CORPSCMD_ACK),
        CmdBody(42, 0xA1, /*map=*/50, /*squad=*/10, /*char=*/42, /*cmd=*/5,
                /*target=*/0xBEEF, /*tg_type=*/1, /*pos_x=*/100,
                /*pos_z=*/200));
    {
        auto c = ReadCmd(p1);   // member 42 (party10, peer1)
        EXPECT(c.member == 42); EXPECT(c.squad == 10);
        EXPECT(c.commander == 42); EXPECT(c.cmd == 5);
        EXPECT(c.target == 0xBEEF); EXPECT(c.tg_type == 1);
        EXPECT(c.map == 50); EXPECT(c.pos_x == 100); EXPECT(c.pos_z == 200);
    }
    {
        auto c = ReadCmd(p2);   // member 200 (party10, peer2)
        EXPECT(c.member == 200); EXPECT(c.commander == 42);
        EXPECT(c.target == 0xBEEF); EXPECT(c.squad == 10);
    }
    {
        auto c = ReadCmd(p3);   // member 400 (party20, peer3)
        EXPECT(c.member == 400); EXPECT(c.commander == 42);
        EXPECT(c.cmd == 5);
    }

    // --- Scenario 2: corps-less general → only own party member -----
    SendFramed(p2, ToUint16(MessageId::MW_CORPSCMD_ACK),
        CmdBody(600, 0xDD, /*map=*/51, /*squad=*/30, /*char=*/600, /*cmd=*/3,
                /*target=*/0xCAFE, /*tg_type=*/2, /*pos_x=*/5, /*pos_z=*/6));
    {
        auto c = ReadCmd(p2);   // member 600 (party30, peer2)
        EXPECT(c.member == 600); EXPECT(c.squad == 30);
        EXPECT(c.commander == 600); EXPECT(c.cmd == 3);
        EXPECT(c.target == 0xCAFE); EXPECT(c.map == 51);
    }

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_corps_cmd_handlers "
                    "(2 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_corps_cmd_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
