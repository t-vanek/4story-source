// W3c-2 wire test: MW_CORPSREPLY_ACK corps formation over a
// three-peer loopback session.
//
// Alice 42/peer1 0x42 (chief party 100), Bob 200/peer2 0x43 (chief
// party 200), Carol 400/peer3 0x44 (chief party 300). Same country.
//
// Scenarios:
//   1. Bob declines Alice's invite → CORPSREPLY(reply) to Alice;
//      no corps formed.
//   2. Bob accepts → new corps (commander = Alice's party); both
//      get CORPSJOIN_REQ + PARTYATTR(commander) + the pairwise
//      ADDSQUAD; registry holds a 2-squad corps.
//   3. Carol accepts Alice's invite (Alice already in the corps) →
//      Carol's party joins the existing corps (grows to 3).

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

std::vector<std::byte> ReplyBody(std::uint32_t char_id, std::uint32_t key,
                                  std::uint8_t reply, const std::string& req)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, char_id);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, reply);
    tworldsvr::wire::WriteString(b, req);
    return b;
}

struct ReplyReq { std::uint32_t char_id = 0, key = 0; std::uint8_t result = 0;
                  std::string name; };
ReplyReq DecodeReply(const std::vector<std::byte>& b)
{
    ReplyReq a{};
    tworldsvr::wire::Reader r(b);
    r.Read(a.char_id); r.Read(a.key); r.Read(a.result); r.ReadString(a.name);
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

struct AddSquad { std::uint32_t recipient = 0, key = 0, chief = 0;
                  std::uint16_t party_id = 0; std::uint8_t count = 0; };
AddSquad DecodeAddSquadHdr(const std::vector<std::byte>& b)
{
    AddSquad a{};
    tworldsvr::wire::Reader r(b);
    r.Read(a.recipient); r.Read(a.key); r.Read(a.chief); r.Read(a.party_id);
    r.Read(a.count);
    return a;
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

const std::uint16_t kCorpsJoinId =
    tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_CORPSJOIN_REQ);
const std::uint16_t kAddSquadId =
    tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_ADDSQUAD_REQ);
