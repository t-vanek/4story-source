// W6-49 wire tests: RPS win-ledger persistence.
//
//   Allowed win → TRPSGameRecord insert call (record=1, char, type,
//     win_count, now-ish date).
//   A stale seeded win date (> 30 days) → prune delete call
//     (record=0, char=0) alongside the fresh insert.
//   Hybrid DM_RPSGAMERECORD_REQ fan-in → direct repo call.
//   Boot-style hydrate: LoadGames + HydrateWinDate feed the cap so
//     a full ledger answers result=0 without new records.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/fake_rps_repository.h"
#include "../services/guild_registry.h"
#include "../services/peer_registry.h"
#include "../services/rps_registry.h"
#include "../wire_codec.h"
#include "../world_server.h"
#include "../world_session.h"

#include "MessageId.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <thread>
#include <utility>
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

std::vector<std::byte> RpsGameBody(std::uint32_t char_id,
                                   std::uint32_t key, std::uint8_t type,
                                   std::uint8_t win_count,
                                   std::uint8_t player_rps)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, type);
    tworldsvr::wire::WritePOD(b, win_count);
    tworldsvr::wire::WritePOD(b, player_rps);
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
    tworldsvr::CharRegistry     chars;
    tworldsvr::GuildRegistry    guilds;
    tworldsvr::PeerRegistry     peers;
    tworldsvr::CtrlSvrSlot      ctrl_svr;
    tworldsvr::RpsRegistry      rps;
    tworldsvr::FakeRpsRepository rps_repo;

    const std::int64_t now =
        static_cast<std::int64_t>(std::time(nullptr));

    // Boot-style hydrate through the repo shapes: one game with a
    // stale (36-day-old) win row.
    rps_repo.SeedGame(tworldsvr::TRpsGame{
        /*type=*/1, /*win_count=*/3,
        /*win_prob=*/40, /*draw_prob=*/30, /*lose_prob=*/30,
        /*win_keep=*/2, /*win_period=*/7, /*win_dates=*/{} });
    rps_repo.SeedWinDate({1, 3, now - 36LL * 24 * 3600});
    for (const auto& g : rps_repo.LoadGames())
        rps.Insert(g);
    for (const auto& d : rps_repo.LoadWinDates())
        EXPECT(rps.HydrateWinDate(d.type, d.win_count, d.date_unix));

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.ctrl_svr = &ctrl_svr;
    ctx.rps = &rps;
    ctx.rps_repo = &rps_repo;
    ctx.nation = 0;

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
    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0442));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }

    // Char 42 on p1.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 42);
        wire::WritePOD<std::uint32_t>(b, 0xA1);
        wire::WritePOD<std::uint32_t>(b, 0x7f000001);
        wire::WritePOD<std::uint16_t>(b, 33500);
        wire::WritePOD<std::uint32_t>(b, 500);
        SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), b);
    }
    for (int i = 0; i < 1000 && !chars.Find(42); ++i)
        std::this_thread::sleep_for(10ms);

    // --- W6-49a: allowed win → prune delete + fresh insert ----------
    SendFramed(p1, ToUint16(MessageId::MW_RPSGAME_ACK),
        RpsGameBody(42, 0xA1, 1, 3, /*player_rps=*/2));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_RPSGAME_REQ));
        wire::Reader r(b);
        std::uint32_t cid = 0, key = 0;
        std::uint8_t result = 0xFF, prps = 0;
        r.Read(cid); r.Read(key); r.Read(result); r.Read(prps);
        EXPECT(result == 1);            // allowed
    }
    for (int i = 0; i < 1000 && rps_repo.RecordCalls().size() < 2; ++i)
        std::this_thread::sleep_for(10ms);
    {
        const auto calls = rps_repo.RecordCalls();
        EXPECT(calls.size() == 2);
        if (calls.size() == 2)
        {
            // 36-day-old row pruned (record=0, char 0)…
            EXPECT(std::get<0>(calls[0]) == false);
            EXPECT(std::get<1>(calls[0]) == 0);
            EXPECT(std::get<2>(calls[0]) == 1);
            EXPECT(std::get<3>(calls[0]) == 3);
            // …then the fresh win recorded (record=1, char 42).
            EXPECT(std::get<0>(calls[1]) == true);
            EXPECT(std::get<1>(calls[1]) == 42);
            EXPECT(std::get<4>(calls[1]) >= now - 5);
        }
    }

    // --- W6-49b: cap reached → result 0, no new records -------------
    SendFramed(p1, ToUint16(MessageId::MW_RPSGAME_ACK),
        RpsGameBody(42, 0xA1, 1, 3, 2));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_RPSGAME_REQ));
    }
    SendFramed(p1, ToUint16(MessageId::MW_RPSGAME_ACK),
        RpsGameBody(42, 0xA1, 1, 3, 2));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_RPSGAME_REQ));
        wire::Reader r(b);
        std::uint32_t cid = 0, key = 0;
        std::uint8_t result = 0xFF, prps = 0;
        r.Read(cid); r.Read(key); r.Read(result); r.Read(prps);
        EXPECT(result == 0);            // win_keep=2 exhausted
    }
    std::this_thread::sleep_for(30ms);
    EXPECT(rps_repo.RecordCalls().size() == 3);   // only the 2nd win

    // --- W6-49c: hybrid DM record fan-in ----------------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 1);       // record
        wire::WritePOD<std::uint32_t>(b, 777);    // char
        wire::WritePOD<std::uint8_t>(b, 1);       // type
        wire::WritePOD<std::uint8_t>(b, 3);       // win_count
        wire::WritePOD<std::int64_t>(b, now);     // date
        SendFramed(p1, ToUint16(MessageId::DM_RPSGAMERECORD_REQ), b);
    }
    for (int i = 0; i < 1000 && rps_repo.RecordCalls().size() < 4; ++i)
        std::this_thread::sleep_for(10ms);
    {
        const auto calls = rps_repo.RecordCalls();
        EXPECT(calls.size() == 4);
        if (calls.size() == 4)
        {
            EXPECT(std::get<0>(calls[3]) == true);
            EXPECT(std::get<1>(calls[3]) == 777);
            EXPECT(std::get<4>(calls[3]) == now);
        }
    }

    p1.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_rps_persist_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_rps_persist_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
