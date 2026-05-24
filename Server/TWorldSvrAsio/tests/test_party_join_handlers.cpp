// W3b-2 wire test: MW_PARTYJOIN_ACK party formation.
//
// Three map peers: Alice (char 42, peer1/wID 0x42), Bob (char 200,
// peer2/0x43), Carol (char 400, peer3/0x44).
//
// Scenarios:
//   A. Decline (response=ASK_NO)  → PARTY_DENY relayed to inviter.
//   B. Inviter offline            → PARTY_NOREQUSER to invitee.
//   C. New party (Bob accepts)    → pairwise PARTYJOIN_REQ + the
//      two PARTYATTR pushes; registry holds a 2-member party with
//      Alice as chief and both party_id back-pointers set.
//   D. Join existing (Carol)      → party grows to 3; Carol learns
//      both existing members, each existing member learns Carol.

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

std::vector<std::byte> NameBody(std::uint32_t char_id, std::uint32_t key,
                                 const std::string& name)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, /*IK_NAME=*/ 48);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, 0);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, 0);
    tworldsvr::wire::WriteString(b, name);
    return b;
}

std::vector<std::byte> JoinBody(const std::string& origin,
                                 const std::string& target,
                                 std::uint8_t  obtain,
                                 std::uint8_t  response,
                                 std::uint32_t max_hp, std::uint32_t hp,
                                 std::uint32_t max_mp, std::uint32_t mp)
{
    std::vector<std::byte> b;
    using namespace tworldsvr::wire;
    WriteString(b, origin);
    WriteString(b, target);
    WritePOD<std::uint8_t>(b, obtain);
    WritePOD<std::uint8_t>(b, response);
    WritePOD<std::uint32_t>(b, max_hp);
    WritePOD<std::uint32_t>(b, hp);
    WritePOD<std::uint32_t>(b, max_mp);
    WritePOD<std::uint32_t>(b, mp);
    return b;
}

struct AddReq { std::uint32_t char_id, key; std::string req, tgt;
                std::uint8_t obtain, result; std::uint32_t request; };
AddReq DecodeAddReq(const std::vector<std::byte>& body)
{
    AddReq a{};
    tworldsvr::wire::Reader r(body);
    r.Read(a.char_id); r.Read(a.key); r.ReadString(a.req); r.ReadString(a.tgt);
    r.Read(a.obtain); r.Read(a.result); r.Read(a.request);
    return a;
}

struct JoinReq {
    std::uint32_t recipient = 0, recipient_key = 0;
    std::uint16_t party_id = 0;
    std::string   member_name;
    std::uint32_t member_id = 0, chief_id = 0;
    std::uint16_t commander = 0;
    std::string   guild_name;
    std::uint8_t  level = 0;
    std::uint32_t max_hp = 0, hp = 0, max_mp = 0, mp = 0;
    std::uint8_t  race = 0, sex = 0, face = 0, hair = 0, obtain = 0, klass = 0;
};
JoinReq DecodeJoinReq(const std::vector<std::byte>& body)
{
    JoinReq j{};
    tworldsvr::wire::Reader r(body);
    r.Read(j.recipient); r.Read(j.recipient_key); r.Read(j.party_id);
    r.ReadString(j.member_name); r.Read(j.member_id); r.Read(j.chief_id);
    r.Read(j.commander); r.ReadString(j.guild_name); r.Read(j.level);
    r.Read(j.max_hp); r.Read(j.hp); r.Read(j.max_mp); r.Read(j.mp);
    r.Read(j.race); r.Read(j.sex); r.Read(j.face); r.Read(j.hair);
    r.Read(j.obtain); r.Read(j.klass);
    return j;
}

struct AttrReq { std::uint32_t char_id = 0, key = 0; std::uint16_t party_id = 0;
                 std::uint8_t party_type = 0; std::uint32_t chief_id = 0;
                 std::uint16_t commander = 0; };
