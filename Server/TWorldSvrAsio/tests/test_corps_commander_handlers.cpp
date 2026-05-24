// W3c-4 wire test: MW_CHGCORPSCOMMANDER_ACK over a three-peer
// loopback session.
//
// Corps 500 = squads [party100/Alice 42/peer1, party200/Bob
// 200/peer2, party300/Carol 400/peer3]; commander = party100,
// general = Alice.
//
// Scenarios:
//   1. Bob (not commander) tries to reassign → NOT_COMMANDER.
//   2. Alice targets the current commander → WRONG_TARGET.
//   3. Alice targets a party not in the corps → TARGET_NO_PARTY.
//   4. Alice hands the commander role to party200 → CHGCOMMANDER +
//      every squad refreshed with commander=200; registry updated.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/corps_constants.h"
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

namespace corps = tworldsvr::corps;

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

std::vector<std::byte> ChgBody(std::uint32_t char_id, std::uint32_t key,
                                std::uint16_t target_party)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, char_id);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, target_party);
    return b;
}

struct Reply { std::uint32_t char_id = 0, key = 0; std::uint8_t result = 0; };
Reply DecodeReply(const std::vector<std::byte>& b)
{
    Reply a{};
    tworldsvr::wire::Reader r(b);
    r.Read(a.char_id); r.Read(a.key); r.Read(a.result);
    return a;
}

struct CorpsJoin { std::uint32_t char_id = 0, key = 0; std::uint16_t corps = 0,
                   commander = 0; };
CorpsJoin DecodeCorpsJoin(const std::vector<std::byte>& b)
{
    CorpsJoin c{};
    tworldsvr::wire::Reader r(b);
    r.Read(c.char_id); r.Read(c.key); r.Read(c.corps); r.Read(c.commander);
    return c;
}

struct Attr { std::uint32_t char_id = 0, key = 0; std::uint16_t party_id = 0;
              std::uint8_t party_type = 0; std::uint32_t chief = 0;
              std::uint16_t commander = 0; };
Attr DecodeAttr(const std::vector<std::byte>& b)
{
    Attr a{};
    tworldsvr::wire::Reader r(b);
    r.Read(a.char_id); r.Read(a.key); r.Read(a.party_id); r.Read(a.party_type);
    r.Read(a.chief); r.Read(a.commander);
    return a;
}

const std::uint16_t kReplyId = tnetlib::protocol::ToUint16(
    tnetlib::protocol::MessageId::MW_CHGCORPSCOMMANDER_REQ);
const std::uint16_t kCorpsJoinId = tnetlib::protocol::ToUint16(
    tnetlib::protocol::MessageId::MW_CORPSJOIN_REQ);
const std::uint16_t kAttrId = tnetlib::protocol::ToUint16(
    tnetlib::protocol::MessageId::MW_PARTYATTR_REQ);

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
    for (int i = 0; i < 100; ++i)
    {
        if (chars.Find(42) && chars.Find(200) && chars.Find(400)) break;
        std::this_thread::sleep_for(10ms);
    }
    EXPECT(chars.Find(42) && chars.Find(200) && chars.Find(400));

    auto mk = [&](std::uint32_t id, std::uint16_t pid) {
        auto c = chars.Find(id);
        if (c) { std::lock_guard g(c->lock); c->party_id = pid; }
        auto p = std::make_shared<tworldsvr::TParty>();
        p->id = pid; p->chief_char_id = id; p->AddMember(id); p->corps_id = 500;
        EXPECT(parties.Insert(p));
    };
    mk(42, 100); mk(200, 200); mk(400, 300);
    {
        auto c = std::make_shared<tworldsvr::TCorps>();
        c->id = 500; c->commander_party_id = 100; c->general_char_id = 42;
        c->AddParty(100); c->AddParty(200); c->AddParty(300);
        EXPECT(corps_reg.Insert(c));
    }
    auto poll = [&](auto pred) {
        for (int i = 0; i < 200; ++i)
        { if (pred()) return true; std::this_thread::sleep_for(5ms); }
        return pred();
    };

    // --- Scenario 1: non-commander Bob → NOT_COMMANDER --------------
    SendFramed(p2, ToUint16(MessageId::MW_CHGCORPSCOMMANDER_ACK),
        ChgBody(200, 0xB0, 300));
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == kReplyId);
        auto a = DecodeReply(b);
        EXPECT(a.char_id == 200); EXPECT(a.result == corps::kNotCommander);
    }

    // --- Scenario 2: Alice targets current commander → WRONG_TARGET -
    SendFramed(p1, ToUint16(MessageId::MW_CHGCORPSCOMMANDER_ACK),
        ChgBody(42, 0xA1, 100));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kReplyId);
        EXPECT(DecodeReply(b).result == corps::kWrongTarget);
    }

    // --- Scenario 3: Alice targets a non-member party → TARGET_NO_PARTY
    SendFramed(p1, ToUint16(MessageId::MW_CHGCORPSCOMMANDER_ACK),
        ChgBody(42, 0xA1, 999));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kReplyId);
        EXPECT(DecodeReply(b).result == corps::kTargetNoParty);
    }

    // --- Scenario 4: Alice hands commander to party200 → success ----
    SendFramed(p1, ToUint16(MessageId::MW_CHGCORPSCOMMANDER_ACK),
        ChgBody(42, 0xA1, 200));
    // p1 (Alice): reply CHGCOMMANDER, then CORPSJOIN + PARTYATTR(cmd=200)
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kReplyId);
        EXPECT(DecodeReply(b).result == corps::kChgCommander);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kCorpsJoinId);
        auto c = DecodeCorpsJoin(b);
        EXPECT(c.char_id == 42); EXPECT(c.corps == 500);
        EXPECT(c.commander == 200);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kAttrId); EXPECT(DecodeAttr(b).commander == 200);
    }
    // p2 (Bob, new commander) + p3 (Carol): CORPSJOIN + PARTYATTR(cmd=200)
    for (auto* s : {&p2, &p3})
    {
        { auto [w, b] = ReadFramed(*s); EXPECT(w == kCorpsJoinId);
          EXPECT(DecodeCorpsJoin(b).commander == 200); }
        { auto [w, b] = ReadFramed(*s); EXPECT(w == kAttrId);
          EXPECT(DecodeAttr(b).commander == 200); }
    }
    EXPECT(poll([&] {
        auto c = corps_reg.Find(500);
        if (!c) return false;
        std::lock_guard g(c->lock);
        return c->commander_party_id == 200 && c->general_char_id == 200;
    }));

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_corps_commander_handlers "
                    "(4 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_corps_commander_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
