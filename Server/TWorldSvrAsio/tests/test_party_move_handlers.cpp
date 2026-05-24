// W3c-5 wire test: MW_PARTYMOVE_ACK corps squad reshuffle.
//
// Alice 42/peer1 0x42 is the "general" (the actor); every other
// char is on peer2 0x43 so the reshuffle broadcasts land there and
// peer1 receives only the MW_PARTYMOVE_REQ result.
//
// Parties: 10 = [Bob 200, Dan 600, Frank 800] (chief 200);
//          20 = [Carol 400, Eve 700] (chief 400);
//          99 = [Solo 900] (chief 900).
//
// Scenarios (reply read on peer1):
//   1. key mismatch                  → CORPS_NOT_COMMANDER
//   2. target not in a party (Alice) → CORPS_NOT_COMMANDER
//   3. move to the target's own party→ CORPS_NOT_COMMANDER
//   4. swap against a size-1 party    → CORPS_WRONG_TARGET
//   5. move Bob from party 10 → 20   → CORPS_SUCCESS + registry

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/corps_constants.h"
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

std::vector<std::byte> MoveBody(std::uint32_t char_id, std::uint32_t key,
                                 const std::string& target,
                                 const std::string& dest,
                                 std::uint16_t target_party)
{
    std::vector<std::byte> b;
    using namespace tworldsvr::wire;
    WritePOD<std::uint32_t>(b, char_id);
    WritePOD<std::uint32_t>(b, key);
    WriteString(b, target);
    WriteString(b, dest);
    WritePOD<std::uint16_t>(b, target_party);
    return b;
}

std::uint8_t ReadResult(boost::asio::ip::tcp::socket& s)
{
    auto [w, b] = ReadFramed(s);
    EXPECT(w == tnetlib::protocol::ToUint16(
        tnetlib::protocol::MessageId::MW_PARTYMOVE_REQ));
    tworldsvr::wire::Reader r(b);
    std::uint32_t cid = 0, key = 0; std::uint8_t result = 0;
    r.Read(cid); r.Read(key); r.Read(result);
    EXPECT(cid == 42);
    return result;
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
    { auto [w, _] = ReadFramed(p1);   // RELAYCONNECT broadcast from p2 reg
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, 0xA1));
    for (std::uint32_t id : {200u, 600u, 800u, 400u, 700u, 900u})
        SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK),
            AddCharBody(id, id));
    for (int i = 0; i < 100; ++i)
    {
        if (chars.Find(42) && chars.Find(900)) break;
        std::this_thread::sleep_for(10ms);
    }
    EXPECT(chars.Find(42) && chars.Find(200) && chars.Find(900));

    auto name_party = [&](std::uint32_t id, const std::string& n,
                          std::uint16_t pid) {
        chars.Rename(id, n);
        auto c = chars.Find(id);
        if (c) { std::lock_guard g(c->lock); c->party_id = pid; }
    };
    chars.Rename(42, "Alice");          // general, party-less
    name_party(200, "Bob",   10);
    name_party(600, "Dan",   10);
    name_party(800, "Frank", 10);
    name_party(400, "Carol", 20);
    name_party(700, "Eve",   20);
    name_party(900, "Solo",  99);
    auto mk = [&](std::uint16_t pid, std::uint32_t chief,
                  std::vector<std::uint32_t> mem) {
        auto p = std::make_shared<tworldsvr::TParty>();
        p->id = pid; p->chief_char_id = chief;
        for (auto m : mem) p->AddMember(m);
        EXPECT(parties.Insert(p));
    };
    mk(10, 200, {200, 600, 800});
    mk(20, 400, {400, 700});
    mk(99, 900, {900});

    auto poll = [&](auto pred) {
        for (int i = 0; i < 200; ++i)
        { if (pred()) return true; std::this_thread::sleep_for(5ms); }
        return pred();
    };
    auto party_of = [&](std::uint32_t id) -> std::uint16_t {
        auto c = chars.Find(id);
        if (!c) return 0xFFFF;
        std::lock_guard g(c->lock);
        return c->party_id;
    };
    auto in_party = [&](std::uint16_t pid, std::uint32_t cid) {
        auto p = parties.Find(pid);
        if (!p) return false;
        std::lock_guard g(p->lock);
        return p->IsMember(cid);
    };

    // --- Scenario 1: key mismatch → NOT_COMMANDER -------------------
    SendFramed(p1, ToUint16(MessageId::MW_PARTYMOVE_ACK),
        MoveBody(42, 0xBADBAD, "Bob", "", 20));
    EXPECT(ReadResult(p1) == corps::kNotCommander);

    // --- Scenario 2: target has no party → NOT_COMMANDER ------------
    SendFramed(p1, ToUint16(MessageId::MW_PARTYMOVE_ACK),
        MoveBody(42, 0xA1, "Alice", "", 20));
    EXPECT(ReadResult(p1) == corps::kNotCommander);

    // --- Scenario 3: move to the target's own party → NOT_COMMANDER -
    SendFramed(p1, ToUint16(MessageId::MW_PARTYMOVE_ACK),
        MoveBody(42, 0xA1, "Bob", "", 10));
    EXPECT(ReadResult(p1) == corps::kNotCommander);

    // --- Scenario 4: swap against a size-1 party → WRONG_TARGET -----
    SendFramed(p1, ToUint16(MessageId::MW_PARTYMOVE_ACK),
        MoveBody(42, 0xA1, "Bob", "Solo", 0));
    EXPECT(ReadResult(p1) == corps::kWrongTarget);
    EXPECT(party_of(200) == 10);   // unchanged

    // --- Scenario 5: move Bob from party 10 → 20 → SUCCESS ----------
    SendFramed(p1, ToUint16(MessageId::MW_PARTYMOVE_ACK),
        MoveBody(42, 0xA1, "Bob", "", 20));
    EXPECT(ReadResult(p1) == corps::kSuccess);
    EXPECT(poll([&] { return party_of(200) == 20; }));
    EXPECT(poll([&] { return in_party(20, 200) && !in_party(10, 200); }));
    // party 10 survived (Dan, Frank remain) with chief succession.
    {
        auto p = parties.Find(10);
        EXPECT(p != nullptr);
        if (p) { std::lock_guard g(p->lock);
                 EXPECT(p->Size() == 2); EXPECT(!p->IsChief(200)); }
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_party_move_handlers "
                    "(5 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_party_move_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
