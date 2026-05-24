// W3b-4 wire test: party attribute changes (MANSTAT / CHGCHIEF /
// CHGTYPE) over a three-peer loopback session.
//
// Party id=100: Alice 42/peer1 0x42 (chief), Bob 200/peer2 0x43,
// Carol 400/peer3 0x44; member order [42,200,400]; obtain PT_FREE.
//
// Scenarios:
//   1. MANSTAT(member=Bob)        → MANSTAT_REQ to all 3 + Bob's
//      stored stats updated.
//   2. CHGTYPE by Bob (non-chief) → CHGTYPE_REQ(NOTCHIEF) to Bob
//      only; obtain unchanged.
//   3. CHGTYPE by Alice (chief)   → CHGTYPE_REQ(result=0) to all 3;
//      obtain = PT_LOTTERY.
//   4. CHGCHIEF Alice→Bob         → CHGCHIEF_REQ(CHGCHIEF) to Alice
//      + PARTYATTR(chief=Bob) to all 3; party chief = Bob.
//   5. CHGCHIEF by ex-chief Alice → CHGCHIEF_REQ(NOTCHIEF).
//   6. CHGCHIEF Bob→Bob (self)    → CHGCHIEF_REQ(ALREADY).

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/guild_registry.h"
#include "../services/party_constants.h"
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

namespace party = tworldsvr::party;

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

std::vector<std::byte> ManstatBody(std::uint16_t party_id,
                                    std::uint32_t member_id,
                                    std::uint8_t type, std::uint8_t level,
                                    std::uint32_t max_hp, std::uint32_t hp,
                                    std::uint32_t max_mp, std::uint32_t mp)
{
    std::vector<std::byte> b;
    using namespace tworldsvr::wire;
    WritePOD<std::uint16_t>(b, party_id);
    WritePOD<std::uint32_t>(b, member_id);
    WritePOD<std::uint8_t>(b, type);
    WritePOD<std::uint8_t>(b, level);
    WritePOD<std::uint32_t>(b, max_hp);
    WritePOD<std::uint32_t>(b, hp);
    WritePOD<std::uint32_t>(b, max_mp);
    WritePOD<std::uint32_t>(b, mp);
    return b;
}

std::vector<std::byte> ChgChiefBody(std::uint32_t chief_id, std::uint32_t key,
                                     std::uint32_t target_id)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, chief_id);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, target_id);
    return b;
}

std::vector<std::byte> ChgTypeBody(std::uint32_t char_id, std::uint32_t key,
                                    std::uint8_t party_type)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, char_id);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, party_type);
    return b;
}

struct Manstat { std::uint32_t recipient = 0, key = 0, member = 0;
                 std::uint8_t type = 0, level = 0;
                 std::uint32_t max_hp = 0, hp = 0, max_mp = 0, mp = 0; };
Manstat DecodeManstat(const std::vector<std::byte>& b)
{
    Manstat m{};
    tworldsvr::wire::Reader r(b);
    r.Read(m.recipient); r.Read(m.key); r.Read(m.member); r.Read(m.type);
    r.Read(m.level); r.Read(m.max_hp); r.Read(m.hp); r.Read(m.max_mp);
    r.Read(m.mp);
    return m;
}

struct ChgChief { std::uint32_t char_id = 0, key = 0; std::uint8_t result = 0; };
ChgChief DecodeChgChief(const std::vector<std::byte>& b)
{
    ChgChief c{};
    tworldsvr::wire::Reader r(b);
    r.Read(c.char_id); r.Read(c.key); r.Read(c.result);
    return c;
}

struct ChgType { std::uint32_t char_id = 0, key = 0;
                 std::uint8_t result = 0, party_type = 0; };
