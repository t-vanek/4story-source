// W6-42 wire tests: MonthRank month-rollover chain.
//
// Seed two rankers (Alice warlord + slot2 after Bob displaces her),
// a guild with a non-zero month bank, then fire SM_MONTHRANKSAVE_REQ:
//   - guild pvp_month_point / rank_month zeroed,
//   - fake repo sees InitMonthRank(7), TSaveMonthRank rows with the
//     legacy total-rank indices (t=0 warlord Alice, t=1 Bob, t=2
//     Alice again — she is both the warlord row and a rank row),
//     InitMonthRank(8), InitMonthPvPoint(7, top_point=900),
//   - fame[0] = the repo-returned new top (Carol), fame[1..2] = Bob,
//     Alice; broadcasts MW_MONTHRANKRESET_REQ(month=7, 9 rankers) +
//     MW_FIRSTGRADEGROUP_REQ(month=7, 3×17) on every map,
//   - table slots 1..32 reset, warlord slot 0 SURVIVES, month -> 8.
// Failure path (fail on char 444): no broadcasts, month unchanged,
// table intact (sentinel-verified).

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/fake_month_rank_repository.h"
#include "../services/guild_registry.h"
#include "../services/month_rank_codec.h"
#include "../services/month_rank_registry.h"
#include "../services/peer_registry.h"
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
#include <memory>
#include <string>
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

tworldsvr::MonthRanker MakeRanker(std::uint32_t id, const char* name,
                                  std::uint32_t total_point,
                                  std::uint32_t month_point)
{
    tworldsvr::MonthRanker r{};
    r.char_id = id; r.name = name;
    r.total_point = total_point; r.month_point = month_point;
    r.month_win = 5; r.month_lose = 1;
    r.total_win = 50; r.total_lose = 10;
    r.country = 0; r.level = 90; r.klass = 2; r.race = 1;
    r.sex = 0; r.hair = 3; r.face = 4;
    r.say = "gg"; r.guild = "TestGuild";
    return r;
}

