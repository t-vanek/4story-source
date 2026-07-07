// W6-54 wire tests: Bow battleground scheduler.
//
//   Wire: the window walk (silent first-tick arm, ALARM notify),
//     ALARM-gated queue ops (solo + guild + duplicate + cancel),
//     match creation at the PEACE edge (teams, roster broadcast,
//     per-char PREPAREFORBOW + team re-stamp + TAddBOWPlayer
//     traces), the BS_PEACE late-join, the BATTLE edge (BOW_START +
//     the non-queued nudge), the points decay broadcast.
//   Registry-direct (injected clocks): the wipe-to-zero peace
//     trigger, the BODPEACE countdown (BOW_END once, winner
//     election), the mid-peace single release, EndBattle (roster
//     fan-out + reset + the winner surviving the release).

#include "../handlers/handlers.h"
#include "../services/bow_constants.h"
#include "../services/bow_registry.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/fake_bow_repository.h"
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

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    namespace wire = tworldsvr::wire;
    namespace bow = tworldsvr::bow;
    using tworldsvr::BowRegistry;

    // ---- registry-direct: end-of-battle sequence -------------------
    {
        BowRegistry reg;
        reg.Configure(3000, 0, 1, 60, 30, 120);
        reg.AddStartTime(1000);
        reg.Init(500);
        EXPECT(reg.NextStart() == 1000);

        // Arm (silent) + ALARM.
        auto a = reg.Tick(1001, 1000, true);
        EXPECT(a.notifies.empty());          // first-tick silent arm
        EXPECT(reg.Running());
        a = reg.Tick(1002, 1000, true);
        EXPECT(a.notifies.size() == 1);
        EXPECT(a.notifies[0].status == bow::kBsAlarm);
        EXPECT(a.notifies[0].second == 58);
        EXPECT(reg.Status() == bow::kBsAlarm);

        // Four solo players queue during ALARM.
        for (std::uint32_t id = 42; id < 46; ++id)
            EXPECT(reg.AddPlayer(id, id, id % 2, 0).result ==
                   bow::kSuccess);

        // PEACE edge → match creation.
        a = reg.Tick(1061, 2000, true);
        EXPECT(a.teleport_in);
        EXPECT(a.roster.size() == 4);
        EXPECT(reg.MatchSize() == 4);
        EXPECT(reg.QueueSize() == 0);
        EXPECT(reg.Points(bow::kCountryD) == 5);

        // Points: 4 x D-score decays C to 1; the 5th wipes to 0 and
        // arms the peace countdown (legacy match-end trigger).
        for (int i = 0; i < 4; ++i)
            reg.UpdatePoints(bow::kCountryD, 3000);
        EXPECT(reg.Points(bow::kCountryC) == 1);
        const auto po = reg.UpdatePoints(bow::kCountryD, 3000);
        EXPECT(po.points_c == 0);
        EXPECT(po.points_d == 10);

        // Next tick rides the BODPEACE path: BOW_END once + winner.
        a = reg.Tick(1062, 4000, true);
        EXPECT(a.commands.size() == 1);
        EXPECT(a.commands[0] == bow::kCmdEnd);
        EXPECT(!a.ended);
        EXPECT(a.notifies.size() == 1);
        EXPECT(a.notifies[0].status == bow::kBsBodPeace);
        EXPECT(a.notifies[0].second == 11);   // 12 - 1s elapsed
        EXPECT(reg.Winner() == bow::kWinnerDefugel);

        // Mid-peace single release (winner decided, roster alive).
        const auto rel = reg.ReleaseSinglePlayer(42, 42);
        EXPECT(rel.release);
        EXPECT(rel.winner == bow::kWinnerDefugel);
        EXPECT(reg.MatchSize() == 3);
        EXPECT(!reg.ReleaseSinglePlayer(42, 42).release);

        // Countdown expiry → EndBattle (left = 12500ms → second 0).
        a = reg.Tick(1063, 15500, true);
        EXPECT(a.ended);
        EXPECT(a.winner == bow::kWinnerDefugel);
        EXPECT(a.end_roster.size() == 3);
        EXPECT(a.commands.empty());          // END fired only once
        EXPECT(a.notifies.size() == 1);
        EXPECT(a.notifies[0].status == bow::kBsBodPeace);
        EXPECT(a.notifies[0].second == 0);
        EXPECT(a.notifies[0].points_d == 5); // post-release reset
        EXPECT(reg.Status() == bow::kBsNormal);
        // The legacy re-arms m_bRunning after EVERY in-window tick
        // (TWorldSvr.cpp:4112) — even the one that ended the battle.
        EXPECT(reg.Running());
        EXPECT(reg.MatchSize() == 0);
        EXPECT(reg.Winner() == bow::kWinnerDefugel);  // survives
    }

    // ---- wire harness ------------------------------------------------
    boost::asio::io_context io;
    tworldsvr::CharRegistry             chars;
    tworldsvr::GuildRegistry            guilds;
    tworldsvr::PeerRegistry             peers;
    tworldsvr::CtrlSvrSlot              ctrl_svr;
    tworldsvr::CtrlSvrSlot              bow_server;
    tworldsvr::BowRegistry              bow_reg;
    tworldsvr::FakeBowRepository        bow_repo;

    bow_reg.Configure(3000, 0, 1, 60, 30, 120);
    bow_reg.AddStartTime(1000);
    bow_reg.Init(500);

    auto add_char = [&](std::uint32_t id, std::uint32_t key,
                        std::uint8_t country, std::uint32_t tactics,
                        const char* name) {
        auto c = std::make_shared<tworldsvr::TChar>();
        c->char_id = id; c->key = key; c->country = country;
        c->user_id = id + 10000;
        c->tactics_guild_id = tactics;
        c->name = name; c->main_server_id = 0x42;
        chars.Insert(c);
        chars.Rename(id, name);
    };
    add_char(42, 42, 1, 0,   "Alice");
    add_char(43, 43, 0, 0,   "Bob");
    add_char(44, 44, 1, 900, "Carol");
    add_char(45, 45, 1, 900, "Dave");
    add_char(46, 46, 0, 0,   "Eve");
    add_char(47, 47, 1, 0,   "Fred");   // never queues

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.ctrl_svr = &ctrl_svr;
    ctx.bow = &bow_reg;
    ctx.bow_repo = &bow_repo;
    ctx.bow_server = &bow_server;
    ctx.nation = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0; svr_cfg.max_connections = 4; svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket p1(client_io), pbow(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep); pbow.connect(ep);
    std::this_thread::sleep_for(20ms);

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0442));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    // The BOW battleground server: LOBYTE(wID) == 30.
    SendFramed(pbow, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x041E));
    { auto [w, _] = ReadFramed(pbow);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    // p1 gets the peer-join RELAYCONNECT fan-out.
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }
    std::this_thread::sleep_for(20ms);
    EXPECT(bow_server.Get() != nullptr);

    auto run_tick = [&](std::uint32_t clt, std::uint64_t ms) {
        boost::asio::co_spawn(io,
            tworldsvr::handlers::RunBowTick(ctx, clt, ms),
            boost::asio::detached);
    };

    // --- W6-54a: silent arm, then the ALARM frame ---------------------
    run_tick(1001, 1000);
    std::this_thread::sleep_for(30ms);
    run_tick(1002, 1000);
    for (auto* sock : {&p1, &pbow})
    {
        auto [w, b] = ReadFramed(*sock);   // first frame ever → 51a
        EXPECT(w == ToUint16(MessageId::MW_BOWTIMEUPDATE_ACK));
        wire::Reader r(b);
        std::uint8_t status = 0; std::uint32_t second = 0;
        std::uint8_t d = 0, c = 0;
        r.Read(status); r.Read(second); r.Read(d); r.Read(c);
        EXPECT(status == bow::kBsAlarm);
        EXPECT(second == 58);
        EXPECT(d == 5 && c == 5);
        EXPECT(r.Eof());
    }

    // --- W6-54b: ALARM queue ops ---------------------------------------
    auto queue_req = [&](std::uint32_t id) {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, id);
        wire::WritePOD<std::uint32_t>(b, id);
        SendFramed(p1, ToUint16(MessageId::MW_ADDTOBOWQUEUE_REQ), b);
    };
    auto read_queue_ack = [&](std::uint32_t id, std::uint8_t expect) {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_ADDTOBOWQUEUE_ACK));
        wire::Reader r(b);
        std::uint8_t result = 0xFF;
        std::uint32_t cid = 0, key = 0, tick = 0;
        r.Read(result); r.Read(cid); r.Read(key); r.Read(tick);
        EXPECT(result == expect);
        EXPECT(cid == id);
        EXPECT(tick == 58);            // m_dwTick echo
    };
    queue_req(42); read_queue_ack(42, bow::kSuccess);
    queue_req(43); read_queue_ack(43, bow::kSuccess);
    queue_req(44); read_queue_ack(44, bow::kSuccess);   // guild 900
    queue_req(45); read_queue_ack(45, bow::kSuccess);   // guild 900
    queue_req(42); read_queue_ack(42, bow::kAlreadyInQueue);
    EXPECT(bow_reg.QueueSize() == 4);
    // Cancel + re-add Bob (solo path).
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 43);
        wire::WritePOD<std::uint32_t>(b, 43);
        SendFramed(p1, ToUint16(MessageId::MW_CANCELBOWQUEUE_REQ), b);
        auto [w, rb] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CANCELBOWQUEUE_ACK));
        wire::Reader r(rb);
        std::uint8_t result = 0xFF;
        r.Read(result);
        EXPECT(result == bow::kSuccess);
    }
    queue_req(43); read_queue_ack(43, bow::kSuccess);

    // --- W6-54c: PEACE edge → match creation ---------------------------
    run_tick(1061, 2000);
    { // p1: notify, then 4x PREPAREFORBOW to the chars' main map.
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_BOWTIMEUPDATE_ACK));
        wire::Reader r(b);
        std::uint8_t status = 0; std::uint32_t second = 0;
        r.Read(status); r.Read(second);
        EXPECT(status == bow::kBsPeace);
        EXPECT(second == 29);
        bool saw_alice_d = false;
        for (int i = 0; i < 4; ++i)
        {
            auto [w2, b2] = ReadFramed(p1);
            EXPECT(w2 == ToUint16(MessageId::MW_PREPAREFORBOW_REQ));
            wire::Reader r2(b2);
            std::uint32_t cid = 0, key = 0; std::uint8_t team = 0xFF;
            r2.Read(cid); r2.Read(key); r2.Read(team);
            EXPECT(team <= bow::kCountryC);
            if (cid == 42 && team == bow::kCountryD)
                saw_alice_d = true;
        }
        // Solo players filled the smaller (D) side: Alice's country
        // was 1 (C) but the guild block claimed the C team, so she
        // landed on D and her char got re-stamped.
        EXPECT(saw_alice_d);
    }
    { // pbow: notify + the full roster.
        auto [w, b] = ReadFramed(pbow);
        EXPECT(w == ToUint16(MessageId::MW_BOWTIMEUPDATE_ACK));
        auto [w2, b2] = ReadFramed(pbow);
        EXPECT(w2 == ToUint16(MessageId::MW_ADDBOWPLAYERS_ACK));
        wire::Reader r(b2);
        std::uint8_t count = 0;
        r.Read(count);
        EXPECT(count == 4);
        std::uint8_t teams[2] = {0, 0};
        for (std::uint8_t i = 0; i < count; ++i)
        {
            std::uint32_t cid = 0, key = 0; std::uint8_t team = 0xFF;
            r.Read(cid); r.Read(key); r.Read(team);
            EXPECT(team <= bow::kCountryC);
            if (team <= bow::kCountryC)
                ++teams[team];
        }
        EXPECT(r.Eof());
        EXPECT(teams[0] == 2 && teams[1] == 2);   // balanced 2v2
    }
    std::this_thread::sleep_for(30ms);
    {
        auto c = chars.Find(42);
        std::lock_guard g(c->lock);
        EXPECT(c->country == bow::kCountryD);     // re-stamped
    }
    EXPECT(bow_repo.AddCalls().size() == 4);
    EXPECT(bow_reg.MatchSize() == 4);

    // --- W6-54d: BS_PEACE late-join ------------------------------------
    queue_req(46);
    { // Teleport-in first (roster row to pbow)…
        auto [w, b] = ReadFramed(pbow);
        EXPECT(w == ToUint16(MessageId::MW_ADDBOWPLAYERS_ACK));
        wire::Reader r(b);
        std::uint8_t count = 0;
        r.Read(count);
        EXPECT(count == 1);
        std::uint32_t cid = 0, key = 0; std::uint8_t team = 0xFF;
        r.Read(cid); r.Read(key); r.Read(team);
        EXPECT(cid == 46);
        EXPECT(team == bow::kCountryD);   // 2v2 tie → D side
    }
    { // …then PREPAREFORBOW + the FAIL+1 ack on the main map.
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_PREPAREFORBOW_REQ));
        auto [w2, b2] = ReadFramed(p1);
        EXPECT(w2 == ToUint16(MessageId::MW_ADDTOBOWQUEUE_ACK));
        wire::Reader r(b2);
        std::uint8_t result = 0;
        r.Read(result);
        EXPECT(result == bow::kLateJoin);
    }
    std::this_thread::sleep_for(30ms);
    EXPECT(bow_repo.AddCalls().size() == 5);
    EXPECT(bow_reg.MatchSize() == 5);

    // --- W6-54e: BATTLE edge --------------------------------------------
    run_tick(1092, 3000);
    { // p1: notify + Fred's non-queued nudge.
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_BOWTIMEUPDATE_ACK));
        wire::Reader r(b);
        std::uint8_t status = 0; std::uint32_t second = 0;
        r.Read(status); r.Read(second);
        EXPECT(status == bow::kBsBattle);
        EXPECT(second == 118);
        auto [w2, b2] = ReadFramed(p1);
        EXPECT(w2 ==
            ToUint16(MessageId::MW_NOTIFYNONQUEUEDPLAYER_ACK));
        wire::Reader r2(b2);
        std::uint32_t cid = 0;
        r2.Read(cid);
        EXPECT(cid == 47);
    }
    { // pbow: notify + BOW_START.
        auto [w, b] = ReadFramed(pbow);
        EXPECT(w == ToUint16(MessageId::MW_BOWTIMEUPDATE_ACK));
        auto [w2, b2] = ReadFramed(pbow);
        EXPECT(w2 == ToUint16(MessageId::MW_BOWCOMMANDEXEC_REQ));
        wire::Reader r(b2);
        std::uint8_t cmd = 0xFF;
        r.Read(cmd);
        EXPECT(cmd == bow::kCmdStart);
        // Fred's nudge routes to his main (p1) only.
    }

    // --- W6-54f: points decay broadcast ---------------------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, bow::kCountryD);
        SendFramed(p1, ToUint16(MessageId::MW_BOWPOINTSUPDATE_REQ), b);
    }
    for (auto* sock : {&p1, &pbow})
    {
        auto [w, b] = ReadFramed(*sock);
        EXPECT(w == ToUint16(MessageId::MW_BOWTIMEUPDATE_ACK));
        wire::Reader r(b);
        std::uint8_t status = 0; std::uint32_t second = 0;
        std::uint8_t d = 0, c = 0;
        r.Read(status); r.Read(second); r.Read(d); r.Read(c);
        EXPECT(status == bow::kBsBattle);
        EXPECT(d == 6 && c == 4);
    }

    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("test_bow_scheduler_handlers: all passed\n");
    else
        std::fprintf(stderr,
            "test_bow_scheduler_handlers: %d failure(s)\n", g_fails);
    return g_fails == 0 ? 0 : 1;
}