ChgType DecodeChgType(const std::vector<std::byte>& b)
{
    ChgType c{};
    tworldsvr::wire::Reader r(b);
    r.Read(c.char_id); r.Read(c.key); r.Read(c.result); r.Read(c.party_type);
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
    tworldsvr::PeerRegistry   peers;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.parties = &parties; ctx.peers = &peers; ctx.nation = 0;

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
    EXPECT(peers.Size() == 3);

    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, 0xA1));
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(200, 0xB0));
    SendFramed(p3, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(400, 0xCA));
    for (int i = 0; i < 100; ++i)
    {
        if (chars.Find(42) && chars.Find(200) && chars.Find(400)) break;
        std::this_thread::sleep_for(10ms);
    }
    EXPECT(chars.Find(42) && chars.Find(200) && chars.Find(400));

    constexpr std::uint16_t kPid = 100;
    {
        auto pty = std::make_shared<tworldsvr::TParty>();
        pty->id = kPid; pty->obtain_type = party::kObtainFree;
        pty->chief_char_id = 42;
        for (std::uint32_t m : {42u, 200u, 400u}) pty->AddMember(m);
        EXPECT(parties.Insert(pty));
    }
    for (std::uint32_t id : {42u, 200u, 400u})
        if (auto c = chars.Find(id)) { std::lock_guard g(c->lock);
            c->party_id = kPid; }

    auto poll = [&](auto pred) {
        for (int i = 0; i < 200; ++i)
        { if (pred()) return true; std::this_thread::sleep_for(5ms); }
        return pred();
    };

    // --- Scenario 1: MANSTAT for Bob → broadcast to all 3 ------------
    SendFramed(p2, ToUint16(MessageId::MW_PARTYMANSTAT_ACK),
        ManstatBody(kPid, 200, /*type=*/1, /*level=*/20, 500, 400, 200, 150));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_PARTYMANSTAT_REQ));
        auto m = DecodeManstat(b);
        EXPECT(m.recipient == 42); EXPECT(m.member == 200);
        EXPECT(m.level == 20); EXPECT(m.max_hp == 500); EXPECT(m.hp == 400);
    }
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_PARTYMANSTAT_REQ));
        auto m = DecodeManstat(b);
        EXPECT(m.recipient == 200); EXPECT(m.member == 200);
        EXPECT(m.max_mp == 200); EXPECT(m.mp == 150);
    }
    {
        auto [w, b] = ReadFramed(p3);
        EXPECT(w == ToUint16(MessageId::MW_PARTYMANSTAT_REQ));
        EXPECT(DecodeManstat(b).recipient == 400);
    }
    EXPECT(poll([&] {
        auto c = chars.Find(200);
        if (!c) return false;
        std::lock_guard g(c->lock);
        return c->max_hp == 500 && c->hp == 400 && c->max_mp == 200;
    }));

    // --- Scenario 2: CHGTYPE by non-chief Bob → NOTCHIEF -------------
    SendFramed(p2, ToUint16(MessageId::MW_CHGPARTYTYPE_ACK),
        ChgTypeBody(200, 0xB0, party::kObtainHunter));
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_CHGPARTYTYPE_REQ));
        auto c = DecodeChgType(b);
        EXPECT(c.char_id == 200); EXPECT(c.result == party::kNotChief);
        EXPECT(c.party_type == party::kObtainHunter);
    }
    {
        auto p = parties.Find(kPid);
        std::lock_guard g(p->lock);
        EXPECT(p->obtain_type == party::kObtainFree); // unchanged
    }

    // --- Scenario 3: CHGTYPE by chief Alice → broadcast result=0 -----
    SendFramed(p1, ToUint16(MessageId::MW_CHGPARTYTYPE_ACK),
        ChgTypeBody(42, 0xA1, party::kObtainLottery));
    for (auto* s : {&p1, &p2, &p3})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CHGPARTYTYPE_REQ));
        auto c = DecodeChgType(b);
        EXPECT(c.result == 0);
        EXPECT(c.party_type == party::kObtainLottery);
    }
    EXPECT(poll([&] {
        auto p = parties.Find(kPid);
        std::lock_guard g(p->lock);
        return p->obtain_type == party::kObtainLottery;
    }));

    // --- Scenario 4: CHGCHIEF Alice→Bob → reply + PARTYATTR all ------
    SendFramed(p1, ToUint16(MessageId::MW_CHGPARTYCHIEF_ACK),
        ChgChiefBody(42, 0xA1, 200));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CHGPARTYCHIEF_REQ));
        auto c = DecodeChgChief(b);
        EXPECT(c.char_id == 42); EXPECT(c.result == party::kChgChief);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_PARTYATTR_REQ));
        auto a = DecodeAttr(b);
        EXPECT(a.char_id == 42); EXPECT(a.chief == 200);
        EXPECT(a.party_id == kPid);
        EXPECT(a.party_type == party::kObtainLottery);
    }
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_PARTYATTR_REQ));
        EXPECT(DecodeAttr(b).chief == 200);
    }
    {
        auto [w, b] = ReadFramed(p3);
        EXPECT(w == ToUint16(MessageId::MW_PARTYATTR_REQ));
        EXPECT(DecodeAttr(b).chief == 200);
    }
    EXPECT(poll([&] {
        auto p = parties.Find(kPid);
        std::lock_guard g(p->lock);
        return p->IsChief(200);
    }));

    // --- Scenario 5: CHGCHIEF by ex-chief Alice → NOTCHIEF ----------
    SendFramed(p1, ToUint16(MessageId::MW_CHGPARTYCHIEF_ACK),
        ChgChiefBody(42, 0xA1, 400));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CHGPARTYCHIEF_REQ));
        auto c = DecodeChgChief(b);
        EXPECT(c.char_id == 42); EXPECT(c.result == party::kNotChief);
    }

    // --- Scenario 6: CHGCHIEF Bob→Bob (self) → ALREADY --------------
    SendFramed(p2, ToUint16(MessageId::MW_CHGPARTYCHIEF_ACK),
        ChgChiefBody(200, 0xB0, 200));
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_CHGPARTYCHIEF_REQ));
        auto c = DecodeChgChief(b);
        EXPECT(c.char_id == 200); EXPECT(c.result == party::kAlready);
    }

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_party_attr_handlers "
                    "(6 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_party_attr_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
