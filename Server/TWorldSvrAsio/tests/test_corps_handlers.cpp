// W3c-1 wire test: MW_CORPSASK_ACK corps invite-relay gate over a
// three-peer loopback session.
//
// Alice 42/peer1 0x42 (chief of party 100), Bob 200/peer2 0x43
// (chief of party 200), Carol 400/peer3 0x44 (member, not chief, of
// party 100). All same country.
//
// Scenarios:
//   1. Alice→Bob (both chiefs, no corps) → CORPSASK_REQ to Bob.
//   2. Alice→Ghost (unknown) → CORPSREPLY(WRONG_TARGET) to Alice.
//   3. Alice→Carol (not a chief) → CORPSREPLY(NO_PARTY).
//   4. Alice→Bob with Alice's party in arena → CORPSREPLY(BUSY).
//   5. Alice→Bob with Bob's party in a full (7-squad) corps →
//      CORPSREPLY(MAX_PARTY).
//   6. Alice→Bob with both parties already in a corps →
//      CORPSREPLY(WRONG_TARGET).

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

std::vector<std::byte> AskBody(std::uint32_t char_id, std::uint32_t key,
                                const std::string& target)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, char_id);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    tworldsvr::wire::WriteString(b, target);
    return b;
}

struct AskReq { std::uint32_t char_id = 0, key = 0, inviter = 0;
                std::string name; };
AskReq DecodeAskReq(const std::vector<std::byte>& b)
{
    AskReq a{};
    tworldsvr::wire::Reader r(b);
    r.Read(a.char_id); r.Read(a.key); r.Read(a.inviter); r.ReadString(a.name);
    return a;
}

struct ReplyReq { std::uint32_t char_id = 0, key = 0; std::uint8_t result = 0;
                  std::string name; };
ReplyReq DecodeReplyReq(const std::vector<std::byte>& b)
{
    ReplyReq a{};
    tworldsvr::wire::Reader r(b);
    r.Read(a.char_id); r.Read(a.key); r.Read(a.result); r.ReadString(a.name);
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

    auto set_char = [&](std::uint32_t id, const std::string& name,
                        std::uint16_t pid) {
        chars.Rename(id, name);   // updates the FindByName index
        auto c = chars.Find(id);
        if (c) { std::lock_guard g(c->lock); c->party_id = pid; }
    };
    set_char(42,  "Alice", 100);
    set_char(400, "Carol", 100);   // member of party 100, not chief
    set_char(200, "Bob",   200);
    {
        auto p100 = std::make_shared<tworldsvr::TParty>();
        p100->id = 100; p100->chief_char_id = 42;
        p100->AddMember(42); p100->AddMember(400);
        EXPECT(parties.Insert(p100));
        auto p200 = std::make_shared<tworldsvr::TParty>();
        p200->id = 200; p200->chief_char_id = 200;
        p200->AddMember(200);
        EXPECT(parties.Insert(p200));
    }
    auto set_party = [&](std::uint16_t pid, auto fn) {
        auto p = parties.Find(pid);
        if (p) { std::lock_guard g(p->lock); fn(*p); }
    };

    // --- Scenario 1: Alice→Bob success → CORPSASK_REQ to Bob --------
    SendFramed(p1, ToUint16(MessageId::MW_CORPSASK_ACK),
        AskBody(42, 0xA1, "Bob"));
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_CORPSASK_REQ));
        auto a = DecodeAskReq(b);
        EXPECT(a.char_id == 200); EXPECT(a.key == 0xB0);
        EXPECT(a.inviter == 42); EXPECT(a.name == "Alice");
    }

    // --- Scenario 2: unknown target → WRONG_TARGET ------------------
    SendFramed(p1, ToUint16(MessageId::MW_CORPSASK_ACK),
        AskBody(42, 0xA1, "Ghost"));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CORPSREPLY_REQ));
        auto a = DecodeReplyReq(b);
        EXPECT(a.char_id == 42); EXPECT(a.result == corps::kWrongTarget);
        EXPECT(a.name == "Ghost");
    }

    // --- Scenario 3: target not a chief → NO_PARTY ------------------
    SendFramed(p1, ToUint16(MessageId::MW_CORPSASK_ACK),
        AskBody(42, 0xA1, "Carol"));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CORPSREPLY_REQ));
        EXPECT(DecodeReplyReq(b).result == corps::kNoParty);
    }

    // --- Scenario 4: requester party in arena → BUSY ----------------
    set_party(100, [](tworldsvr::TParty& p) { p.arena = true; });
    SendFramed(p1, ToUint16(MessageId::MW_CORPSASK_ACK),
        AskBody(42, 0xA1, "Bob"));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CORPSREPLY_REQ));
        EXPECT(DecodeReplyReq(b).result == corps::kBusy);
    }
    set_party(100, [](tworldsvr::TParty& p) { p.arena = false; });

    // --- Scenario 5: target's corps is full → MAX_PARTY -------------
    {
        auto full = std::make_shared<tworldsvr::TCorps>();
        full->id = 500; full->commander_party_id = 200;
        for (std::uint16_t s : {200u, 11u, 12u, 13u, 14u, 15u, 16u})
            full->AddParty(s);          // 7 == MAX_CORPS_PARTY
        EXPECT(corps_reg.Insert(full));
    }
    set_party(200, [](tworldsvr::TParty& p) { p.corps_id = 500; });
    SendFramed(p1, ToUint16(MessageId::MW_CORPSASK_ACK),
        AskBody(42, 0xA1, "Bob"));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CORPSREPLY_REQ));
        EXPECT(DecodeReplyReq(b).result == corps::kMaxParty);
    }

    // --- Scenario 6: both parties already in a corps → WRONG_TARGET -
    set_party(100, [](tworldsvr::TParty& p) { p.corps_id = 501; });
    SendFramed(p1, ToUint16(MessageId::MW_CORPSASK_ACK),
        AskBody(42, 0xA1, "Bob"));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CORPSREPLY_REQ));
        EXPECT(DecodeReplyReq(b).result == corps::kWrongTarget);
    }

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_corps_handlers (6 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_corps_handlers (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
