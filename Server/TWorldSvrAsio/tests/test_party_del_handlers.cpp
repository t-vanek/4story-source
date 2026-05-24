// W3b-3 wire test: MW_PARTYDEL_ACK leave / kick.
//
// Four map peers hosting a 4-member party that is set up directly
// in the registry (this test exercises LeaveParty, not formation):
//   Alice 42  / peer1 0x42  (chief)
//   Bob   200 / peer2 0x43
//   Carol 400 / peer3 0x44
//   Dave  600 / peer4 0x45
//
// Scenarios (progressive, on the same party id=100):
//   A. Chief Alice leaves (4 members) → survives, succession to
//      Bob; every member gets the new-chief PARTYATTR then the
//      DEL, the leaver gets a cleared PARTYATTR.
//   B. Carol is kicked (3 members, non-chief) → survives, no
//      succession; kick flag propagates.
//   C. Chief Bob leaves (2 members) → disband: both Bob and Dave
//      are pulled out and the party drops from the registry.

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

std::vector<std::byte> DelBody(std::uint16_t party_id, std::uint32_t char_id,
                                std::uint8_t kick)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint16_t>(b, party_id);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, char_id);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, kick);
    return b;
}

struct DelReq {
    std::uint32_t recipient = 0, recipient_key = 0, leaver = 0, chief = 0;
    std::uint16_t commander = 0, party_id = 0;
    std::uint8_t  kick = 0;
};
DelReq DecodeDelReq(const std::vector<std::byte>& body)
{
    DelReq d{};
    tworldsvr::wire::Reader r(body);
    r.Read(d.recipient); r.Read(d.recipient_key); r.Read(d.leaver);
    r.Read(d.chief); r.Read(d.commander); r.Read(d.party_id); r.Read(d.kick);
    return d;
}

struct AttrReq { std::uint32_t char_id = 0, key = 0; std::uint16_t party_id = 0;
                 std::uint8_t party_type = 0; std::uint32_t chief = 0;
                 std::uint16_t commander = 0; };
