// W6-29 wire test: RPS event subsystem.
//
// Two peers: p1 (Alice's main = 0x42, also the "control" peer for
// the CT_RPSGAME* admin packets), p2 (a second map peer to verify
// the change broadcast fan-out).
//
// Test cases:
//   * MW_RPSGAME_ACK with no config (kNotFound) → MW_RPSGAME_REQ
//     result=0.
//   * Seed an RPS config via direct Insert (boot path), then run
//     MW_RPSGAME_ACK twice with win_keep=2 — first two wins are
//     allowed (result=1), the third hits the cap (result=0).
//   * CT_RPSGAMEDATA_REQ → CT_RPSGAMEDATA_ACK(change=0) with the
//     current row.
//   * CT_RPSGAMECHANGE_REQ with one updated row → registry's row
//     reflects the new probs, ACK(change=1) lands on the control
//     peer + MW_RPSGAMECHANGE_REQ verbatim broadcast on both peers.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/guild_registry.h"
#include "../services/peer_registry.h"
#include "../services/rps_registry.h"
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

std::vector<std::byte> RpsGameBody(std::uint32_t char_id, std::uint32_t key,
                                   std::uint8_t type, std::uint8_t win_count,
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

std::vector<std::byte> GroupBody(std::uint8_t group)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, group);
    return b;
}

std::vector<std::byte>
ChangeBody(std::uint8_t group, std::uint8_t type, std::uint8_t win_count,
           std::uint8_t win_prob, std::uint8_t draw_prob, std::uint8_t lose_prob,
           std::uint16_t win_keep, std::uint16_t win_period)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, group);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, 1);   // count
    tworldsvr::wire::WritePOD(b, type);
    tworldsvr::wire::WritePOD(b, win_count);
    tworldsvr::wire::WritePOD(b, win_prob);
    tworldsvr::wire::WritePOD(b, draw_prob);
    tworldsvr::wire::WritePOD(b, lose_prob);
    tworldsvr::wire::WritePOD(b, win_keep);
    tworldsvr::wire::WritePOD(b, win_period);
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
    tworldsvr::RpsRegistry   rps;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.rps = &rps; ctx.nation = 0;

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

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0043));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, 0xA1));
    for (int i = 0; i < 200 && !chars.Find(42); ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(chars.Find(42) != nullptr);

    // --- Test A: MW_RPSGAME_ACK with no config → result=0 -----------
    {
        SendFramed(p1, ToUint16(MessageId::MW_RPSGAME_ACK),
                   RpsGameBody(42, 0xA1, /*type=*/1, /*win_count=*/3,
                       /*player_rps=*/0));
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_RPSGAME_REQ));
        wire::Reader r(b);
        std::uint32_t cid = 0, key = 0;
        std::uint8_t  result = 0xFF, prps = 0xFF;
        r.Read(cid); r.Read(key); r.Read(result); r.Read(prps);
        EXPECT(cid == 42);
        EXPECT(result == 0);            // kNotFound
        EXPECT(prps == 0);              // echoed
    }

    // --- Test B: seed config + win-keep cap (3 attempts, cap=2) -----
    EXPECT(rps.Insert(tworldsvr::TRpsGame{
        /*type=*/1, /*win_count=*/3,
        /*win_prob=*/40, /*draw_prob=*/30, /*lose_prob=*/30,
        /*win_keep=*/2, /*win_period=*/7,
        /*win_dates=*/{} }));

    auto one_round = [&](std::uint8_t expect_result) {
        SendFramed(p1, ToUint16(MessageId::MW_RPSGAME_ACK),
                   RpsGameBody(42, 0xA1, /*type=*/1, /*win_count=*/3,
                       /*player_rps=*/1));
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_RPSGAME_REQ));
        wire::Reader r(b);
        std::uint32_t cid = 0, key = 0;
        std::uint8_t  result = 0xFF, prps = 0xFF;
        r.Read(cid); r.Read(key); r.Read(result); r.Read(prps);
        EXPECT(result == expect_result);
    };
    one_round(1);     // 1st win — under cap
    one_round(1);     // 2nd win — at cap exactly (fresh_count=2, win_keep=2 → kCapReached)
    // The legacy gate fires when fresh_count >= win_keep BEFORE
    // inserting, so the third attempt hits the cap (we recorded
    // 2 wins, threshold is win_keep=2).
    one_round(0);

    // --- Test C: CT_RPSGAMEDATA_REQ → ACK(change=0, 1 row) ----------
    {
        SendFramed(p1, ToUint16(MessageId::CT_RPSGAMEDATA_REQ),
                   GroupBody(/*group=*/7));
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::CT_RPSGAMEDATA_ACK));
        wire::Reader r(b);
        std::uint8_t  change = 0xFF, group = 0;
        std::uint16_t count = 0;
        r.Read(change); r.Read(group); r.Read(count);
        EXPECT(change == 0);
        EXPECT(group == 7);
        EXPECT(count == 1);
        if (count == 1)
        {
            std::uint8_t  type = 0, wc = 0, wp = 0, dp = 0, lp = 0;
            std::uint16_t wk = 0, wpd = 0;
            r.Read(type); r.Read(wc); r.Read(wp); r.Read(dp); r.Read(lp);
            r.Read(wk); r.Read(wpd);
            EXPECT(type == 1);
            EXPECT(wc == 3);
            EXPECT(wp == 40); EXPECT(dp == 30); EXPECT(lp == 30);
            EXPECT(wk == 2); EXPECT(wpd == 7);
        }
    }

    // --- Test D: CT_RPSGAMECHANGE_REQ updates + broadcasts ---------
    {
        SendFramed(p1, ToUint16(MessageId::CT_RPSGAMECHANGE_REQ),
                   ChangeBody(/*group=*/7, /*type=*/1, /*wc=*/3,
                       /*wp=*/50, /*dp=*/25, /*lp=*/25,
                       /*wk=*/5, /*wpd=*/14));
        // Two emitted packets per recipient peer: the ACK(change=1)
        // back to p1 (the requester) and the MW_RPSGAMECHANGE_REQ
        // verbatim broadcast to *every* map peer (p1 + p2). Read
        // them in dispatch-emit order: ACK on p1, then the broadcast
        // on p1 + p2 (peer registry iteration order is unordered;
        // we read whichever arrives, then the other).
        auto [w1, b1] = ReadFramed(p1);
        EXPECT(w1 == ToUint16(MessageId::CT_RPSGAMEDATA_ACK));
        wire::Reader r1(b1);
        std::uint8_t change = 0; r1.Read(change);
        EXPECT(change == 1);

        // The broadcast hits both p1 and p2 (legacy SSHandler.cpp:
        // 13317 — every server in m_mapSERVER).
        auto [w_p1, _b_p1] = ReadFramed(p1);
        EXPECT(w_p1 == ToUint16(MessageId::MW_RPSGAMECHANGE_REQ));
        auto [w_p2, _b_p2] = ReadFramed(p2);
        EXPECT(w_p2 == ToUint16(MessageId::MW_RPSGAMECHANGE_REQ));

        // Registry now has the updated probs.
        auto snap = rps.Snapshot();
        EXPECT(snap.size() == 1);
        if (snap.size() == 1)
        {
            EXPECT(snap[0].win_prob == 50);
            EXPECT(snap[0].draw_prob == 25);
            EXPECT(snap[0].lose_prob == 25);
            EXPECT(snap[0].win_keep == 5);
            EXPECT(snap[0].win_period == 14);
        }
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_rps_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_rps_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
