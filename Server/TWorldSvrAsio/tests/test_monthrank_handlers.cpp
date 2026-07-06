// W6-41 wire tests: MonthRank live table.
//
//   MW_MONTHRANKUPDATE_ACK — first ranker lands (start=end, warlord
//     flag + slot0 payload), stale-month drop, second ranker with a
//     higher MonthPoint displaces the first (range broadcast).
//   MW_MONTHRANKRESETCHAR_ACK — mirrored to the char's OTHER maps
//     only (the reporting peer is skipped).
//   MW_WARLORDSAY_ACK — verbatim-field broadcast to every map.
//   Replay-on-connect — a joining peer receives the full 2+3×33-
//     ranker MW_MONTHRANKLIST_REQ table.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
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
    tworldsvr::CharRegistry      chars;
    tworldsvr::GuildRegistry     guilds;
    tworldsvr::PeerRegistry      peers;
    tworldsvr::CtrlSvrSlot       ctrl_svr;
    tworldsvr::MonthRankRegistry month_rank;
    month_rank.SetRankMonth(7);

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.ctrl_svr = &ctrl_svr;
    ctx.month_rank = &month_rank;
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

    // p1 joins first — its replay table must be all-empty rankers.
    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0442));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_MONTHRANKLIST_REQ));
        wire::Reader r(b);
        std::uint8_t month = 0, count = 0;
        r.Read(month); r.Read(count);
        EXPECT(month == 7);
        EXPECT(count == 33);
        tworldsvr::MonthRanker first{};
        EXPECT(tworldsvr::ReadMonthRanker(r, first));
        EXPECT(first.char_id == 0);
        EXPECT(first.name.empty());
    }

    // --- W6-41a: stale month → drop -------------------------------
    SendFramed(p1, ToUint16(MessageId::MW_MONTHRANKUPDATE_ACK),
        UpdateBody(/*month=*/6, 0, MakeRanker(111, "Alice", 900, 40)));
    std::this_thread::sleep_for(30ms);
    EXPECT(month_rank.Get(0, 0).char_id == 0);   // untouched

    // --- W6-41b: first ranker lands + becomes warlord ---------------
    // Fresh char (old=0→32) beats empty slot 1 (new=1) → the whole
    // 1..32 range shifts and broadcasts (legacy old>new branch);
    // Alice takes slot 1 AND the warlord seat (flag + slot0 payload).
    SendFramed(p1, ToUint16(MessageId::MW_MONTHRANKUPDATE_ACK),
        UpdateBody(7, 0, MakeRanker(111, "Alice", 900, 40)));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_MONTHRANKUPDATE_REQ));
        wire::Reader r(b);
        std::uint8_t month = 0, country = 0, start = 0, end = 0;
        r.Read(month); r.Read(country); r.Read(start); r.Read(end);
        EXPECT(month == 7);
        EXPECT(country == 0);
        EXPECT(start == 1 && end == 32);
        tworldsvr::MonthRanker slot{};
        EXPECT(tworldsvr::ReadMonthRanker(r, slot));
        EXPECT(slot.char_id == 111);       // slot 1 = Alice
        for (int i = 2; i <= 32; ++i)      // rest of the range: empty
        {
            tworldsvr::MonthRanker rest{};
            EXPECT(tworldsvr::ReadMonthRanker(r, rest));
            EXPECT(rest.char_id == 0);
        }
        std::uint8_t warlord_flag = 0;
        r.Read(warlord_flag);
        EXPECT(warlord_flag == 1);
        tworldsvr::MonthRanker warlord{};
        EXPECT(tworldsvr::ReadMonthRanker(r, warlord));
        EXPECT(warlord.char_id == 111);
        EXPECT(warlord.name == "Alice");
    }
    EXPECT(month_rank.Get(0, 0).char_id == 111);   // warlord slot
    EXPECT(month_rank.Get(0, 1).char_id == 111);   // rank slot

    // --- W6-41c: stronger ranker displaces -------------------------
    // Bob: more MonthPoint → takes slot 1, Alice shifts to slot 2;
    // lower TotalPoint → warlord unchanged (flag 0).
    SendFramed(p1, ToUint16(MessageId::MW_MONTHRANKUPDATE_ACK),
        UpdateBody(7, 0, MakeRanker(222, "Bob", 500, 60)));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_MONTHRANKUPDATE_REQ));
        wire::Reader r(b);
        std::uint8_t month = 0, country = 0, start = 0, end = 0;
        r.Read(month); r.Read(country); r.Read(start); r.Read(end);
        EXPECT(start == 1 && end == 32);
        tworldsvr::MonthRanker s1{}, s2{};
        EXPECT(tworldsvr::ReadMonthRanker(r, s1));
        EXPECT(tworldsvr::ReadMonthRanker(r, s2));
        EXPECT(s1.char_id == 222);         // Bob above Alice
        EXPECT(s2.char_id == 111);
        for (int i = 3; i <= 32; ++i)
        {
            tworldsvr::MonthRanker rest{};
            EXPECT(tworldsvr::ReadMonthRanker(r, rest));
        }
        std::uint8_t warlord_flag = 0xFF;
        r.Read(warlord_flag);
        EXPECT(warlord_flag == 0);         // Alice keeps warlord
    }
    EXPECT(month_rank.Get(0, 0).char_id == 111);
    EXPECT(month_rank.Get(0, 1).char_id == 222);
    EXPECT(month_rank.Get(0, 2).char_id == 111);

    // --- W6-41d: RESETCHAR mirrors to other maps only ---------------
    // p2 registers now (gets its own replay), char 900 connected to
    // both 0x42 and 0x43; p2 reports the reset → only p1 receives it.
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0443));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::MW_MONTHRANKLIST_REQ)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }
    for (auto* s : {&p1, &p2})
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 900);
        wire::WritePOD<std::uint32_t>(b, 0xC3);
        wire::WritePOD<std::uint32_t>(b, 0x7f000001);
        wire::WritePOD<std::uint16_t>(b, 33500);
        wire::WritePOD<std::uint32_t>(b, 700);
        SendFramed(*s, ToUint16(MessageId::MW_ADDCHAR_ACK), b);
        std::this_thread::sleep_for(20ms);
    }
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 900);
        SendFramed(p2, ToUint16(MessageId::MW_MONTHRANKRESETCHAR_ACK), b);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_MONTHRANKRESETCHAR_REQ));
        wire::Reader r(b);
        std::uint32_t cid = 0;
        r.Read(cid);
        EXPECT(cid == 900);
    }

    // --- W6-41e: WARLORDSAY broadcast -------------------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WritePOD<std::uint8_t>(b, 7);
        wire::WritePOD<std::uint32_t>(b, 111);
        wire::WriteString(b, "kneel before Defugel");
        SendFramed(p2, ToUint16(MessageId::MW_WARLORDSAY_ACK), b);
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_WARLORDSAY_REQ));
        wire::Reader r(b);
        std::uint8_t type = 0, month = 0;
        std::uint32_t cid = 0;
        std::string say;
        r.Read(type); r.Read(month); r.Read(cid); r.ReadString(say);
        EXPECT(type == 1);
        EXPECT(month == 7);
        EXPECT(cid == 111);
        EXPECT(say == "kneel before Defugel");
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_monthrank_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_monthrank_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
