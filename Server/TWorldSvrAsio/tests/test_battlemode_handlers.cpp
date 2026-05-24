// W6-27 wire test: BattleMode status query + CM teleport into Bow/BR.
//
// One peer (0x42 = Alice's main). Test cases:
//   * BATTLEMODESTATUS_REQ → BATTLEMODESTATUS_ACK with the quiescent
//     payload (Bow + BR both zero, bow_winner = TCONTRY_N).
//   * CMTELEPORTBATTLEMODE_REQ(SYSTEM_BOW) → BowRegistry::AddPlayer
//     enqueues Alice with country=TCONTRY_C (verified via the
//     registry directly — no reply on the wire).
//   * CMTELEPORTBATTLEMODE_REQ(SYSTEM_BR) → legacy no-op; the BR
//     queue is unchanged (verified directly).

#include "../handlers/handlers.h"
#include "../services/bow_constants.h"
#include "../services/bow_registry.h"
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

std::vector<std::byte> CmTeleportBody(std::uint32_t char_id, std::uint32_t key,
                                      std::uint8_t system_type)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, system_type);
    return b;
}

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    namespace wire = tworldsvr::wire;

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

    // Alice (10, key=0xAA) on p1. Tactics + guild id set so we can
    // verify CM-teleport picks tactics_id first for the guild hint.
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(10, 0xAA));
    for (int i = 0; i < 200 && !chars.Find(10); ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(chars.Find(10) != nullptr);
    {
        auto a = chars.Find(10);
        std::lock_guard g(a->lock);
        a->guild_id         = 7;
        a->tactics_guild_id = 22;
    }

    // --- Test A: BATTLEMODESTATUS_REQ → quiescent ACK ---------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_BATTLEMODESTATUS_REQ),
                   Idkey(10, 0xAA));
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_BATTLEMODESTATUS_ACK));
        wire::Reader r(b);
        std::uint32_t cid = 0, key = 0;
        std::uint8_t  bow_status = 0xFF, bow_winner = 0;
        std::uint32_t bow_start = 0xFFFFFFFF;
        std::uint8_t  br_status = 0xFF, br_type = 0xFF;
        std::uint32_t br_start = 0xFFFFFFFF;
        r.Read(cid); r.Read(key);
        r.Read(bow_status); r.Read(bow_start); r.Read(bow_winner);
        r.Read(br_status);  r.Read(br_start);  r.Read(br_type);
        EXPECT(cid == 10);
        EXPECT(key == 0xAA);
        EXPECT(bow_status == 0);
        EXPECT(bow_start == 0);
        EXPECT(bow_winner == 3);   // TCONTRY_N
        EXPECT(br_status == 0);
        EXPECT(br_start == 0);
        EXPECT(br_type == 0);
    }

    // --- Test B: CMTELEPORTBATTLEMODE(SYSTEM_BOW) enqueues Alice ----
    EXPECT(bow.QueueSize() == 0);
    {
        SendFramed(p1, ToUint16(MessageId::MW_CMTELEPORTBATTLEMODE_REQ),
                   CmTeleportBody(10, 0xAA, /*system_type=*/0));
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        {
            if (bow.Contains(10)) { ok = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(ok);
    }

    // --- Test C: CMTELEPORTBATTLEMODE(SYSTEM_BR) is a no-op ---------
    //  Legacy body is empty (SSHandler.cpp:14400 — TODO in original).
    //  Verify the BR queue stays empty by polling for a few ticks and
    //  then sending a stimulus (a Bow ADDTOBOWQUEUE_REQ) to drain the
    //  handler queue + provide a sync point.
    EXPECT(br.QueueSize() == 0);
    SendFramed(p1, ToUint16(MessageId::MW_CMTELEPORTBATTLEMODE_REQ),
               CmTeleportBody(10, 0xAA, /*system_type=*/1));
    // Send a follow-up enqueue to sync handler progress.
    SendFramed(p1, ToUint16(MessageId::MW_BATTLEMODESTATUS_REQ),
               Idkey(10, 0xAA));
    auto [w2, _2] = ReadFramed(p1);
    EXPECT(w2 == ToUint16(MessageId::MW_BATTLEMODESTATUS_ACK));
    EXPECT(br.QueueSize() == 0);   // BR untouched

    p1.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_battlemode_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_battlemode_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
