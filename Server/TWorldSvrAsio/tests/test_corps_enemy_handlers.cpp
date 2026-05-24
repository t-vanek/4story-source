// W3c-7 wire test: the corps chief-to-chief relays (enemy-list
// family + CORPSHP) over a four-peer loopback session.
//
// Corps 500 = [party10 (Alice 42/peer1 chief, Bob 200/peer2),
// party20 (Carol 400/peer3 chief), party30 (Dan 600/peer4 chief)],
// commander/general = Alice.
//
// Scenarios:
//   1. Alice (chief) relays an ENEMYLIST payload → the OTHER squad
//      chiefs (Carol/p3, Dan/p4) receive MW_CORPSENEMYLIST_REQ with
//      the opaque tail intact + their own char_id/key swapped in;
//      Bob (non-chief) + Alice (own squad) get nothing.
//   2. Bob (non-chief) relays → dropped (verified by #3).
//   3. Alice relays again → chiefs get exactly this payload (proves
//      #2 sent nothing).
//   4. Alice relays a CORPSHP payload → forwarded under
//      MW_CORPSHP_REQ (per-message wID mapping).

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

// ACK body = [char_id][key][tail (a u32 marker)].
std::vector<std::byte> RelayBody(std::uint32_t char_id, std::uint32_t key,
                                  std::uint32_t tail)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, char_id);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, tail);
    return b;
}

struct Relayed { std::uint32_t char_id = 0, key = 0, tail = 0; };
Relayed DecodeRelay(const std::vector<std::byte>& b)
{
    Relayed x{};
    tworldsvr::wire::Reader r(b);
    r.Read(x.char_id); r.Read(x.key); r.Read(x.tail);
    return x;
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
    svr_cfg.port = 0; svr_cfg.max_connections = 8; svr_cfg.ctx = ctx;
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

    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, 0xA1));
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(200, 0xB0));
    SendFramed(p3, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(400, 0xCA));
    SendFramed(p4, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(600, 0xDD));
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
                  std::vector<std::uint32_t> mem) {
        auto p = std::make_shared<tworldsvr::TParty>();
        p->id = pid; p->chief_char_id = chief;
        for (auto m : mem) p->AddMember(m);
        p->corps_id = 500;
        EXPECT(parties.Insert(p));
    };
    set_party(42, 10); set_party(200, 10); set_party(400, 20);
    set_party(600, 30);
    mk(10, 42, {42, 200}); mk(20, 400, {400}); mk(30, 600, {600});
    {
        auto c = std::make_shared<tworldsvr::TCorps>();
        c->id = 500; c->commander_party_id = 10; c->general_char_id = 42;
        c->AddParty(10); c->AddParty(20); c->AddParty(30);
        EXPECT(corps_reg.Insert(c));
    }

    const std::uint16_t kEnemyAck =
        ToUint16(MessageId::MW_CORPSENEMYLIST_ACK);
    const std::uint16_t kEnemyReq =
        ToUint16(MessageId::MW_CORPSENEMYLIST_REQ);
    const std::uint16_t kHpAck = ToUint16(MessageId::MW_CORPSHP_ACK);
    const std::uint16_t kHpReq = ToUint16(MessageId::MW_CORPSHP_REQ);

    // --- Scenario 1: Alice relays an ENEMYLIST payload --------------
    SendFramed(p1, kEnemyAck, RelayBody(42, 0xA1, 0x11111111));
    // squads iterate [10,20,30]; skip 10 → Carol(p3), Dan(p4).
    {
        auto [w, b] = ReadFramed(p3);
        EXPECT(w == kEnemyReq);
        auto x = DecodeRelay(b);
        EXPECT(x.char_id == 400);          // recipient swapped in
        EXPECT(x.key == 0xCA);
        EXPECT(x.tail == 0x11111111);      // payload intact
    }
    {
        auto [w, b] = ReadFramed(p4);
        EXPECT(w == kEnemyReq);
        auto x = DecodeRelay(b);
        EXPECT(x.char_id == 600); EXPECT(x.key == 0xDD);
        EXPECT(x.tail == 0x11111111);
    }

    // --- Scenario 2: Bob (non-chief) relays → dropped ---------------
    SendFramed(p2, kEnemyAck, RelayBody(200, 0xB0, 0x22222222));

    // --- Scenario 3: Alice relays again → proves #2 sent nothing ----
    SendFramed(p1, kEnemyAck, RelayBody(42, 0xA1, 0x33333333));
    {
        auto [w, b] = ReadFramed(p3);
        EXPECT(w == kEnemyReq);
        EXPECT(DecodeRelay(b).tail == 0x33333333);  // not 0x2222...
    }
    {
        auto [w, b] = ReadFramed(p4);
        EXPECT(w == kEnemyReq);
        EXPECT(DecodeRelay(b).tail == 0x33333333);
    }

    // --- Scenario 4: CORPSHP payload → forwarded under HP_REQ -------
    SendFramed(p1, kHpAck, RelayBody(42, 0xA1, 0x44444444));
    {
        auto [w, b] = ReadFramed(p3);
        EXPECT(w == kHpReq);
        EXPECT(DecodeRelay(b).tail == 0x44444444);
    }
    {
        auto [w, b] = ReadFramed(p4);
        EXPECT(w == kHpReq);
        EXPECT(DecodeRelay(b).tail == 0x44444444);
    }

    p1.close(); p2.close(); p3.close(); p4.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_corps_enemy_handlers "
                    "(4 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_corps_enemy_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