const std::uint16_t kAttrId =
    tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::MW_PARTYATTR_REQ);

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

    auto set_char = [&](std::uint32_t id, const std::string& name,
                        std::uint16_t pid) {
        chars.Rename(id, name);
        auto c = chars.Find(id);
        if (c) { std::lock_guard g(c->lock); c->party_id = pid; }
    };
    auto mk_party = [&](std::uint16_t pid, std::uint32_t chief) {
        auto p = std::make_shared<tworldsvr::TParty>();
        p->id = pid; p->chief_char_id = chief; p->AddMember(chief);
        EXPECT(parties.Insert(p));
    };
    set_char(42, "Alice", 100);  mk_party(100, 42);
    set_char(200, "Bob",  200);  mk_party(200, 200);
    set_char(400, "Carol", 300); mk_party(300, 400);

    auto poll = [&](auto pred) {
        for (int i = 0; i < 200; ++i)
        { if (pred()) return true; std::this_thread::sleep_for(5ms); }
        return pred();
    };
    auto corps_of = [&](std::uint16_t pid) -> std::uint16_t {
        auto p = parties.Find(pid);
        if (!p) return 0;
        std::lock_guard g(p->lock);
        return p->corps_id;
    };

    // --- Scenario 1: Bob declines → CORPSREPLY to Alice -------------
    SendFramed(p2, ToUint16(MessageId::MW_CORPSREPLY_ACK),
        ReplyBody(200, 0xB0, /*ASK_NO=*/1, "Alice"));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CORPSREPLY_REQ));
        auto a = DecodeReply(b);
        EXPECT(a.char_id == 42); EXPECT(a.result == 1);
        EXPECT(a.name == "Bob");
    }
    EXPECT(corps_reg.Size() == 0);

    // --- Scenario 2: Bob accepts → new corps ------------------------
    SendFramed(p2, ToUint16(MessageId::MW_CORPSREPLY_ACK),
        ReplyBody(200, 0xB0, /*ASK_YES=*/0, "Alice"));
    // p2 (Bob): CORPSJOIN, PARTYATTR(party=200), ADDSQUAD(party=100)
    std::uint16_t corps_id = 0;
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == kCorpsJoinId);
        auto c = DecodeCorpsJoin(b);
        EXPECT(c.char_id == 200); EXPECT(c.commander == 100);
        corps_id = c.corps; EXPECT(corps_id != 0);
    }
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == kAttrId);
        auto a = DecodeAttr(b);
        EXPECT(a.char_id == 200); EXPECT(a.party_id == 200);
        EXPECT(a.chief == 200); EXPECT(a.commander == 100);
    }
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == kAddSquadId);
        auto a = DecodeAddSquadHdr(b);
        EXPECT(a.recipient == 200); EXPECT(a.party_id == 100);
        EXPECT(a.chief == 42); EXPECT(a.count == 1);
    }
    // p1 (Alice): ADDSQUAD(party=200), CORPSJOIN, PARTYATTR(party=100)
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kAddSquadId);
        auto a = DecodeAddSquadHdr(b);
        EXPECT(a.recipient == 42); EXPECT(a.party_id == 200);
        EXPECT(a.chief == 200);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kCorpsJoinId);
        auto c = DecodeCorpsJoin(b);
        EXPECT(c.char_id == 42); EXPECT(c.corps == corps_id);
        EXPECT(c.commander == 100);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kAttrId);
        auto a = DecodeAttr(b);
        EXPECT(a.char_id == 42); EXPECT(a.party_id == 100);
        EXPECT(a.commander == 100);
    }
    EXPECT(poll([&] { return corps_reg.Size() == 1; }));
    EXPECT(poll([&] {
        return corps_of(100) == corps_id && corps_of(200) == corps_id; }));
    {
        auto c = corps_reg.Find(corps_id);
        EXPECT(c != nullptr);
        if (c) { std::lock_guard g(c->lock);
                 EXPECT(c->Size() == 2); EXPECT(c->commander_party_id == 100);
                 EXPECT(c->general_char_id == 42); }
    }

    // --- Scenario 3: Carol accepts (Alice already in corps) ---------
    SendFramed(p3, ToUint16(MessageId::MW_CORPSREPLY_ACK),
        ReplyBody(400, 0xCA, /*ASK_YES=*/0, "Alice"));
    // Carol (p3): ADDSQUAD(party200), ADDSQUAD(party100), CORPSJOIN, ATTR
    {
        auto [w, b] = ReadFramed(p3);
        EXPECT(w == kAddSquadId);
        EXPECT(DecodeAddSquadHdr(b).party_id == 200);
    }
    {
        auto [w, b] = ReadFramed(p3);
        EXPECT(w == kAddSquadId);
        EXPECT(DecodeAddSquadHdr(b).party_id == 100);
    }
    {
        auto [w, b] = ReadFramed(p3);
        EXPECT(w == kCorpsJoinId);
        auto c = DecodeCorpsJoin(b);
        EXPECT(c.char_id == 400); EXPECT(c.corps == corps_id);
        EXPECT(c.commander == 100);
    }
    {
        auto [w, b] = ReadFramed(p3);
        EXPECT(w == kAttrId);
        EXPECT(DecodeAttr(b).char_id == 400);
    }
    // Existing squads each learn party 300.
    { auto [w, b] = ReadFramed(p2); EXPECT(w == kAddSquadId);
      EXPECT(DecodeAddSquadHdr(b).party_id == 300); }
    { auto [w, b] = ReadFramed(p1); EXPECT(w == kAddSquadId);
      EXPECT(DecodeAddSquadHdr(b).party_id == 300); }
    EXPECT(poll([&] { return corps_of(300) == corps_id; }));
    {
        auto c = corps_reg.Find(corps_id);
        EXPECT(c != nullptr);
        if (c) { std::lock_guard g(c->lock); EXPECT(c->Size() == 3);
                 EXPECT(c->IsParty(300)); }
    }
    EXPECT(corps_reg.Size() == 1);

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_corps_reply_handlers "
                    "(3 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_corps_reply_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
