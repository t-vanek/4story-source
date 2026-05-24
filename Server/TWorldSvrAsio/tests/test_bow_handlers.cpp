// W6-24 wire test: Bow battleground — queue enqueue / cancel + the
// scoreboard points handler.
//
// One map peer (Alice's main, 0x42). Alice has country = TCONTRY_C
// (eligible). Test cases:
//
//   * Enqueue Alice → ADDTOBOWQUEUE_ACK(SUCCESS); BowRegistry now
//     holds her.
//   * Enqueue Alice again → ADDTOBOWQUEUE_ACK(ALREADYINQUEUE).
//   * Enqueue Bob (country = TCONTRY_B with aid = TCONTRY_PEACE) →
//     ADDTOBOWQUEUE_ACK(COUNTRY) (legacy rejects when neither
//     primary nor aid_country is D/C).
//   * Cancel Alice → CANCELBOWQUEUE_ACK(SUCCESS); registry empty.
//   * Cancel Alice again → CANCELBOWQUEUE_ACK(FAIL).
//   * Two BOWPOINTSUPDATE_REQ packets bump the per-country
//     scoreboard (no reply emitted; verified by reading the
//     registry directly).

#include "../handlers/handlers.h"
#include "../services/bow_constants.h"
#include "../services/bow_registry.h"
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

std::vector<std::byte> PointsBody(std::uint8_t country)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, country);
    return b;
}

struct QueueReply {
    std::uint8_t  result;
    std::uint32_t char_id;
    std::uint32_t key;
    std::uint32_t tick;
};

QueueReply ParseQueueReply(const std::vector<std::byte>& body)
{
    tworldsvr::wire::Reader r(body);
    QueueReply q{};
    r.Read(q.result);
    r.Read(q.char_id);
    r.Read(q.key);
    r.Read(q.tick);
    return q;
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
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.bow = &bow; ctx.nation = 0;

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

    // Alice: country=C (1), guild=10, tactics=0.
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, 0xA1));
    // Bob: country=B (2), aid=PEACE (4) — both > kCountryC → reject.
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(99, 0xCC));
    for (int i = 0; i < 200 && (!chars.Find(42) || !chars.Find(99)); ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(chars.Find(42) != nullptr);
    EXPECT(chars.Find(99) != nullptr);
    {
        auto a = chars.Find(42);
        std::lock_guard g(a->lock);
        a->country     = tworldsvr::bow::kCountryC;   // 1
        a->aid_country = tworldsvr::bow::kCountryC;
        a->guild_id    = 10;
    }
    {
        auto b = chars.Find(99);
        std::lock_guard g(b->lock);
        b->country     = 2;        // TCONTRY_B
        b->aid_country = 4;        // TCONTRY_PEACE — also > kCountryC
    }

    // --- Test A: enqueue Alice → SUCCESS ----------------------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_ADDTOBOWQUEUE_REQ),
                   Idkey(42, 0xA1));
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_ADDTOBOWQUEUE_ACK));
        auto q = ParseQueueReply(b);
        EXPECT(q.result  == tworldsvr::bow::kSuccess);
        EXPECT(q.char_id == 42);
        EXPECT(q.key     == 0xA1);
        EXPECT(bow.Contains(42));
        EXPECT(bow.QueueSize() == 1);
    }

    // --- Test B: enqueue Alice again → ALREADYINQUEUE ---------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_ADDTOBOWQUEUE_REQ),
                   Idkey(42, 0xA1));
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_ADDTOBOWQUEUE_ACK));
        auto q = ParseQueueReply(b);
        EXPECT(q.result == tworldsvr::bow::kAlreadyInQueue);
        EXPECT(bow.QueueSize() == 1);
    }

    // --- Test C: enqueue Bob (B/PEACE) → COUNTRY --------------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_ADDTOBOWQUEUE_REQ),
                   Idkey(99, 0xCC));
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_ADDTOBOWQUEUE_ACK));
        auto q = ParseQueueReply(b);
        EXPECT(q.result == tworldsvr::bow::kCountry);
        EXPECT(!bow.Contains(99));
    }

    // --- Test D: cancel Alice → SUCCESS -----------------------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_CANCELBOWQUEUE_REQ),
                   Idkey(42, 0xA1));
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CANCELBOWQUEUE_ACK));
        auto q = ParseQueueReply(b);
        EXPECT(q.result == tworldsvr::bow::kSuccess);
        EXPECT(!bow.Contains(42));
        EXPECT(bow.QueueSize() == 0);
    }

    // --- Test E: cancel Alice again → FAIL --------------------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_CANCELBOWQUEUE_REQ),
                   Idkey(42, 0xA1));
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CANCELBOWQUEUE_ACK));
        auto q = ParseQueueReply(b);
        EXPECT(q.result == tworldsvr::bow::kFail);
    }

    // --- Test F: BOWPOINTSUPDATE bumps the scoreboard ---------------
    //  No reply emitted; verify directly. Send two updates for D + one
    //  for C and confirm the per-country counters.
    EXPECT(bow.Points(tworldsvr::bow::kCountryD) == 0);
    EXPECT(bow.Points(tworldsvr::bow::kCountryC) == 0);
    SendFramed(p1, ToUint16(MessageId::MW_BOWPOINTSUPDATE_REQ),
               PointsBody(tworldsvr::bow::kCountryD));
    SendFramed(p1, ToUint16(MessageId::MW_BOWPOINTSUPDATE_REQ),
               PointsBody(tworldsvr::bow::kCountryD));
    SendFramed(p1, ToUint16(MessageId::MW_BOWPOINTSUPDATE_REQ),
               PointsBody(tworldsvr::bow::kCountryC));
    // Send a follow-up enqueue + read its ACK to ensure handler queue
    // drained both packets in order.
    SendFramed(p1, ToUint16(MessageId::MW_ADDTOBOWQUEUE_REQ),
               Idkey(42, 0xA1));
    auto [w, b] = ReadFramed(p1);
    EXPECT(w == ToUint16(MessageId::MW_ADDTOBOWQUEUE_ACK));
    auto q = ParseQueueReply(b);
    EXPECT(q.result == tworldsvr::bow::kSuccess);
    EXPECT(bow.Points(tworldsvr::bow::kCountryD) == 2);
    EXPECT(bow.Points(tworldsvr::bow::kCountryC) == 1);

    p1.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_bow_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_bow_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
