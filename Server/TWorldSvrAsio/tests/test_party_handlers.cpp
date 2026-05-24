// W3b-1 wire test: MW_PARTYADD_ACK invite-relay gate.
//
// Two map peers: peer1 (wID 0x0042) hosts the inviter "Alice"
// (char 42); peer2 (wID 0x0043) hosts the invitee "Bob" (char
// 200). Every failure result is relayed back to the inviter's map
// (peer1, the originating socket); PARTY_AGREE is forwarded to the
// invitee's map (peer2) and flips Bob's party_waiter flag.
//
// Scenarios:
//   1. Unknown target            → PARTY_NOUSER  on peer1
//   2. Valid invite              → PARTY_AGREE   on peer2 +
//      Bob.party_waiter set + Alice combat stats stashed
//   3. Target mid-invite         → PARTY_WAITERS on peer1
//   4. Target already partied    → PARTY_ALREADY on peer1
//   5. War-country mismatch      → PARTY_COUNTRY on peer1
//   6. Inviter not party chief   → PARTY_NOTCHIEF on peer1
//   7. Inviter's party is full   → PARTY_FULL    on peer1

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

void SendFramed(boost::asio::ip::tcp::socket& sock,
                std::uint16_t wId,
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

std::vector<std::byte> PartyAddBody(const std::string& request_name,
                                     const std::string& target_name,
                                     std::uint8_t  obtain_type,
                                     std::uint32_t max_hp,
                                     std::uint32_t hp,
                                     std::uint32_t max_mp,
                                     std::uint32_t mp)
{
    std::vector<std::byte> b;
    using namespace tworldsvr::wire;
    WriteString(b, request_name);
    WriteString(b, target_name);
    WritePOD<std::uint8_t>(b, obtain_type);
    WritePOD<std::uint32_t>(b, max_hp);
    WritePOD<std::uint32_t>(b, hp);
    WritePOD<std::uint32_t>(b, max_mp);
    WritePOD<std::uint32_t>(b, mp);
    return b;
}

// Decoded MW_PARTYADD_REQ for assertions.
struct AddReq
{
    std::uint32_t char_id = 0;
    std::uint32_t key     = 0;
    std::string   request_name;
    std::string   target_name;
    std::uint8_t  obtain_type = 0;
    std::uint8_t  result      = 0;
    std::uint32_t request_char_id = 0;
};

AddReq DecodeAddReq(const std::vector<std::byte>& body)
{
    AddReq a;
    tworldsvr::wire::Reader r(body);
    r.Read(a.char_id);
    r.Read(a.key);
    r.ReadString(a.request_name);
    r.ReadString(a.target_name);
    r.Read(a.obtain_type);
    r.Read(a.result);
    r.Read(a.request_char_id);
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
    svr_cfg.max_connections = 4;
    svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket peer1(client_io);
    tcp::socket peer2(client_io);
    peer1.connect(tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port));
    peer2.connect(tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port));
    std::this_thread::sleep_for(20ms);

    SendFramed(peer1, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(peer1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(peer2, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0043));
    { auto [w, _] = ReadFramed(peer2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    EXPECT(peers.Size() == 2);

    // peer2's registration fans a cluster-wide RELAYCONNECT
    // broadcast out to the already-registered peer1. Drain it so it
    // doesn't desync peer1's reply stream below.
    { auto [w, _] = ReadFramed(peer1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    // Inviter Alice on peer1 (msi 0x42), invitee Bob on peer2 (0x43).
    SendFramed(peer1, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(42, 0xCAFEBABE));
    SendFramed(peer1, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        NameBody(42, 0xCAFEBABE, "Alice"));
    SendFramed(peer2, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(200, 0xD00D));
    SendFramed(peer2, ToUint16(MessageId::MW_CHANGECHARBASE_ACK),
        NameBody(200, 0xD00D, "Bob"));

    for (int i = 0; i < 100; ++i)
    {
        if (chars.FindByName("Alice") && chars.FindByName("Bob")) break;
        std::this_thread::sleep_for(10ms);
    }
    EXPECT(chars.FindByName("Alice") != nullptr);
    EXPECT(chars.FindByName("Bob")   != nullptr);

    // Helper to mutate a char's fields directly (test thread; the
    // server is idle between scenarios so this races nothing).
    auto with_char = [&](std::uint32_t id, auto fn) {
        auto c = chars.Find(id);
        EXPECT(c != nullptr);
        if (c) { std::lock_guard g(c->lock); fn(*c); }
    };

    // --- Scenario 1: unknown target → PARTY_NOUSER -------------------
    SendFramed(peer1, ToUint16(MessageId::MW_PARTYADD_ACK),
        PartyAddBody("Alice", "Ghost", party::kObtainFree, 1, 1, 1, 1));
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_PARTYADD_REQ));
        auto a = DecodeAddReq(body);
        EXPECT(a.char_id == 42);
        EXPECT(a.result == party::kNoUser);
        EXPECT(a.request_char_id == 0);
    }

    // --- Scenario 2: valid invite → PARTY_AGREE on peer2 -------------
    SendFramed(peer1, ToUint16(MessageId::MW_PARTYADD_ACK),
        PartyAddBody("Alice", "Bob", party::kObtainHunter,
                     1000, 900, 500, 400));
    {
        auto [w, body] = ReadFramed(peer2);
        EXPECT(w == ToUint16(MessageId::MW_PARTYADD_REQ));
        auto a = DecodeAddReq(body);
        EXPECT(a.char_id == 200);              // recipient = target
        EXPECT(a.key == 0xD00D);
        EXPECT(a.request_name == "Alice");
        EXPECT(a.target_name == "Bob");
        EXPECT(a.obtain_type == party::kObtainHunter);
        EXPECT(a.result == party::kAgree);
        EXPECT(a.request_char_id == 42);       // inviter id
    }
    // Bob flagged waiter; Alice's combat stats stashed.
    for (int i = 0; i < 50; ++i)
    {
        auto b = chars.Find(200);
        if (b) { std::lock_guard g(b->lock); if (b->party_waiter) break; }
        std::this_thread::sleep_for(10ms);
    }
    with_char(200, [](tworldsvr::TChar& c) { EXPECT(c.party_waiter); });
    with_char(42, [](tworldsvr::TChar& c) {
        EXPECT(c.max_hp == 1000);
        EXPECT(c.hp == 900);
        EXPECT(c.max_mp == 500);
        EXPECT(c.mp == 400);
    });

    // --- Scenario 3: target mid-invite → PARTY_WAITERS ---------------
    SendFramed(peer1, ToUint16(MessageId::MW_PARTYADD_ACK),
        PartyAddBody("Alice", "Bob", party::kObtainFree, 1, 1, 1, 1));
    {
        auto [w, body] = ReadFramed(peer1);
        EXPECT(w == ToUint16(MessageId::MW_PARTYADD_REQ));
        auto a = DecodeAddReq(body);
        EXPECT(a.char_id == 42);
        EXPECT(a.result == party::kWaiters);
    }
    with_char(200, [](tworldsvr::TChar& c) { c.party_waiter = false; });

    // --- Scenario 4: target already partied → PARTY_ALREADY ----------
    with_char(200, [](tworldsvr::TChar& c) { c.party_id = 5; });
    SendFramed(peer1, ToUint16(MessageId::MW_PARTYADD_ACK),
        PartyAddBody("Alice", "Bob", party::kObtainFree, 1, 1, 1, 1));
    {
        auto [w, body] = ReadFramed(peer1);
        auto a = DecodeAddReq(body);
        EXPECT(a.char_id == 42);
        EXPECT(a.result == party::kAlready);
    }
    with_char(200, [](tworldsvr::TChar& c) { c.party_id = 0; });

    // --- Scenario 5: war-country mismatch → PARTY_COUNTRY ------------
    with_char(200, [](tworldsvr::TChar& c) { c.aid_country = 1; });
    SendFramed(peer1, ToUint16(MessageId::MW_PARTYADD_ACK),
        PartyAddBody("Alice", "Bob", party::kObtainFree, 1, 1, 1, 1));
    {
        auto [w, body] = ReadFramed(peer1);
        auto a = DecodeAddReq(body);
        EXPECT(a.char_id == 42);
        EXPECT(a.result == party::kCountry);
    }
    with_char(200, [](tworldsvr::TChar& c) { c.aid_country = 0; });

    // --- Scenario 6: inviter not party chief → PARTY_NOTCHIEF --------
    {
        auto pty = std::make_shared<tworldsvr::TParty>();
        pty->id = 10;
        pty->chief_char_id = 999;          // someone other than Alice
        pty->AddMember(999);
        pty->AddMember(42);
        EXPECT(parties.Insert(pty));
    }
    with_char(42, [](tworldsvr::TChar& c) {
        c.party_id = 10; });
    SendFramed(peer1, ToUint16(MessageId::MW_PARTYADD_ACK),
        PartyAddBody("Alice", "Bob", party::kObtainFree, 1, 1, 1, 1));
    {
        auto [w, body] = ReadFramed(peer1);
        auto a = DecodeAddReq(body);
        EXPECT(a.char_id == 42);
        EXPECT(a.result == party::kNotChief);
    }

    // --- Scenario 7: inviter's party is full → PARTY_FULL ------------
    {
        auto pty = std::make_shared<tworldsvr::TParty>();
        pty->id = 11;
        pty->chief_char_id = 42;           // Alice is chief
        for (std::uint32_t m : {42u, 2u, 3u, 4u, 5u, 6u, 7u})
            pty->AddMember(m);             // 7 == MAX_PARTY_MEMBER
        EXPECT(pty->Size() == party::kMaxPartyMember);
        EXPECT(parties.Insert(pty));
    }
    with_char(42, [](tworldsvr::TChar& c) { c.party_id = 11; });
    SendFramed(peer1, ToUint16(MessageId::MW_PARTYADD_ACK),
        PartyAddBody("Alice", "Bob", party::kObtainFree, 1, 1, 1, 1));
    {
        auto [w, body] = ReadFramed(peer1);
        auto a = DecodeAddReq(body);
        EXPECT(a.char_id == 42);
        EXPECT(a.result == party::kFull);
    }

    peer1.close();
    peer2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_party_handlers (7 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_party_handlers (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