std::vector<std::byte>
UpdateBody(std::uint8_t month, std::uint8_t country,
           const tworldsvr::MonthRanker& r)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint8_t>(b, month);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, country);
    tworldsvr::WriteMonthRanker(b, r);
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
    tworldsvr::CharRegistry            chars;
    tworldsvr::GuildRegistry           guilds;
    tworldsvr::PeerRegistry            peers;
    tworldsvr::CtrlSvrSlot             ctrl_svr;
    tworldsvr::MonthRankRegistry       month_rank;
    tworldsvr::FakeMonthRankRepository rank_repo;
    month_rank.SetRankMonth(7);
    rank_repo.SetNewTop(MakeRanker(333, "Carol", 1200, 0));

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.ctrl_svr = &ctrl_svr;
    ctx.month_rank = &month_rank;
    ctx.month_rank_repo = &rank_repo;
    ctx.nation = 0;

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

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0442));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_MONTHRANKLIST_REQ)); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0443));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::MW_MONTHRANKLIST_REQ)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    // Seed rankers: Alice (900/40) then Bob (500/60) — Alice ends as
    // warlord + slot 2, Bob slot 1. Drain both UPDATE broadcasts.
    SendFramed(p1, ToUint16(MessageId::MW_MONTHRANKUPDATE_ACK),
        UpdateBody(7, 0, MakeRanker(111, "Alice", 900, 40)));
    for (auto* s : {&p1, &p2})
    { auto [w, _] = ReadFramed(*s);
      EXPECT(w == ToUint16(MessageId::MW_MONTHRANKUPDATE_REQ)); }
    SendFramed(p1, ToUint16(MessageId::MW_MONTHRANKUPDATE_ACK),
        UpdateBody(7, 0, MakeRanker(222, "Bob", 500, 60)));
    for (auto* s : {&p1, &p2})
    { auto [w, _] = ReadFramed(*s);
      EXPECT(w == ToUint16(MessageId::MW_MONTHRANKUPDATE_REQ)); }

    // Seed a guild with a live month bank.
    {
        auto g = std::make_shared<tworldsvr::TGuild>();
        g->id = 42; g->name = "Guild42";
        g->pvp_month_point = 77; g->rank_month = 3;
        guilds.Insert(g);
    }

    // --- W6-42a: successful rollover --------------------------------
    SendFramed(p1, ToUint16(MessageId::SM_MONTHRANKSAVE_REQ), {});

    // Both maps: MONTHRANKRESET (month=7, 9 rankers) then
    // FIRSTGRADEGROUP (month=7, 3×17).
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_MONTHRANKRESET_REQ));
        wire::Reader r(b);
        std::uint8_t month = 0, count = 0;
        r.Read(month); r.Read(count);
        EXPECT(month == 7);
        EXPECT(count == 9);
        tworldsvr::MonthRanker f0{}, f1{}, f2{};
        EXPECT(tworldsvr::ReadMonthRanker(r, f0));
        EXPECT(tworldsvr::ReadMonthRanker(r, f1));
        EXPECT(tworldsvr::ReadMonthRanker(r, f2));
        EXPECT(f0.char_id == 333);          // Carol from the repo
        EXPECT(f0.name == "Carol");
        EXPECT(f1.char_id == 222);          // Bob (month 60)
        EXPECT(f2.char_id == 111);          // Alice (month 40)
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_FIRSTGRADEGROUP_REQ));
        wire::Reader r(b);
        std::uint8_t month = 0, count = 0;
        r.Read(month); r.Read(count);
        EXPECT(month == 7);
        EXPECT(count == 17);
        tworldsvr::MonthRanker g0{}, g1{}, g2{};
        EXPECT(tworldsvr::ReadMonthRanker(r, g0));  // c0 slot0 warlord
        EXPECT(tworldsvr::ReadMonthRanker(r, g1));  // c0 slot1 Bob
        EXPECT(tworldsvr::ReadMonthRanker(r, g2));  // c0 slot2 Alice
        EXPECT(g0.char_id == 111);
        EXPECT(g1.char_id == 222);
        EXPECT(g2.char_id == 111);
    }

    // Registry post-state: month advanced, slots 1.. reset, warlord
    // survives (legacy MonthRankReset only clears 1..32).
    EXPECT(month_rank.RankMonth() == 8);
    EXPECT(month_rank.Get(0, 0).char_id == 111);
    EXPECT(month_rank.Get(0, 1).char_id == 0);
    EXPECT(month_rank.Get(0, 2).char_id == 0);

    // Guild bank zeroed.
    {
        auto g = guilds.Find(42);
        EXPECT(g != nullptr);
        if (g)
        {
            std::lock_guard lk(g->lock);
            EXPECT(g->pvp_month_point == 0);
            EXPECT(g->rank_month == 0);
        }
    }

    // Fake-repo call trace.
    {
        const auto init = rank_repo.InitCalls();
        EXPECT(init.size() == 2);
        if (init.size() == 2)
        {
            EXPECT(init[0] == 7);
            EXPECT(init[1] == 8);
        }
        const auto saves = rank_repo.SaveCalls();
        EXPECT(saves.size() == 3);
        if (saves.size() == 3)
        {
            EXPECT(saves[0].month == 7);
            EXPECT(saves[0].rank == 0);          // j (warlord row)
            EXPECT(saves[0].total_rank == 0);    // t (top warlord)
            EXPECT(saves[0].ranker.char_id == 111);
            EXPECT(saves[1].rank == 1);
            EXPECT(saves[1].total_rank == 1);    // Bob tops the pool
            EXPECT(saves[1].ranker.char_id == 222);
            EXPECT(saves[2].rank == 2);
            EXPECT(saves[2].total_rank == 2);    // Alice second
            EXPECT(saves[2].ranker.char_id == 111);
        }
        const auto pv = rank_repo.PvPointCalls();
        EXPECT(pv.size() == 1);
        if (!pv.empty())
        {
            EXPECT(pv[0].first == 7);
            EXPECT(pv[0].second == 900);         // Alice's TotalPoint
        }
    }

    // --- W6-42b: failed persist leaves state intact -----------------
    // Seed ranker 444 (month=8 now) and make its save fail.
    rank_repo.FailOnCharId(444);
    SendFramed(p1, ToUint16(MessageId::MW_MONTHRANKUPDATE_ACK),
        UpdateBody(8, 1, MakeRanker(444, "Dave", 300, 20)));
    for (auto* s : {&p1, &p2})
    { auto [w, _] = ReadFramed(*s);
      EXPECT(w == ToUint16(MessageId::MW_MONTHRANKUPDATE_REQ)); }

    SendFramed(p1, ToUint16(MessageId::SM_MONTHRANKSAVE_REQ), {});
    std::this_thread::sleep_for(50ms);
    EXPECT(month_rank.RankMonth() == 8);              // no advance
    EXPECT(month_rank.Get(1, 1).char_id == 444);      // table intact

    // Sentinel: WARLORDSAY must be the next frame on both maps —
    // no RESET / FIRSTGRADEGROUP leaked from the failed rollover.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WritePOD<std::uint8_t>(b, 8);
        wire::WritePOD<std::uint32_t>(b, 444);
        wire::WriteString(b, "denied");
        SendFramed(p1, ToUint16(MessageId::MW_WARLORDSAY_ACK), b);
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, _] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_WARLORDSAY_REQ));
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_monthrank_rollover_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_monthrank_rollover_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
