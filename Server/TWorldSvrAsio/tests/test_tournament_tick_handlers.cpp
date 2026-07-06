// W6-51 wire tests: tournament scheduler tick + step advance.
//
//   Registry-direct: PopDueTick walk (not-due stop, due consume,
//     zero-period END never reschedules, final END → kReschedule)
//     + AdvanceStep (catalogue guard, next-step start, group flip).
//   Wire: SM_TOURNAMENT_REQ → MW_TOURNAMENTENABLE_REQ broadcast to
//     every map peer (next-step start carried); catalogue-miss drop.
//   Tick e2e over a resume-grafted schedule: step fires (ENABLE +
//     TTournamentStatus trace), multi-due drain in one pass, final
//     END reschedule → next-month clocks + election rebuild +
//     MW_TOURNAMENTINFO broadcast + TTournamentClear.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/fake_tournament_repository.h"
#include "../services/guild_registry.h"
#include "../services/peer_registry.h"
#include "../services/tournament_constants.h"
#include "../services/tournament_registry.h"
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
#include <ctime>
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

// Build the three-step ladder used across the file.
tworldsvr::TournamentRegistry::StepMap MakeLadder()
{
    namespace tnmt = tworldsvr::tournament;
    using tworldsvr::TournamentRegistry;
    using tworldsvr::TournamentStep;

    TournamentRegistry::StepMap steps;
    steps.emplace(TournamentRegistry::StepKey(tnmt::kStepEnter, 0),
        TournamentStep{0, tnmt::kStepEnter, 600, 0, 0});
    steps.emplace(TournamentRegistry::StepKey(tnmt::kStepQFinal, 0),
        TournamentStep{0, tnmt::kStepQFinal, 60, 0, 0});
    steps.emplace(TournamentRegistry::StepKey(tnmt::kStepEnd, 0),
        TournamentStep{0, tnmt::kStepEnd, 30, 0, 0});
    return steps;
}

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    namespace wire = tworldsvr::wire;
    namespace tnmt = tworldsvr::tournament;
    using tworldsvr::TournamentRegistry;
    using tworldsvr::TournamentBattleTime;

    const std::int64_t base_now =
        static_cast<std::int64_t>(std::time(nullptr));
    TournamentBattleTime bt{};
    bt.week = 1; bt.day = 1; bt.start_sec = 7200;

    // ---- registry-direct: PopDueTick walk --------------------------
    {
        TournamentRegistry reg;
        EXPECT(reg.PopDueTick(base_now).kind ==
               TournamentRegistry::DueAction::Kind::kNone);

        // Resume-graft at ENTER re-bases the ladder on base_now:
        // ENTER start=base, end=+1200 (600 + the 600 resume pad);
        // QFINAL +1200..+1260; END +1260..+1290.
        const auto res = reg.SetTournamentTime(MakeLadder(), bt, 5,
            false, true, base_now, 5, 0, tnmt::kStepEnter);
        EXPECT(res.id == 5);
        EXPECT(reg.ActiveScheduleId() == 5);

        using Kind = TournamentRegistry::DueAction::Kind;
        // Not due yet.
        EXPECT(reg.PopDueTick(base_now - 1).kind == Kind::kNone);
        // ENTER fires with the SCHEDULE period (600 — the +600
        // resume pad lives on end/current-steps only).
        auto a = reg.PopDueTick(base_now);
        EXPECT(a.kind == Kind::kStep);
        EXPECT(a.id == 5 && a.group == 0 &&
               a.step == tnmt::kStepEnter && a.period == 600);
        // Consumed; QFINAL still future.
        EXPECT(reg.PopDueTick(base_now).kind == Kind::kNone);
        a = reg.PopDueTick(base_now + 1200);
        EXPECT(a.kind == Kind::kStep && a.step == tnmt::kStepQFinal);
        a = reg.PopDueTick(base_now + 1260);
        EXPECT(a.kind == Kind::kStep && a.step == tnmt::kStepEnd);
        // END's end elapsed + it is the last group → reschedule.
        a = reg.PopDueTick(base_now + 1290);
        EXPECT(a.kind == Kind::kReschedule && a.id == 5);
        // Both clocks consumed.
        EXPECT(reg.PopDueTick(base_now + 1290).kind == Kind::kNone);
    }

    // Zero-period END never fires the reschedule (legacy
    // `if(!m_dwPeriod) continue` guard).
    {
        TournamentRegistry reg;
        TournamentRegistry::StepMap steps;
        steps.emplace(TournamentRegistry::StepKey(tnmt::kStepEnter, 0),
            tworldsvr::TournamentStep{0, tnmt::kStepEnter, 600, 0, 0});
        steps.emplace(TournamentRegistry::StepKey(tnmt::kStepEnd, 0),
            tworldsvr::TournamentStep{0, tnmt::kStepEnd, 0, 0, 0});
        const auto res = reg.SetTournamentTime(std::move(steps), bt, 6,
            false, true, base_now, 6, 0, tnmt::kStepEnter);
        EXPECT(res.id == 6);
        using Kind = TournamentRegistry::DueAction::Kind;
        auto a = reg.PopDueTick(base_now);
        EXPECT(a.kind == Kind::kStep && a.step == tnmt::kStepEnter);
        EXPECT(reg.PopDueTick(base_now + 999999).kind == Kind::kNone);
    }

    // ---- registry-direct: AdvanceStep ------------------------------
    {
        TournamentRegistry reg;
        const auto res = reg.SetTournamentTime(MakeLadder(), bt, 5,
            false, true, base_now, 5, 0, tnmt::kStepEnter);
        EXPECT(res.id == 5);

        // No catalogue → guard drop.
        EXPECT(!reg.AdvanceStep(5, 0, tnmt::kStepEnter).has_value());

        tworldsvr::TournamentEntrySeed e{};
        e.entry_id = 2; e.name = "Duel";
        e.type = tnmt::kEntryPrivate;
        reg.SeedEntries(5, {e});

        const auto adv = reg.AdvanceStep(5, 0, tnmt::kStepEnter);
        EXPECT(adv.has_value());
        if (adv)
            EXPECT(adv->next_step_start == base_now + 1200);
        EXPECT(reg.CurrentStep() == tnmt::kStepEnter);

        // Group flip resets + retargets (observable via group).
        const auto adv2 = reg.AdvanceStep(5, 1, tnmt::kStepQFinal);
        EXPECT(adv2.has_value());
        if (adv2)
            EXPECT(adv2->next_step_start == 0);   // no (QFEND, g1) step
        EXPECT(reg.CurrentGroup() == 1);
    }

    // ---- wire + tick e2e --------------------------------------------
    boost::asio::io_context io;
    tworldsvr::CharRegistry              chars;
    tworldsvr::GuildRegistry             guilds;
    tworldsvr::PeerRegistry              peers;
    tworldsvr::CtrlSvrSlot               ctrl_svr;
    tworldsvr::TournamentRegistry        tournaments;
    tworldsvr::FakeTournamentRepository  tournament_repo;

    // Server-side state: resume-grafted ladder + one catalogue entry.
    {
        const auto res = tournaments.SetTournamentTime(MakeLadder(),
            bt, 5, false, true, base_now, 5, 0, tnmt::kStepEnter);
        EXPECT(res.id == 5);
        tworldsvr::TournamentEntrySeed e{};
        e.entry_id = 2; e.name = "Duel";
        e.type = tnmt::kEntryPrivate;
        e.min_level = 20; e.max_level = 90;
        tournaments.SeedEntries(5, {e});
    }

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.ctrl_svr = &ctrl_svr;
    ctx.tournaments = &tournaments;
    ctx.tournament_repo = &tournament_repo;
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
    // W6-53 join replays: the grafted resume sits at ENTER (>= MATCH)
    // with a seeded catalogue, so the join emits the info + an
    // all-peer bracket re-assert (empty roster pre-select).
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_TOURNAMENTINFO_REQ)); }
    { auto [w, b] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_TOURNAMENTMATCH_REQ));
      wire::Reader r(b);
      std::uint8_t count = 0xFF;
      r.Read(count);
      EXPECT(count == 0); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0443));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::MW_TOURNAMENTINFO_REQ)); }
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::MW_TOURNAMENTMATCH_REQ)); }
    // p2's registration fans MW_RELAYCONNECT_REQ to the already-
    // registered p1 (W3a-2) + the join-time bracket re-assert.
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_TOURNAMENTMATCH_REQ)); }

    // --- W6-51a: SM_TOURNAMENT_REQ catalogue-miss drop ---------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint16_t>(b, 99);
        wire::WritePOD<std::uint8_t>(b, 0);
        wire::WritePOD<std::uint8_t>(b, tnmt::kStepEnter);
        wire::WritePOD<std::uint32_t>(b, 600);
        SendFramed(p1, ToUint16(MessageId::SM_TOURNAMENT_REQ), b);
    }
    std::this_thread::sleep_for(30ms);

    // --- W6-51b: SM_TOURNAMENT_REQ step advance ----------------------
    // (First frame both peers ever get — proves the 51a drop.)
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint16_t>(b, 5);
        wire::WritePOD<std::uint8_t>(b, 0);
        wire::WritePOD<std::uint8_t>(b, tnmt::kStepEnter);
        wire::WritePOD<std::uint32_t>(b, 600);
        SendFramed(p1, ToUint16(MessageId::SM_TOURNAMENT_REQ), b);
    }
    for (auto* sock : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*sock);
        EXPECT(w == ToUint16(MessageId::MW_TOURNAMENTENABLE_REQ));
        wire::Reader r(b);
        std::uint8_t  group = 0xFF, step = 0;
        std::uint32_t period = 0;
        std::int64_t  next = 0;
        r.Read(group); r.Read(step); r.Read(period); r.Read(next);
        EXPECT(group == 0);
        EXPECT(step == tnmt::kStepEnter);
        EXPECT(period == 600);
        EXPECT(next == base_now + 1200);   // QFINAL start
        EXPECT(r.Eof());
    }
    EXPECT(tournaments.CurrentStep() == tnmt::kStepEnter);

    // --- W6-51c: tick drains the due step + persists status ----------
    //
    // The 51b advance did NOT consume the schedule clock (that is the
    // tick's job) — so the first tick at base_now fires ENTER again.
    auto run_tick = [&](std::int64_t now) {
        boost::asio::co_spawn(io,
            tworldsvr::handlers::RunTournamentTick(ctx, now),
            boost::asio::detached);
    };
    run_tick(base_now);
    for (auto* sock : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*sock);
        EXPECT(w == ToUint16(MessageId::MW_TOURNAMENTENABLE_REQ));
        wire::Reader r(b);
        std::uint8_t group = 0xFF, step = 0;
        r.Read(group); r.Read(step);
        EXPECT(step == tnmt::kStepEnter);
    }
    std::this_thread::sleep_for(30ms);
    {
        const auto calls = tournament_repo.SaveStatusCalls();
        EXPECT(calls.size() == 1);
        if (!calls.empty())
        {
            EXPECT(calls[0].id == 5);
            EXPECT(calls[0].group == 0);
            EXPECT(calls[0].step == tnmt::kStepEnter);
        }
    }

    // --- W6-51d: one pass drains a multi-due backlog ------------------
    // QFINAL (due at +1200) and END (due at +1260) both fire in one
    // tick at +1260, in ladder order.
    run_tick(base_now + 1260);
    for (auto* sock : {&p1, &p2})
    {
        auto [w1, b1] = ReadFramed(*sock);
        EXPECT(w1 == ToUint16(MessageId::MW_TOURNAMENTENABLE_REQ));
        wire::Reader r1(b1);
        std::uint8_t g1 = 0, s1 = 0;
        r1.Read(g1); r1.Read(s1);
        EXPECT(s1 == tnmt::kStepQFinal);

        auto [w2, b2] = ReadFramed(*sock);
        EXPECT(w2 == ToUint16(MessageId::MW_TOURNAMENTENABLE_REQ));
        wire::Reader r2(b2);
        std::uint8_t g2 = 0, s2 = 0;
        r2.Read(g2); r2.Read(s2);
        EXPECT(s2 == tnmt::kStepEnd);
    }
    std::this_thread::sleep_for(30ms);
    EXPECT(tournament_repo.SaveStatusCalls().size() == 3);
    EXPECT(tournaments.CurrentStep() == tnmt::kStepEnd);

    // --- W6-51e: final END end → next-month reschedule + rebuild -----
    run_tick(base_now + 1290);
    for (auto* sock : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*sock);
        EXPECT(w == ToUint16(MessageId::MW_TOURNAMENTINFO_REQ));
        wire::Reader r(b);
        std::uint8_t fgc = 0xFF, group = 0xFF, step = 0xFF,
            ecount = 0;
        r.Read(fgc); r.Read(group); r.Read(step); r.Read(ecount);
        EXPECT(step == tnmt::kStepReady);   // rebuild reset
        EXPECT(ecount == 1);
        std::uint8_t egroup = 0, eid = 0;
        std::string  name;
        r.Read(egroup); r.Read(eid); r.ReadString(name);
        EXPECT(eid == 2 && name == "Duel");
    }
    std::this_thread::sleep_for(30ms);
    EXPECT(tournament_repo.ClearCalls() >= 1);
    EXPECT(tournaments.CurrentId() == 5);
    EXPECT(tournaments.CurrentStep() == tnmt::kStepReady);
    // Schedule clocks recomputed into next month.
    {
        const auto rows = tournaments.ScheduleList();
        EXPECT(rows.size() == 1);
        if (!rows.empty() && !rows[0].steps.empty())
            EXPECT(rows[0].steps[0].start > base_now + 1290);
    }
    // No further due work.
    run_tick(base_now + 1290);
    std::this_thread::sleep_for(30ms);
    EXPECT(tournament_repo.SaveStatusCalls().size() == 3);

    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("test_tournament_tick_handlers: all passed\n");
    else
        std::fprintf(stderr,
            "test_tournament_tick_handlers: %d failure(s)\n", g_fails);
    return g_fails == 0 ? 0 : 1;
}