AttrReq DecodeAttrReq(const std::vector<std::byte>& body)
{
    AttrReq a{};
    tworldsvr::wire::Reader r(body);
    r.Read(a.char_id); r.Read(a.key); r.Read(a.party_id);
    r.Read(a.party_type); r.Read(a.chief); r.Read(a.commander);
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
    svr_cfg.max_connections = 8;
    svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket p1(client_io), p2(client_io), p3(client_io), p4(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep); p2.connect(ep); p3.connect(ep); p4.connect(ep);
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
    reg(p4, 0x0045); drain(p1); drain(p2); drain(p3);
    EXPECT(peers.Size() == 4);

    struct C { std::uint32_t id; std::uint32_t key; tcp::socket* sock; };
    C alice{42, 0xA1, &p1}, bob{200, 0xB0, &p2}, carol{400, 0xCA, &p3},
      dave{600, 0xDA, &p4};
    for (auto* c : {&alice, &bob, &carol, &dave})
        SendFramed(*c->sock, ToUint16(MessageId::MW_ADDCHAR_ACK),
            AddCharBody(c->id, c->key));

    for (int i = 0; i < 100; ++i)
    {
        if (chars.Find(42) && chars.Find(200) && chars.Find(400) &&
            chars.Find(600)) break;
        std::this_thread::sleep_for(10ms);
    }
    EXPECT(chars.Find(42) && chars.Find(200) && chars.Find(400) &&
           chars.Find(600));

    // Build the party directly: chief Alice, member order A,B,C,D.
    constexpr std::uint16_t kPid = 100;
    {
        auto pty = std::make_shared<tworldsvr::TParty>();
        pty->id            = kPid;
        pty->obtain_type   = party::kObtainHunter;
        pty->chief_char_id = 42;
        for (std::uint32_t m : {42u, 200u, 400u, 600u}) pty->AddMember(m);
        EXPECT(parties.Insert(pty));
    }
    auto set_pid = [&](std::uint32_t id) {
        auto c = chars.Find(id);
        if (c) { std::lock_guard g(c->lock); c->party_id = kPid; }
    };
    set_pid(42); set_pid(200); set_pid(400); set_pid(600);

    auto party_id_of = [&](std::uint32_t id) -> std::uint16_t {
        auto c = chars.Find(id);
        if (!c) return 0xFFFF;
        std::lock_guard g(c->lock);
        return c->party_id;
    };
    auto poll = [&](auto pred) {
        for (int i = 0; i < 200; ++i)
        { if (pred()) return true; std::this_thread::sleep_for(5ms); }
        return pred();
    };

    // --- Scenario A: chief Alice leaves → succession to Bob ----------
    SendFramed(p1, ToUint16(MessageId::MW_PARTYDEL_ACK),
        DelBody(kPid, 42, /*kick=*/0));
    // peer1 (Alice/leaver): ATTR(chief=Bob) → DEL(leaver=42,chief=0) → ATTR(0)
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_PARTYATTR_REQ));
        auto a = DecodeAttrReq(b);
        EXPECT(a.char_id == 42); EXPECT(a.chief == 200);
        EXPECT(a.party_id == kPid);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_PARTYDEL_REQ));
        auto d = DecodeDelReq(b);
        EXPECT(d.recipient == 42); EXPECT(d.leaver == 42);
        EXPECT(d.chief == 0); EXPECT(d.party_id == 0); EXPECT(d.kick == 0);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_PARTYATTR_REQ));
        auto a = DecodeAttrReq(b);
        EXPECT(a.char_id == 42); EXPECT(a.party_id == 0); EXPECT(a.chief == 0);
    }
    // peer2 (Bob/new chief): ATTR(chief=Bob) → DEL(leaver=42,chief=Bob,pid)
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_PARTYATTR_REQ));
        auto a = DecodeAttrReq(b);
        EXPECT(a.char_id == 200); EXPECT(a.chief == 200);
    }
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_PARTYDEL_REQ));
        auto d = DecodeDelReq(b);
        EXPECT(d.recipient == 200); EXPECT(d.leaver == 42);
        EXPECT(d.chief == 200); EXPECT(d.party_id == kPid);
    }
    // peer3/peer4: ATTR(chief=Bob) → DEL(leaver=42,chief=Bob,pid)
    for (auto* s : {&p3, &p4})
    {
        { auto [w, b] = ReadFramed(*s);
          EXPECT(w == ToUint16(MessageId::MW_PARTYATTR_REQ));
          EXPECT(DecodeAttrReq(b).chief == 200); }
        { auto [w, b] = ReadFramed(*s);
          EXPECT(w == ToUint16(MessageId::MW_PARTYDEL_REQ));
          auto d = DecodeDelReq(b);
          EXPECT(d.leaver == 42); EXPECT(d.chief == 200);
          EXPECT(d.party_id == kPid); }
    }
    EXPECT(poll([&] { return party_id_of(42) == 0; }));
    {
        auto p = parties.Find(kPid);
        EXPECT(p != nullptr);
        if (p) { std::lock_guard g(p->lock);
                 EXPECT(p->Size() == 3); EXPECT(p->IsChief(200));
                 EXPECT(!p->IsMember(42)); }
    }

    // --- Scenario B: Carol kicked (non-chief) → survives -------------
    SendFramed(p3, ToUint16(MessageId::MW_PARTYDEL_ACK),
        DelBody(kPid, 400, /*kick=*/1));
    // peer3 (Carol/leaver): DEL(leaver=400,chief=0,kick=1) → ATTR(0)
    {
        auto [w, b] = ReadFramed(p3);
        EXPECT(w == ToUint16(MessageId::MW_PARTYDEL_REQ));
        auto d = DecodeDelReq(b);
        EXPECT(d.recipient == 400); EXPECT(d.leaver == 400);
        EXPECT(d.chief == 0); EXPECT(d.party_id == 0); EXPECT(d.kick == 1);
    }
    {
        auto [w, b] = ReadFramed(p3);
        EXPECT(w == ToUint16(MessageId::MW_PARTYATTR_REQ));
        EXPECT(DecodeAttrReq(b).party_id == 0);
    }
    // peer2 (Bob) + peer4 (Dave): DEL(leaver=400,chief=Bob,pid,kick=1)
    for (auto* s : {&p2, &p4})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_PARTYDEL_REQ));
        auto d = DecodeDelReq(b);
        EXPECT(d.leaver == 400); EXPECT(d.chief == 200);
        EXPECT(d.party_id == kPid); EXPECT(d.kick == 1);
    }
    EXPECT(poll([&] { return party_id_of(400) == 0; }));
    {
        auto p = parties.Find(kPid);
        EXPECT(p != nullptr);
        if (p) { std::lock_guard g(p->lock);
                 EXPECT(p->Size() == 2); EXPECT(!p->IsMember(400)); }
    }

    // --- Scenario C: chief Bob leaves (2 members) → disband ----------
    SendFramed(p2, ToUint16(MessageId::MW_PARTYDEL_ACK),
        DelBody(kPid, 200, /*kick=*/0));
    // peer2 (Bob/leaver): DEL(leaver=200,chief=0) → ATTR(0)
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_PARTYDEL_REQ));
        auto d = DecodeDelReq(b);
        EXPECT(d.recipient == 200); EXPECT(d.leaver == 200);
        EXPECT(d.chief == 0); EXPECT(d.party_id == 0);
    }
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_PARTYATTR_REQ));
        EXPECT(DecodeAttrReq(b).party_id == 0);
    }
    // peer4 (Dave): DEL(leaver=200) → DEL(leaver=600) [cascade] → ATTR(0)
    {
        auto [w, b] = ReadFramed(p4);
        EXPECT(w == ToUint16(MessageId::MW_PARTYDEL_REQ));
        auto d = DecodeDelReq(b);
        EXPECT(d.recipient == 600); EXPECT(d.leaver == 200);
        EXPECT(d.chief == 0); EXPECT(d.party_id == 0);
    }
    {
        auto [w, b] = ReadFramed(p4);
        EXPECT(w == ToUint16(MessageId::MW_PARTYDEL_REQ));
        auto d = DecodeDelReq(b);
        EXPECT(d.recipient == 600); EXPECT(d.leaver == 600);
        EXPECT(d.chief == 0); EXPECT(d.party_id == 0);
    }
    {
        auto [w, b] = ReadFramed(p4);
        EXPECT(w == ToUint16(MessageId::MW_PARTYATTR_REQ));
        EXPECT(DecodeAttrReq(b).party_id == 0);
    }
    EXPECT(poll([&] { return parties.Find(kPid) == nullptr; }));
    EXPECT(poll([&] {
        return party_id_of(200) == 0 && party_id_of(600) == 0; }));
    EXPECT(parties.Size() == 0);

    p1.close(); p2.close(); p3.close(); p4.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_party_del_handlers "
                    "(3 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_party_del_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
