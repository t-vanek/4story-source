// W6-25 wire test: Battle Royale — queue + premade-team flow + map
// vote. Two peers: p1 (Alice's main = 0x42, also Carol's), p2 (Bob's
// main = 0x43). Alice is the chief / inviter, Bob the invited mate,
// Carol the solo unknown-target case.
//
// Test cases:
//   * Enqueue Alice → ADDTOBRQUEUE_ACK(SUCCESS); queue size = 1.
//   * Enqueue Alice again → ALREADYINQUEUE.
//   * BRTEAMMATEADD from Alice → forwarded as SUCCESS dialog on p2.
//   * BRTEAMMATEADD with self / unknown name → NOTFOUND on p1.
//   * BRTEAMMATEADDRESULT(SUCCESS) from Bob → JoinPremadeTeam +
//     UPDATEBRTEAM broadcast on both p1 and p2.
//   * Ready signal from Bob (only_ready=1, mate) →
//     FlagPlayerReady + UPDATEBRTEAM (Bob ready, team not yet ready).
//   * Ready signal from Alice (only_ready=1, chief) →
//     FlagTeamReady + UPDATEBRTEAM (team_ready=1).
//   * BRTEAMMATEDEL by chief drops the mate.
//   * VOTEFORBRMAP (map name set) records the user's vote;
//     same call with empty map + valid mode records the mode vote.
//   * BR fall-through in W6-24's OnCancelBowQueueReq removes the
//     player from the BR queue when not in the Bow queue.

#include "../handlers/handlers.h"
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

std::vector<std::byte> AddCharBody(std::uint32_t char_id, std::uint32_t key,
                                   std::uint32_t user_id = 100)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, 0x7f000001);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, 33500);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, user_id);
    return b;
}

std::vector<std::byte> AddToBrQueueBody(std::uint32_t char_id, std::uint32_t key,
                                        std::uint8_t only_ready)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, only_ready);
    return b;
}

std::vector<std::byte> NameBody(std::uint32_t char_id, std::uint32_t key,
                                const std::string& name)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WriteString(b, name);
    return b;
}

std::vector<std::byte> ResultBody(std::uint32_t char_id, std::uint32_t key,
                                  std::uint8_t result, const std::string& name)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, result);
    tworldsvr::wire::WriteString(b, name);
    return b;
}

std::vector<std::byte> VoteBody(std::uint32_t char_id, std::uint32_t key,
                                const std::string& map, std::uint8_t mode)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WriteString(b, map);
    tworldsvr::wire::WritePOD(b, mode);
    return b;
}

struct AddAck { std::uint8_t result; std::uint32_t char_id, key, tick; };

AddAck ParseAddAck(const std::vector<std::byte>& b)
{
    tworldsvr::wire::Reader r(b);
    AddAck a{};
    r.Read(a.result); r.Read(a.char_id); r.Read(a.key); r.Read(a.tick);
    return a;
}

struct MateAck { std::uint8_t result; std::uint32_t char_id, key;
                 std::string name; };

MateAck ParseMateAck(const std::vector<std::byte>& b)
{
    tworldsvr::wire::Reader r(b);
    MateAck m{};
    r.Read(m.result); r.Read(m.char_id); r.Read(m.key);
    r.ReadString(m.name);
    return m;
}

struct TeamRow { std::uint32_t id; std::string name; std::uint8_t ready; };
struct TeamAck {
    std::uint32_t char_id, key;
    std::string   chief;
    std::uint8_t  team_ready;
    std::vector<TeamRow> rows;
};

