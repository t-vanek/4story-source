// W6-26 wire test: MW_LEAVEBATTLEFIELD_REQ routing.
//
// One peer (p1 = svr 0x42). Four chars, each in a different state,
// exercise the legacy SSHandler.cpp:14125 routing branches:
//
//   * Char A — channel=BR_SERVER_ID, in BR solo queue. LEAVE drops
//     her from the queue.
//   * Char B — channel=BR_SERVER_ID, in a BR premade team. LEAVE
//     drops her from the team.
//   * Char C — map_id=BOW_MAP_ID, in Bow queue. LEAVE drops her.
//   * Char D — channel=0, map_id=0 (not on a battlefield). LEAVE
//     is a no-op; her state in BR + Bow stays put.

#include "../handlers/handlers.h"
#include "../services/bow_constants.h"
#include "../services/bow_registry.h"
#include "../services/br_constants.h"
#include "../services/br_registry.h"
#include "../services/char_registry.h"
#include "../services/guild_registry.h"
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
#include <mutex>
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

std::vector<std::byte> Idkey(std::uint32_t char_id, std::uint32_t key)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    return b;
}

// Polls the registries until each setup invariant holds — used to
// synchronise the test with the async handlers (no reply on the
// LEAVE wire, so we wait for state mutation).
template <class Pred>
bool WaitFor(Pred p)
{
    using namespace std::chrono_literals;
    for (int i = 0; i < 200; ++i)
    {
        if (p()) return true;
        std::this_thread::sleep_for(10ms);
    }
    return false;
}

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;

    boost::asio::io_context io;
    tworldsvr::CharRegistry  chars;
    tworldsvr::GuildRegistry guilds;
    tworldsvr::PeerRegistry  peers;
    tworldsvr::BowRegistry   bow;
    tworldsvr::BrRegistry    br;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.bow = &bow; ctx.br = &br; ctx.nation = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0; svr_cfg.max_connections = 4; svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket p1(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep);
    std::this_thread::sleep_for(20ms);

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }

    // ADDCHAR four chars on p1: A=10, B=20 (premade chief), C=30
    // (Bow queue), D=40 (off-battlefield baseline).
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(10, 0xAA));
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(20, 0xBB));
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(30, 0xCC));
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(40, 0xDD));
    EXPECT(WaitFor([&]{
        return chars.Find(10) && chars.Find(20) &&
               chars.Find(30) && chars.Find(40);
    }));

    // Place each char on the right battlefield location + seed the
    // matching registry state. Char A in BR solo queue.
    {
        auto a = chars.Find(10);
        std::lock_guard g(a->lock);
        a->channel = tworldsvr::br::kBrServerId;
    }
    EXPECT(br.AddPlayerToQueue(10, 0xAA, /*klass=*/1, "A")
           == tworldsvr::br::kSuccess);

    // Char B chief of a BR premade with a fake mate (id=200, key=0).
    // We don't need to register the mate as a char — the test only
    // checks the premade roster is emptied.
    {
        auto b = chars.Find(20);
        std::lock_guard g(b->lock);
        b->channel = tworldsvr::br::kBrServerId;
    }
    br.JoinPremadeTeam(20, 0xBB, /*klass=*/1, "B",
                       200, /*mate_key=*/0, /*klass=*/1, "Mate");
    EXPECT(br.GetPremadePlayerCountByChief(20) == 2);

    // Char C in Bow queue, on Bow map.
    {
        auto c = chars.Find(30);
        std::lock_guard g(c->lock);
        c->map_id  = tworldsvr::bow::kBowMapId;
        c->country = tworldsvr::bow::kCountryC;
    }
    EXPECT(bow.AddPlayer(30, 0xCC, tworldsvr::bow::kCountryC, /*guild=*/0)
           == tworldsvr::bow::kSuccess);

    // Char D off-battlefield (channel=0, map_id=0) but with state in
    // BOTH the BR queue and the Bow queue. LEAVE must NOT touch
    // either (handler's channel/map gate rejects the branch).
    EXPECT(br.AddPlayerToQueue(40, 0xDD, /*klass=*/1, "D")
           == tworldsvr::br::kSuccess);
    EXPECT(bow.AddPlayer(40, 0xDD, tworldsvr::bow::kCountryD, /*guild=*/0)
           == tworldsvr::bow::kSuccess);

    // --- Test A: BR queue cleanup -----------------------------------
    SendFramed(p1, ToUint16(MessageId::MW_LEAVEBATTLEFIELD_REQ),
               Idkey(10, 0xAA));
    EXPECT(WaitFor([&]{ return !br.FindPlayerInPremade(10) &&
                               br.QueueSize() == 1 /* only D left */; }));

    // --- Test B: BR premade cleanup (chief-leave dissolves) ---------
    SendFramed(p1, ToUint16(MessageId::MW_LEAVEBATTLEFIELD_REQ),
               Idkey(20, 0xBB));
    EXPECT(WaitFor([&]{ return br.GetPremadePlayerCountByChief(20) == 0; }));
    EXPECT(br.TeamCount() == 0);

    // --- Test C: Bow queue cleanup ----------------------------------
    SendFramed(p1, ToUint16(MessageId::MW_LEAVEBATTLEFIELD_REQ),
               Idkey(30, 0xCC));
    EXPECT(WaitFor([&]{ return !bow.Contains(30); }));

    // --- Test D: off-battlefield → no-op ---------------------------
    //   Char D stays in both BR and Bow queues after the LEAVE. We
    //   verify by sending the LEAVE and then a stimulus (Bow
    //   enqueue + read its ACK) and confirming the registries are
    //   unchanged.
    SendFramed(p1, ToUint16(MessageId::MW_LEAVEBATTLEFIELD_REQ),
               Idkey(40, 0xDD));
    // Send an ADDTOBOWQUEUE for Char A to give us a synchronisation
    // point — Char A was already in the BR queue and has now been
    // removed (Test A); enqueueing her in Bow gives us an ACK to
    // read. Her TChar.aid_country is 0 (= TCONTRY_D, eligible).
    SendFramed(p1, ToUint16(MessageId::MW_ADDTOBOWQUEUE_REQ),
               Idkey(10, 0xAA));
    auto [w, b] = ReadFramed(p1);
    EXPECT(w == ToUint16(MessageId::MW_ADDTOBOWQUEUE_ACK));
    // Char D's pre-LEAVE state is intact.
    EXPECT(br.QueueSize() == 1);     // only Char D
    EXPECT(bow.Contains(40));        // still in Bow queue

    p1.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_leavebattlefield_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_leavebattlefield_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
