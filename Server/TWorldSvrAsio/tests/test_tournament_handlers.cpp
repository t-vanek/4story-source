// W6-50 wire tests: tournament state core + operator tools.
//
//   Registry-direct: SetTournamentTime month math + resume graft +
//     past-window self-eviction + main-tournament overlap eviction.
//   Boot: LoadTournamentState against the fake repo (event schedule
//     + entries + rewards + election + rebuild).
//   Wire: TET_SCHEDULEADD (new id ack + persist trace; overlap-fail
//     ack), TET_ENTRYADD (catalogue replace + persist), TET_LIST
//     (byte-walked times + steps + catalogue + rosters),
//     TET_PLAYERADD (repo lookup + register + persist + echo; guard
//     zeros; duplicate zeros), TET_PLAYERDEL (unregister + persist +
//     echo), TET_SCHEDULEDEL (schedule + catalogue drop + persist),
//     SM_TOURNAMENTUPDATE_REQ (rebuild + MW_TOURNAMENTINFO broadcast).

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/fake_tournament_repository.h"
#include "../services/guild_registry.h"
#include "../services/peer_registry.h"
#include "../services/tournament_boot.h"
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

// Local-midnight helper mirroring the registry's month math so the
// registry-direct expectations are TZ-independent.
std::int64_t LocalMidnight(int year, int month, int day)
{
    std::tm tm{};
    tm.tm_year  = year - 1900;
    tm.tm_mon   = month - 1;
    tm.tm_mday  = day;
    tm.tm_isdst = -1;
    return static_cast<std::int64_t>(std::mktime(&tm));
}

int WeekdayOf(std::int64_t t)
{
    std::time_t tt = static_cast<std::time_t>(t);
    std::tm     lt{};
#ifdef _WIN32
    localtime_s(&lt, &tt);
#else
    localtime_r(&tt, &lt);
#endif
    return lt.tm_wday + 1;
}