TeamAck ParseTeamAck(const std::vector<std::byte>& b)
{
    tworldsvr::wire::Reader r(b);
    TeamAck t{};
    r.Read(t.char_id); r.Read(t.key);
    r.ReadString(t.chief);
    r.Read(t.team_ready);
    std::uint8_t n = 0; r.Read(n);
    for (std::uint8_t i = 0; i < n; ++i)
    {
        TeamRow row{};
        r.Read(row.id); r.ReadString(row.name); r.Read(row.ready);
        t.rows.push_back(row);
    }
    return t;
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
    tcp::socket p1(client_io), p2(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep); p2.connect(ep);
    std::this_thread::sleep_for(20ms);

    // p1=0x42 (Alice's main), p2=0x43 (Bob's main).
    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0043));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK),
               AddCharBody(42, 0xA1, /*user_id=*/1001));
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK),
               AddCharBody(200, 0xB0, /*user_id=*/2002));
    for (int i = 0; i < 200 && (!chars.Find(42) || !chars.Find(200)); ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(chars.Find(42) != nullptr);
    EXPECT(chars.Find(200) != nullptr);
    EXPECT(chars.Rename(42, "Alice"));
    EXPECT(chars.Rename(200, "Bob"));
    {
        auto a = chars.Find(42);
        std::lock_guard g(a->lock);
        a->name = "Alice"; a->klass = 1;
    }
    {
        auto b = chars.Find(200);
        std::lock_guard g(b->lock);
        b->name = "Bob"; b->klass = 2;
    }

    // --- Test A: enqueue Alice → SUCCESS ----------------------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_ADDTOBRQUEUE_REQ),
                   AddToBrQueueBody(42, 0xA1, /*only_ready=*/0));
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_ADDTOBRQUEUE_ACK));
        auto a = ParseAddAck(b);
        EXPECT(a.result == tworldsvr::br::kSuccess);
        EXPECT(a.char_id == 42);
        EXPECT(br.QueueSize() == 1);
    }

    // --- Test B: enqueue Alice again → ALREADYINQUEUE --------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_ADDTOBRQUEUE_REQ),
                   AddToBrQueueBody(42, 0xA1, 0));
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_ADDTOBRQUEUE_ACK));
        EXPECT(ParseAddAck(b).result == tworldsvr::br::kAlreadyInQueue);
    }

    // --- Test C: BR fall-through from W6-24 Cancel Bow handler -----
    //  Alice isn't in the Bow queue, so OnCancelBowQueueReq's Bow
    //  remove returns FAIL; the W6-25 fall-through then erases her
    //  from the BR queue. Verifies the W6-24 hook lands.
    {
        std::vector<std::byte> cancel_body;
        tworldsvr::wire::WritePOD<std::uint32_t>(cancel_body, 42);
        tworldsvr::wire::WritePOD<std::uint32_t>(cancel_body, 0xA1);
        SendFramed(p1, ToUint16(MessageId::MW_CANCELBOWQUEUE_REQ),
                   cancel_body);
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CANCELBOWQUEUE_ACK));
        // Result byte first in the wire — SUCCESS now that the BR
        // fall-through removed Alice from the queue.
        tworldsvr::wire::Reader r(b);
        std::uint8_t  result = 0xFF;
        std::uint32_t cid = 0, key = 0, tick = 0;
        r.Read(result); r.Read(cid); r.Read(key); r.Read(tick);
        EXPECT(result == tworldsvr::br::kSuccess);
        EXPECT(cid == 42);
        EXPECT(br.QueueSize() == 0);
    }

    // --- Test D: invite Bob → forwarded SUCCESS on p2 --------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_BRTEAMMATEADD_REQ),
                   NameBody(42, 0xA1, "Bob"));
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_BRTEAMMATEADD_ACK));
        auto m = ParseMateAck(b);
        EXPECT(m.result == tworldsvr::br::kTeamAddSuccess);
        EXPECT(m.char_id == 200);          // recipient = Bob
        EXPECT(m.key == 0xB0);
        EXPECT(m.name == "Alice");         // inviter
    }

    // --- Test E: invite unknown / self → NOTFOUND on inviter -------
    {
        SendFramed(p1, ToUint16(MessageId::MW_BRTEAMMATEADD_REQ),
                   NameBody(42, 0xA1, "Zelda"));
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_BRTEAMMATEADD_ACK));
        EXPECT(ParseMateAck(b).result == tworldsvr::br::kTeamAddNotFound);

        SendFramed(p1, ToUint16(MessageId::MW_BRTEAMMATEADD_REQ),
                   NameBody(42, 0xA1, "Alice"));
        auto [w2, b2] = ReadFramed(p1);
        EXPECT(w2 == ToUint16(MessageId::MW_BRTEAMMATEADD_ACK));
        EXPECT(ParseMateAck(b2).result == tworldsvr::br::kTeamAddNotFound);
    }

    // --- Test F: Bob accepts → JoinPremadeTeam + UPDATEBRTEAM ------
    {
        SendFramed(p2, ToUint16(MessageId::MW_BRTEAMMATEADDRESULT_ACK),
                   ResultBody(200, 0xB0, tworldsvr::br::kTeamAddSuccess,
                              "Alice"));
        // Broadcast lands on both p1 (chief) and p2 (mate). Read
        // each — content identical except (char_id, key).
        auto [w1, b1] = ReadFramed(p1);
        EXPECT(w1 == ToUint16(MessageId::MW_UPDATEBRTEAM_ACK));
        auto t1 = ParseTeamAck(b1);
        EXPECT(t1.char_id == 42);
        EXPECT(t1.chief == "Alice");
        EXPECT(t1.team_ready == 0);
        EXPECT(t1.rows.size() == 2);

        auto [w2, b2] = ReadFramed(p2);
        EXPECT(w2 == ToUint16(MessageId::MW_UPDATEBRTEAM_ACK));
        auto t2 = ParseTeamAck(b2);
        EXPECT(t2.char_id == 200);
        EXPECT(t2.chief == "Alice");

        // Registry state.
        EXPECT(br.TeamCount() == 1);
        EXPECT(br.GetPremadePlayerCountByChief(42) == 2);
        EXPECT(br.FindPlayerInPremade(200));
    }

    // --- Test G: Bob signals ready (only_ready=1, mate) ------------
    {
        SendFramed(p2, ToUint16(MessageId::MW_ADDTOBRQUEUE_REQ),
                   AddToBrQueueBody(200, 0xB0, /*only_ready=*/1));
        auto [w1, b1] = ReadFramed(p1);
        EXPECT(w1 == ToUint16(MessageId::MW_UPDATEBRTEAM_ACK));
        auto t1 = ParseTeamAck(b1);
        EXPECT(t1.team_ready == 0);    // chief not signaled yet
        // Find Bob's row and check ready=1.
        bool bob_ready = false;
        for (const auto& row : t1.rows)
            if (row.id == 200) bob_ready = (row.ready == 1);
        EXPECT(bob_ready);
        auto [w2, _] = ReadFramed(p2);
        EXPECT(w2 == ToUint16(MessageId::MW_UPDATEBRTEAM_ACK));
    }

    // --- Test H: Alice signals ready (chief) → team_ready=1 --------
    {
        SendFramed(p1, ToUint16(MessageId::MW_ADDTOBRQUEUE_REQ),
                   AddToBrQueueBody(42, 0xA1, /*only_ready=*/1));
        auto [w1, b1] = ReadFramed(p1);
        EXPECT(w1 == ToUint16(MessageId::MW_UPDATEBRTEAM_ACK));
        auto t1 = ParseTeamAck(b1);
        EXPECT(t1.team_ready == 1);
        auto [w2, _] = ReadFramed(p2);
        EXPECT(w2 == ToUint16(MessageId::MW_UPDATEBRTEAM_ACK));
    }

    // --- Test I: chief drops mate (BRTEAMMATEDEL) → no reply -------
    {
        SendFramed(p1, ToUint16(MessageId::MW_BRTEAMMATEDEL_REQ),
                   NameBody(42, 0xA1, "Bob"));
        // Poll for registry state — no wire reply (legacy parity).
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        {
            if (!br.FindPlayerInPremade(200)) { ok = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(ok);
        // Alice's team still exists (chief stays after a mate drop).
        EXPECT(br.GetPremadePlayerCountByChief(42) == 1);
    }

    // --- Test J: vote for map / mode -------------------------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_VOTEFORBRMAP_REQ),
                   VoteBody(42, 0xA1, "ArenaMap", /*mode=*/0xFF));
        SendFramed(p2, ToUint16(MessageId::MW_VOTEFORBRMAP_REQ),
                   VoteBody(200, 0xB0, /*map=*/"", /*mode=*/0));
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        {
            if (br.MapVoteCount() == 1 && br.ModeVoteCount() == 1)
            { ok = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(ok);
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_br_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_br_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