AttrReq DecodeAttrReq(const std::vector<std::byte>& body)
{
    AttrReq a{};
    tworldsvr::wire::Reader r(body);
    r.Read(a.char_id); r.Read(a.key); r.Read(a.party_id);
    r.Read(a.party_type); r.Read(a.chief_id); r.Read(a.commander);
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
    ctx.io      = &io;
    ctx.chars   = &chars;
    ctx.guilds  = &guilds;
    ctx.parties = &parties;
    ctx.peers   = &peers;
    ctx.nation  = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0;
    svr_cfg.max_connections = 6;
    svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket peer1(client_io), peer2(client_io), peer3(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    peer1.connect(ep); peer2.connect(ep); peer3.connect(ep);
    std::this_thread::sleep_for(20ms);

    auto reg = [&](tcp::socket& s, std::uint16_t wid) {
        SendFramed(s, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(wid));
        auto [w, _] = ReadFramed(s);
        EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK));
    };
    auto drain_relayconnect = [&](tcp::socket& s) {
        auto [w, _] = ReadFramed(s);
        EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ));
    };
    // Each registration after the first fans a RELAYCONNECT to the
    // already-registered peers — drain them so they don't desync.
    reg(peer1, 0x0042);
    reg(peer2, 0x0043); drain_relayconnect(peer1);
    reg(peer3, 0x0044); drain_relayconnect(peer1); drain_relayconnect(peer2);
    EXPECT(peers.Size() == 3);

    SendFramed(peer1, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(42, 0xA11CE));
    SendFramed(peer1, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        NameBody(42, 0xA11CE, "Alice"));
    SendFramed(peer2, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(200, 0xB0B));
    SendFramed(peer2, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        NameBody(200, 0xB0B, "Bob"));
    SendFramed(peer3, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(400, 0xCA801));
    SendFramed(peer3, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        NameBody(400, 0xCA801, "Carol"));

    for (int i = 0; i < 100; ++i)
    {
        if (chars.FindByName("Alice") && chars.FindByName("Bob") &&
            chars.FindByName("Carol")) break;
        std::this_thread::sleep_for(10ms);
    }
    EXPECT(chars.FindByName("Alice"));
    EXPECT(chars.FindByName("Bob"));
    EXPECT(chars.FindByName("Carol"));

    auto with_char = [&](std::uint32_t id, auto fn) {
        auto c = chars.Find(id);
        EXPECT(c != nullptr);
        if (c) { std::lock_guard g(c->lock); fn(*c); }
    };
    // Distinct describe-fields so the JOIN_REQ propagation is checked.
    with_char(42, [](tworldsvr::TChar& c) {
        c.level = 10; c.race = 1; c.sex = 0; c.face = 2; c.hair = 3;
        c.klass = 4; c.max_hp = 111; c.hp = 110; c.max_mp = 22; c.mp = 21; });
    with_char(200, [](tworldsvr::TChar& c) {
        c.level = 20; c.race = 2; c.sex = 1; c.face = 5; c.hair = 6;
        c.klass = 7; });
    with_char(400, [](tworldsvr::TChar& c) {
        c.level = 30; c.race = 3; c.klass = 8; });

    // --- Scenario A: decline → PARTY_DENY to inviter -----------------
    SendFramed(peer2, ToUint16(MessageId::MW_PARTYJOIN_ACK),
        JoinBody("Alice", "Bob", party::kObtainFree, /*ASK_NO=*/1,
                 1, 1, 1, 1));
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_PARTYADD_REQ));
        auto a = DecodeAddReq(body);
        EXPECT(a.char_id == 42);
        EXPECT(a.result == party::kDeny);
    }

    // --- Scenario B: inviter offline → PARTY_NOREQUSER to invitee ----
    SendFramed(peer2, ToUint16(MessageId::MW_PARTYJOIN_ACK),
        JoinBody("Nobody", "Bob", party::kObtainFree, party::kAskYes,
                 1, 1, 1, 1));
    {
        auto [w, body] = ReadFramed(peer2);
        EXPECT(w == ToUint16(MessageId::MW_PARTYADD_REQ));
        auto a = DecodeAddReq(body);
        EXPECT(a.char_id == 200);
        EXPECT(a.result == party::kNoReqUser);
    }
    EXPECT(parties.Size() == 0);

    // --- Scenario C: Bob accepts → new party -------------------------
    SendFramed(peer2, ToUint16(MessageId::MW_PARTYJOIN_ACK),
        JoinBody("Alice", "Bob", party::kObtainHunter, party::kAskYes,
                 1000, 900, 500, 400));
    std::uint16_t pid = 0;
    // peer1 (Alice): PARTYATTR(Alice) then JOIN_REQ describing Bob.
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_PARTYATTR_REQ));
        auto a = DecodeAttrReq(body);
        EXPECT(a.char_id == 42);
        EXPECT(a.party_type == party::kObtainHunter);
        EXPECT(a.chief_id == 42);
        pid = a.party_id;
        EXPECT(pid != 0);
    }
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_PARTYJOIN_REQ));
        auto j = DecodeJoinReq(body);
        EXPECT(j.recipient == 42);
        EXPECT(j.member_id == 200);            // describes Bob
        EXPECT(j.member_name == "Bob");
        EXPECT(j.chief_id == 42);
        EXPECT(j.party_id == pid);
        EXPECT(j.obtain == party::kObtainHunter);
        EXPECT(j.level == 20);
        EXPECT(j.max_hp == 1000);              // from the JOIN packet
        EXPECT(j.hp == 900);
        EXPECT(j.klass == 7);
        EXPECT(j.guild_name.empty());          // NAME_NULL
    }
    // peer2 (Bob): JOIN_REQ describing Alice then PARTYATTR(Bob).
    {
        auto [w, body] = ReadFramed(peer2);
        EXPECT(w == ToUint16(MessageId::MW_PARTYJOIN_REQ));
        auto j = DecodeJoinReq(body);
        EXPECT(j.recipient == 200);
        EXPECT(j.member_id == 42);             // describes Alice
        EXPECT(j.member_name == "Alice");
        EXPECT(j.chief_id == 42);
        EXPECT(j.party_id == pid);
        EXPECT(j.level == 10);
        EXPECT(j.max_hp == 111);               // Alice's stashed stats
        EXPECT(j.race == 1);
        EXPECT(j.klass == 4);
    }
    {
        auto [w, body] = ReadFramed(peer2);
        EXPECT(w == ToUint16(MessageId::MW_PARTYATTR_REQ));
        auto a = DecodeAttrReq(body);
        EXPECT(a.char_id == 200);
        EXPECT(a.party_id == pid);
        EXPECT(a.chief_id == 42);
    }
    // Registry + back-pointers.
    EXPECT(parties.Size() == 1);
    {
        auto p = parties.Find(pid);
        EXPECT(p != nullptr);
        if (p)
        {
            std::lock_guard g(p->lock);
            EXPECT(p->Size() == 2);
            EXPECT(p->IsChief(42));
            EXPECT(p->IsMember(42));
            EXPECT(p->IsMember(200));
            EXPECT(p->obtain_type == party::kObtainHunter);
        }
    }
    with_char(42,  [&](tworldsvr::TChar& c) { EXPECT(c.party_id == pid); });
    with_char(200, [&](tworldsvr::TChar& c) {
        EXPECT(c.party_id == pid);
        EXPECT(c.max_hp == 1000);              // SetCharStatus stored
    });

    // --- Scenario D: Carol joins the existing party ------------------
    SendFramed(peer3, ToUint16(MessageId::MW_PARTYJOIN_ACK),
        JoinBody("Alice", "Carol", party::kObtainHunter, party::kAskYes,
                 2000, 1800, 700, 600));
    // peer3 (Carol): JOIN_REQ(Alice), JOIN_REQ(Bob), then PARTYATTR.
    {
        auto [w, body] = ReadFramed(peer3);
        EXPECT(w == ToUint16(MessageId::MW_PARTYJOIN_REQ));
        auto j = DecodeJoinReq(body);
        EXPECT(j.recipient == 400);
        EXPECT(j.member_id == 42);             // member order: Alice first
        EXPECT(j.party_id == pid);
        EXPECT(j.chief_id == 42);
    }
    {
        auto [w, body] = ReadFramed(peer3);
        EXPECT(w == ToUint16(MessageId::MW_PARTYJOIN_REQ));
        auto j = DecodeJoinReq(body);
        EXPECT(j.recipient == 400);
        EXPECT(j.member_id == 200);            // then Bob
        EXPECT(j.member_name == "Bob");
    }
    {
        auto [w, body] = ReadFramed(peer3);
        EXPECT(w == ToUint16(MessageId::MW_PARTYATTR_REQ));
        auto a = DecodeAttrReq(body);
        EXPECT(a.char_id == 400);
        EXPECT(a.party_id == pid);
    }
    // Alice + Bob each learn Carol.
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_PARTYJOIN_REQ));
        auto j = DecodeJoinReq(body);
        EXPECT(j.recipient == 42);
        EXPECT(j.member_id == 400);
        EXPECT(j.member_name == "Carol");
        EXPECT(j.level == 30);
        EXPECT(j.max_hp == 2000);
    }
    {
        auto [w, body] = ReadFramed(peer2);
        EXPECT(w == ToUint16(MessageId::MW_PARTYJOIN_REQ));
        auto j = DecodeJoinReq(body);
        EXPECT(j.recipient == 200);
        EXPECT(j.member_id == 400);
    }
    EXPECT(parties.Size() == 1);
    {
        auto p = parties.Find(pid);
        EXPECT(p != nullptr);
        if (p) { std::lock_guard g(p->lock); EXPECT(p->Size() == 3);
                 EXPECT(p->IsMember(400)); }
    }
    with_char(400, [&](tworldsvr::TChar& c) { EXPECT(c.party_id == pid); });

    peer1.close(); peer2.close(); peer3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_party_join_handlers "
                    "(4 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_party_join_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