// The n-th `weekday` (1..7) of the month containing `t0`.
std::int64_t NthWeekdayOfMonth(int year, int month, int weekday, int n)
{
    int hit = 0;
    for (int d = 1; d <= 31; ++d)
    {
        const std::int64_t t = LocalMidnight(year, month, d);
        if (WeekdayOf(t) == weekday && ++hit == n)
            return t;
    }
    return -1;
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
    using tworldsvr::TournamentStep;
    using tworldsvr::TournamentBattleTime;

    // ---- registry-direct: SetTournamentTime month math ------------
    {
        TournamentRegistry reg;
        // Fixed reference point: 2026-03-10 12:00 local.
        const std::int64_t now =
            LocalMidnight(2026, 3, 10) + 12 * 3600;

        // Schedule on the 2nd Wednesday (weekday 4). March 2026:
        // 2nd Wednesday = the 11th — later than `now`, so the
        // current-month pass lands.
        TournamentBattleTime bt{};
        bt.week = 2; bt.day = 4; bt.start_sec = 3600;

        TournamentRegistry::StepMap steps;
        steps.emplace(TournamentRegistry::StepKey(tnmt::kStepEnter, 0),
            TournamentStep{0, tnmt::kStepEnter, 600, 0, 0});
        steps.emplace(TournamentRegistry::StepKey(tnmt::kStepEnd, 0),
            TournamentStep{0, tnmt::kStepEnd, 0, 0, 0});

        const auto res = reg.SetTournamentTime(steps, bt, 7,
            /*month_base=*/false, /*enable=*/true, now);
        EXPECT(res.id == 7);
        EXPECT(res.evicted.empty());
        EXPECT(reg.HasSchedule(7));
        EXPECT(reg.NextScheduleId() == 7);

        const auto rows = reg.ScheduleList();
        EXPECT(rows.size() == 1);
        if (!rows.empty())
        {
            const std::int64_t expect_start =
                NthWeekdayOfMonth(2026, 3, 4, 2) + 3600;
            EXPECT(rows[0].id == 7);
            EXPECT(rows[0].steps.size() == 2);
            if (rows[0].steps.size() == 2)
            {
                EXPECT(rows[0].steps[0].step == tnmt::kStepEnter);
                EXPECT(rows[0].steps[0].start == expect_start);
                EXPECT(rows[0].steps[0].end == expect_start + 600);
                EXPECT(rows[0].steps[1].start == expect_start + 600);
            }
        }

        // Overlap: another id in the same window fails, nothing
        // evicted (non-main).
        const auto res2 = reg.SetTournamentTime(steps, bt, 8,
            false, true, now);
        EXPECT(res2.id == 0);
        EXPECT(res2.evicted.empty());
        EXPECT(!reg.HasSchedule(8));

        // Main tournament (id 1) in the same window evicts id 7 and
        // still fails itself (legacy `return 0` after the evict).
        const auto res3 = reg.SetTournamentTime(steps, bt, 1,
            false, true, now);
        EXPECT(res3.id == 0);
        EXPECT(res3.evicted.size() == 1 &&
               res3.evicted[0] == 7);
        EXPECT(!reg.HasSchedule(7));

        // No-window re-add of a registered id evicts itself: a
        // 5th-Saturday slot matches neither March nor April 2026
        // (four Saturdays each, and April's day-31 mktime spillover
        // lands on Friday May 1st — the same normalization quirk the
        // release-MFC CTime scan has), so the month scan never finds
        // a start and the dstart<=now branch fires.
        const auto res4 = reg.SetTournamentTime(steps, bt, 9,
            false, true, now);
        EXPECT(res4.id == 9);
        TournamentBattleTime none{};
        none.week = 5; none.day = 7; none.start_sec = 3600;
        const auto res5 = reg.SetTournamentTime(steps, none, 9,
            false, true, now);
        EXPECT(res5.id == 0);
        EXPECT(res5.evicted.size() == 1 && res5.evicted[0] == 9);
        EXPECT(!reg.HasSchedule(9));
    }

    // ---- registry-direct: boot-resume graft ------------------------
    {
        TournamentRegistry reg;
        const std::int64_t now =
            LocalMidnight(2026, 3, 10) + 12 * 3600;
        TournamentBattleTime bt{};
        bt.week = 2; bt.day = 4; bt.start_sec = 3600;

        TournamentRegistry::StepMap steps;
        steps.emplace(TournamentRegistry::StepKey(tnmt::kStepEnter, 0),
            TournamentStep{0, tnmt::kStepEnter, 600, 0, 0});
        steps.emplace(TournamentRegistry::StepKey(tnmt::kStepSFEnter, 0),
            TournamentStep{0, tnmt::kStepSFEnter, 300, 0, 0});
        steps.emplace(TournamentRegistry::StepKey(tnmt::kStepFinal, 0),
            TournamentStep{0, tnmt::kStepFinal, 900, 0, 0});
        steps.emplace(TournamentRegistry::StepKey(tnmt::kStepEnd, 0),
            TournamentStep{0, tnmt::kStepEnd, 0, 0, 0});

        // Resume at SFINAL → compressed back to SFENTER, clock
        // re-based on `now`, the resumed step +10 min; earlier steps
        // zero out, later ones chain after.
        const auto res = reg.SetTournamentTime(steps, bt, 3,
            false, true, now, /*tnmt_id=*/3, /*tnmt_group=*/0,
            /*tnmt_step=*/tnmt::kStepSFinal);
        EXPECT(res.id == 3);
        EXPECT(reg.CurrentId() == 3);
        EXPECT(reg.CurrentStep() == tnmt::kStepSFEnter);
        EXPECT(reg.ActiveScheduleId() == 3);

        const auto rows = reg.ScheduleList();
        EXPECT(rows.size() == 1);
        EXPECT(!rows.empty() && rows[0].steps.size() == 4);
        if (!rows.empty() && rows[0].steps.size() == 4)
        {
            // ENTER (before the resume point): zeroed.
            EXPECT(rows[0].steps[0].start == 0);
            EXPECT(rows[0].steps[0].end == 0);
            // SFENTER: start=now, period 300 + 600 add_time.
            EXPECT(rows[0].steps[1].start == now);
            EXPECT(rows[0].steps[1].end == now + 300 + 600);
            // FINAL chains after.
            EXPECT(rows[0].steps[2].start == now + 900);
            EXPECT(rows[0].steps[2].end == now + 900 + 900);
            // END: period 0 → zeroed by the edit branch.
            EXPECT(rows[0].steps[3].start == 0);
        }

        // A QFINAL resume hits none of the compression branches
        // (legacy bEdit stays FALSE): the step is adopted as-is and
        // the month-scan clocks stay.
        TournamentRegistry reg2;
        const auto res2 = reg2.SetTournamentTime(steps, bt, 4,
            false, true, now, 4, 0, tnmt::kStepQFinal);
        EXPECT(res2.id == 4);
        EXPECT(reg2.CurrentId() == 4);
        EXPECT(reg2.CurrentStep() == tnmt::kStepQFinal);
        const auto rows2 = reg2.ScheduleList();
        if (!rows2.empty() && !rows2[0].steps.empty())
        {
            const std::int64_t expect_start =
                NthWeekdayOfMonth(2026, 3, 4, 2) + 3600;
            EXPECT(rows2[0].steps[0].start == expect_start);
        }
    }

    // ---- boot + wire ------------------------------------------------
    boost::asio::io_context io;
    tworldsvr::CharRegistry              chars;
    tworldsvr::GuildRegistry             guilds;
    tworldsvr::PeerRegistry              peers;
    tworldsvr::CtrlSvrSlot               ctrl_svr;
    tworldsvr::TournamentRegistry        tournaments;
    tworldsvr::FakeTournamentRepository  tournament_repo;

    const std::int64_t boot_now =
        static_cast<std::int64_t>(std::time(nullptr));

    // Event tournament 5: 1st Sunday, two steps, one entry with one
    // reward, one seeded char for PLAYERADD.
    {
        tournament_repo.SeedMaxLevel(137);
        tworldsvr::TnmtEventTimeRow t{};
        t.tour_id = 5;
        t.time.week = 1; t.time.day = 1; t.time.start_sec = 7200;
        tournament_repo.SeedEventTime(t);
        tournament_repo.SeedEventSchedule(5,
            {tworldsvr::TnmtEventStepRow{tnmt::kStepEnter, 600},
             tworldsvr::TnmtEventStepRow{tnmt::kStepEnd, 0}});
        tworldsvr::TournamentEntrySeed e{};
        e.entry_id = 2; e.name = "Duel"; e.type = tnmt::kEntryPrivate;
        e.cls = 0x3F; e.fee = 500; e.fee_back = 400;
        e.permit_item_id = 0; e.permit_count = 0;
        e.min_level = 20; e.max_level = 90;
        tournament_repo.SeedEventEntries(5, {e});
        tournament_repo.SeedEventRewards(5,
            {tworldsvr::TnmtRewardRow{2,
                 tworldsvr::TnmtReward{1, 777, 3, 0x0F, 1}}});
        tournament_repo.SeedChar(
            tworldsvr::TnmtPlayerBrief{42, 1, "Alice", 60, 3});
    }

    tworldsvr::LoadTournamentState(tournaments, tournament_repo,
        boot_now);
    EXPECT(tournaments.HasSchedule(5));
    EXPECT(tournaments.HasTournament(5));
    EXPECT(tournaments.ActiveScheduleId() == 5);
    EXPECT(tournaments.CurrentId() == 5);
    EXPECT(tournaments.CurrentStep() == tnmt::kStepReady);
    EXPECT(tournaments.NextScheduleId() == 5);

    // Guild-name lookup source for PLAYERADD.
    {
        auto c = std::make_shared<tworldsvr::TChar>();
        c->char_id  = 42;
        c->name     = "Alice";
        c->guild_id = 900;
        chars.Insert(c);
        auto g = std::make_shared<tworldsvr::TGuild>();
        g->id   = 900;
        g->name = "Order";
        guilds.Insert(g);
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
    tcp::socket p1(client_io), pc(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep); pc.connect(ep);
    std::this_thread::sleep_for(20ms);

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0442));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(pc, ToUint16(MessageId::CT_CTRLSVR_REQ), {});
    std::this_thread::sleep_for(20ms);
    EXPECT(ctrl_svr.Get() != nullptr);

    // --- W6-50a: TET_SCHEDULEADD, new id -----------------------------
    // Day 2 avoids the boot schedule's day-1 window.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 101);
        wire::WritePOD<std::uint8_t>(b, tnmt::kTetScheduleAdd);
        wire::WritePOD<std::uint16_t>(b, 0);       // new id
        wire::WritePOD<std::uint8_t>(b, 1);        // week
        wire::WritePOD<std::uint8_t>(b, 2);        // day (Monday)
        wire::WritePOD<std::uint32_t>(b, 3600);    // start_sec
        wire::WritePOD<std::uint8_t>(b, 2);        // step count
        wire::WritePOD<std::uint8_t>(b, tnmt::kStepEnter);
        wire::WritePOD<std::uint32_t>(b, 600);
        wire::WritePOD<std::uint8_t>(b, tnmt::kStepEnd);
        wire::WritePOD<std::uint32_t>(b, 0);
        SendFramed(pc, ToUint16(MessageId::CT_TOURNAMENTEVENT_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_TOURNAMENTEVENT_ACK));
        wire::Reader r(b);
        std::uint32_t mgid = 0; std::uint8_t type = 0;
        std::uint16_t new_id = 0;
        r.Read(mgid); r.Read(type); r.Read(new_id);
        EXPECT(mgid == 101);
        EXPECT(type == tnmt::kTetScheduleAdd);
        EXPECT(new_id == 6);
        EXPECT(r.Eof());
    }
    // The schedule persist runs after the ack ships — give the
    // handler coroutine a beat to finish.
    std::this_thread::sleep_for(30ms);
    EXPECT(tournaments.HasSchedule(6));
    // New-id adds skip the re-election (legacy `if(wTID)` gate).
    EXPECT(tournaments.ActiveScheduleId() == 5);
    {
        const auto calls = tournament_repo.SaveScheduleCalls();
        EXPECT(calls.size() == 1);
        if (!calls.empty())
        {
            EXPECT(calls[0].tour_id == 6);
            EXPECT(calls[0].time.week == 1 && calls[0].time.day == 2);
            EXPECT(calls[0].time.start_sec == 3600);
            EXPECT(calls[0].steps.size() == 2);
            if (calls[0].steps.size() == 2)
            {
                EXPECT(calls[0].steps[0].step == tnmt::kStepEnter);
                EXPECT(calls[0].steps[0].period == 600);
                EXPECT(calls[0].steps[1].step == tnmt::kStepEnd);
            }
        }
    }

    // --- W6-50b: TET_SCHEDULEADD overlap → id 0, no persist ----------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 102);
        wire::WritePOD<std::uint8_t>(b, tnmt::kTetScheduleAdd);
        wire::WritePOD<std::uint16_t>(b, 0);
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WritePOD<std::uint8_t>(b, 2);        // same window as 6
        wire::WritePOD<std::uint32_t>(b, 3600);
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WritePOD<std::uint8_t>(b, tnmt::kStepEnter);
        wire::WritePOD<std::uint32_t>(b, 300);
        SendFramed(pc, ToUint16(MessageId::CT_TOURNAMENTEVENT_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_TOURNAMENTEVENT_ACK));
        wire::Reader r(b);
        std::uint32_t mgid = 0; std::uint8_t type = 0;
        std::uint16_t new_id = 0xFFFF;
        r.Read(mgid); r.Read(type); r.Read(new_id);
        EXPECT(mgid == 102);
        EXPECT(new_id == 0);
    }
    EXPECT(tournaments.NextScheduleId() == 6);
    EXPECT(tournament_repo.SaveScheduleCalls().size() == 1);

    // --- W6-50c: TET_ENTRYADD for id 6 --------------------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 103);
        wire::WritePOD<std::uint8_t>(b, tnmt::kTetEntryAdd);
        wire::WritePOD<std::uint16_t>(b, 6);
        wire::WritePOD<std::uint8_t>(b, 1);        // entry count
        wire::WritePOD<std::uint8_t>(b, 1);        // entry id
        wire::WriteString(b, "Cup");
        wire::WritePOD<std::uint8_t>(b, tnmt::kEntryPrivate);
        wire::WritePOD<std::uint32_t>(b, 0x0F);    // class
        wire::WritePOD<std::uint32_t>(b, 1000);    // fee
        wire::WritePOD<std::uint32_t>(b, 800);     // fee back
        wire::WritePOD<std::uint16_t>(b, 77);      // permit item
        wire::WritePOD<std::uint8_t>(b, 2);        // permit count
        wire::WritePOD<std::uint8_t>(b, 10);       // min level
        wire::WritePOD<std::uint8_t>(b, 90);       // max level
        wire::WritePOD<std::uint8_t>(b, 1);        // reward count
        wire::WritePOD<std::uint8_t>(b, 1);        // chart type
        wire::WritePOD<std::uint16_t>(b, 500);     // item
        wire::WritePOD<std::uint8_t>(b, 2);        // count
        wire::WritePOD<std::uint32_t>(b, 0xF0);    // class
        wire::WritePOD<std::uint8_t>(b, 1);        // shield
        SendFramed(pc, ToUint16(MessageId::CT_TOURNAMENTEVENT_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_TOURNAMENTEVENT_ACK));
        wire::Reader r(b);
        std::uint32_t mgid = 0; std::uint8_t type = 0;
        r.Read(mgid); r.Read(type);
        EXPECT(mgid == 103);
        EXPECT(type == tnmt::kTetEntryAdd);
        EXPECT(r.Eof());
    }
    // The entry persist runs after the ack ships.
    std::this_thread::sleep_for(30ms);
    EXPECT(tournaments.HasTournament(6));
    {
        const auto calls = tournament_repo.SaveEntriesCalls();
        EXPECT(calls.size() == 1);
        if (!calls.empty())
        {
            EXPECT(calls[0].tour_id == 6);
            EXPECT(calls[0].entries.size() == 1);
            if (!calls[0].entries.empty())
            {
                const auto& e = calls[0].entries[0];
                EXPECT(e.entry_id == 1 && e.name == "Cup");
                EXPECT(e.fee == 1000 && e.fee_back == 800);
                EXPECT(e.rewards.size() == 1);
                if (!e.rewards.empty())
                    EXPECT(e.rewards[0].item_id == 500 &&
                           e.rewards[0].cls == 0xF0);
            }
        }
    }

    // --- W6-50d: TET_LIST byte-walk -----------------------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 104);
        wire::WritePOD<std::uint8_t>(b, tnmt::kTetList);
        SendFramed(pc, ToUint16(MessageId::CT_TOURNAMENTEVENT_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_TOURNAMENTEVENT_ACK));
        wire::Reader r(b);
        std::uint32_t mgid = 0; std::uint8_t type = 0, tcount = 0;
        r.Read(mgid); r.Read(type); r.Read(tcount);
        EXPECT(mgid == 104);
        EXPECT(type == tnmt::kTetList);
        EXPECT(tcount == 2);                      // times: ids 5, 6
        for (std::uint8_t i = 0; i < tcount; ++i)
        {
            std::uint16_t tid = 0; std::uint8_t week = 0, day = 0;
            std::uint32_t start = 0; std::uint8_t scount = 0;
            r.Read(tid); r.Read(week); r.Read(day); r.Read(start);
            r.Read(scount);
            if (i == 0)
            {
                EXPECT(tid == 5 && week == 1 && day == 1 &&
                       start == 7200);
                EXPECT(scount == 2);
            }
            else
            {
                EXPECT(tid == 6 && week == 1 && day == 2 &&
                       start == 3600);
                EXPECT(scount == 2);
            }
            for (std::uint8_t s = 0; s < scount; ++s)
            {
                std::uint8_t step = 0; std::uint32_t period = 0;
                std::int64_t dstart = 0;
                r.Read(step); r.Read(period); r.Read(dstart);
                EXPECT(dstart > 0 || period == 0);
            }
        }
        std::uint8_t cat_count = 0;
        r.Read(cat_count);
        EXPECT(cat_count == 2);                   // catalogue: 5, 6
        // Tournament 5 (boot-seeded).
        {
            std::uint16_t tid = 0; std::uint8_t ecount = 0;
            r.Read(tid); r.Read(ecount);
            EXPECT(tid == 5 && ecount == 1);
            std::uint8_t eid = 0; std::string name;
            r.Read(eid); r.ReadString(name);
            EXPECT(eid == 2 && name == "Duel");
            std::uint8_t etype = 0; std::uint32_t cls = 0, fee = 0,
                feeb = 0;
            std::uint16_t permit = 0;
            std::uint8_t pcount = 0, minl = 0, maxl = 0, rcount = 0;
            r.Read(etype); r.Read(cls); r.Read(fee); r.Read(feeb);
            r.Read(permit); r.Read(pcount); r.Read(minl);
            r.Read(maxl); r.Read(rcount);
            EXPECT(etype == tnmt::kEntryPrivate && cls == 0x3F);
            EXPECT(fee == 500 && feeb == 400);
            EXPECT(minl == 20 && maxl == 90);
            EXPECT(rcount == 1);
            std::uint8_t chart = 0; std::uint16_t item = 0;
            std::uint8_t rc = 0; std::uint32_t rcls = 0;
            std::uint8_t shield = 0;
            r.Read(chart); r.Read(item); r.Read(rc); r.Read(rcls);
            r.Read(shield);
            EXPECT(chart == 1 && item == 777 && rc == 3 &&
                   rcls == 0x0F && shield == 1);
            std::uint8_t players = 0;
            r.Read(players);
            EXPECT(players == 0);
        }
        // Tournament 6 ("Cup" from W6-50c).
        {
            std::uint16_t tid = 0; std::uint8_t ecount = 0;
            r.Read(tid); r.Read(ecount);
            EXPECT(tid == 6 && ecount == 1);
            std::uint8_t eid = 0; std::string name;
            r.Read(eid); r.ReadString(name);
            EXPECT(eid == 1 && name == "Cup");
        }
    }

    // --- W6-50e: TET_PLAYERADD ----------------------------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 105);
        wire::WritePOD<std::uint8_t>(b, tnmt::kTetPlayerAdd);
        wire::WritePOD<std::uint16_t>(b, 5);      // current tournament
        wire::WritePOD<std::uint8_t>(b, 2);       // entry "Duel"
        wire::WriteString(b, "Alice");
        SendFramed(pc, ToUint16(MessageId::CT_TOURNAMENTEVENT_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_TOURNAMENTEVENT_ACK));
        wire::Reader r(b);
        std::uint32_t mgid = 0; std::uint8_t type = 0, eid = 0;
        std::uint32_t char_id = 0; std::string name;
        std::uint8_t level = 0, cls = 0, country = 0;
        r.Read(mgid); r.Read(type); r.Read(eid); r.Read(char_id);
        r.ReadString(name); r.Read(level); r.Read(cls);
        r.Read(country);
        EXPECT(mgid == 105);
        EXPECT(type == tnmt::kTetPlayerAdd);
        EXPECT(eid == 2);
        EXPECT(char_id == 42);
        EXPECT(name == "Alice");
        EXPECT(level == 60 && cls == 3 && country == 1);
    }
    EXPECT(tournaments.HasPlayer(42));
    EXPECT(tournaments.PlayerCount() == 1);
    {
        const auto calls = tournament_repo.ApplyCalls();
        EXPECT(calls.size() == 1);
        if (!calls.empty())
        {
            EXPECT(calls[0].add);
            EXPECT(calls[0].char_id == 42);
            EXPECT(calls[0].entry_id == 2);
            EXPECT(calls[0].chief_id == 42);
        }
    }

    // Wrong tournament id → zeroed echo (guard branch).
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 106);
        wire::WritePOD<std::uint8_t>(b, tnmt::kTetPlayerAdd);
        wire::WritePOD<std::uint16_t>(b, 99);
        wire::WritePOD<std::uint8_t>(b, 2);
        wire::WriteString(b, "Alice");
        SendFramed(pc, ToUint16(MessageId::CT_TOURNAMENTEVENT_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_TOURNAMENTEVENT_ACK));
        wire::Reader r(b);
        std::uint32_t mgid = 0; std::uint8_t type = 0, eid = 0;
        std::uint32_t char_id = 1; std::string name;
        std::uint8_t level = 9, cls = 9, country = 9;
        r.Read(mgid); r.Read(type); r.Read(eid); r.Read(char_id);
        r.ReadString(name); r.Read(level); r.Read(cls);
        r.Read(country);
        EXPECT(mgid == 106);
        EXPECT(char_id == 0);
        EXPECT(name.empty());
        EXPECT(level == 0);
        EXPECT(cls == tnmt::kClassCount);
        EXPECT(country == tnmt::kCountryN);
    }

    // Duplicate add → zeroed echo through the ACK-side guard.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 107);
        wire::WritePOD<std::uint8_t>(b, tnmt::kTetPlayerAdd);
        wire::WritePOD<std::uint16_t>(b, 5);
        wire::WritePOD<std::uint8_t>(b, 2);
        wire::WriteString(b, "Alice");
        SendFramed(pc, ToUint16(MessageId::CT_TOURNAMENTEVENT_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(pc);
        wire::Reader r(b);
        std::uint32_t mgid = 0; std::uint8_t type = 0, eid = 0;
        std::uint32_t char_id = 1;
        r.Read(mgid); r.Read(type); r.Read(eid); r.Read(char_id);
        EXPECT(mgid == 107);
        EXPECT(char_id == 0);
    }
    EXPECT(tournament_repo.ApplyCalls().size() == 1);

    // --- W6-50f: TET_PLAYERDEL ----------------------------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 108);
        wire::WritePOD<std::uint8_t>(b, tnmt::kTetPlayerDel);
        wire::WritePOD<std::uint16_t>(b, 5);
        wire::WritePOD<std::uint8_t>(b, 2);
        wire::WriteString(b, "Alice");
        SendFramed(pc, ToUint16(MessageId::CT_TOURNAMENTEVENT_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_TOURNAMENTEVENT_ACK));
        wire::Reader r(b);
        std::uint32_t mgid = 0; std::uint8_t type = 0, eid = 0;
        std::uint32_t char_id = 0;
        r.Read(mgid); r.Read(type); r.Read(eid); r.Read(char_id);
        EXPECT(mgid == 108);
        EXPECT(type == tnmt::kTetPlayerDel);
        EXPECT(eid == 2);
        EXPECT(char_id == 42);
        EXPECT(r.Eof());
    }
    EXPECT(!tournaments.HasPlayer(42));
    {
        const auto calls = tournament_repo.ApplyCalls();
        EXPECT(calls.size() == 2);
        if (calls.size() == 2)
        {
            EXPECT(!calls[1].add);
            EXPECT(calls[1].char_id == 42);
            EXPECT(calls[1].entry_id == 0);   // legacy default args
            EXPECT(calls[1].chief_id == 0);
        }
    }

    // --- W6-50g: TET_SCHEDULEDEL for 6 --------------------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 109);
        wire::WritePOD<std::uint8_t>(b, tnmt::kTetScheduleDel);
        wire::WritePOD<std::uint16_t>(b, 6);
        SendFramed(pc, ToUint16(MessageId::CT_TOURNAMENTEVENT_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_TOURNAMENTEVENT_ACK));
        wire::Reader r(b);
        std::uint32_t mgid = 0; std::uint8_t type = 0;
        std::uint16_t tid = 0;
        r.Read(mgid); r.Read(type); r.Read(tid);
        EXPECT(mgid == 109);
        EXPECT(type == tnmt::kTetScheduleDel);
        EXPECT(tid == 6);
    }
    EXPECT(!tournaments.HasSchedule(6));
    EXPECT(!tournaments.HasTournament(6));
    {
        const auto dels = tournament_repo.DeleteEventCalls();
        EXPECT(dels.size() == 1);
        if (!dels.empty())
            EXPECT(dels[0] == 6);
    }
    // Election kept tournament 5 current → no MW_TOURNAMENTINFO
    // broadcast (verified below: p1's next frame is the 50h one).

    // --- W6-50h: SM_TOURNAMENTUPDATE_REQ rebuild + broadcast ----------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint16_t>(b, 5);
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WritePOD<std::uint8_t>(b, 0);              // group
        wire::WritePOD<std::uint8_t>(b, tnmt::kStepEnter);
        wire::WritePOD<std::uint32_t>(b, 600);
        wire::WritePOD<std::int64_t>(b, boot_now + 100);
        SendFramed(p1, ToUint16(MessageId::SM_TOURNAMENTUPDATE_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_TOURNAMENTINFO_REQ));
        wire::Reader r(b);
        std::uint8_t fgc = 0xFF, group = 0xFF, step = 0xFF,
            ecount = 0;
        r.Read(fgc); r.Read(group); r.Read(step); r.Read(ecount);
        EXPECT(fgc == 0);                 // main unscheduled at boot
        EXPECT(group == 0);
        EXPECT(step == tnmt::kStepReady); // rebuild resets the step
        EXPECT(ecount == 1);
        std::uint8_t egroup = 0, eid = 0;
        std::string  name;
        r.Read(egroup); r.Read(eid); r.ReadString(name);
        EXPECT(egroup == 0 && eid == 2 && name == "Duel");
        std::uint8_t etype = 0; std::uint32_t cls = 0, fee = 0,
            feeb = 0;
        std::uint16_t permit = 0;
        std::uint8_t pcount = 0, minl = 0, maxl = 0, rcount = 0;
        r.Read(etype); r.Read(cls); r.Read(fee); r.Read(feeb);
        r.Read(permit); r.Read(pcount); r.Read(minl); r.Read(maxl);
        r.Read(rcount);
        EXPECT(etype == tnmt::kEntryPrivate);
        EXPECT(minl == 20 && maxl == 90);
        EXPECT(rcount == 1);
        std::uint8_t chart = 0; std::uint16_t item = 0;
        std::uint8_t rc = 0;
        r.Read(chart); r.Read(item); r.Read(rc);
        EXPECT(chart == 1 && item == 777 && rc == 3);
        EXPECT(r.Eof());                  // rewards carry no cls/shield
    }

    // --- W6-50i: TET_SCHEDULEDEL of the last schedule -----------------
    // Election finds nothing → rebuild to none + info broadcast with
    // zero entries + the DM_TOURNAMENTCLEAR persist.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 110);
        wire::WritePOD<std::uint8_t>(b, tnmt::kTetScheduleDel);
        wire::WritePOD<std::uint16_t>(b, 5);
        SendFramed(pc, ToUint16(MessageId::CT_TOURNAMENTEVENT_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(p1);     // broadcast reaches the map
        EXPECT(w == ToUint16(MessageId::MW_TOURNAMENTINFO_REQ));
        wire::Reader r(b);
        std::uint8_t fgc = 0xFF, group = 0xFF, step = 0xFF,
            ecount = 0xFF;
        r.Read(fgc); r.Read(group); r.Read(step); r.Read(ecount);
        EXPECT(ecount == 0);
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_TOURNAMENTEVENT_ACK));
        wire::Reader r(b);
        std::uint32_t mgid = 0; std::uint8_t type = 0;
        std::uint16_t tid = 0;
        r.Read(mgid); r.Read(type); r.Read(tid);
        EXPECT(mgid == 110 && tid == 5);
    }
    EXPECT(tournaments.CurrentId() == 0);
    EXPECT(tournaments.ActiveScheduleId() == 0);
    EXPECT(tournaments.PlayerCount() == 0);
    EXPECT(tournament_repo.ClearCalls() >= 1);
    {
        const auto dels = tournament_repo.DeleteEventCalls();
        EXPECT(dels.size() == 2);
        if (dels.size() == 2)
            EXPECT(dels[1] == 5);
    }

    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("test_tournament_handlers: all passed\n");
    else
        std::fprintf(stderr, "test_tournament_handlers: %d failure(s)\n",
            g_fails);
    return g_fails == 0 ? 0 : 1;
}
