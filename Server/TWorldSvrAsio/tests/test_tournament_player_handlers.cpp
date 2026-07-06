// W6-52 wire tests: tournament player vertical (MW_TOURNAMENT_ACK).
//
//   Guards: unknown char / key mismatch silent drops.
//   SCHEDULE: current ladder echo (zero-period steps filtered).
//   APPLY: TIMEOUT at READY, first-grade SUCCESS + DISQUALIFY at
//     the 1st step, NORMAL success (+ APPLYINFO follow-up
//     byte-walk incl. class-filtered rewards + 0xFF max-level
//     translation), already-registered short SUCCESS, HWID dup
//     FAIL, country FAIL.
//   JOINLIST: PARTY-step gate + listing.
//   PARTY ops: online add success (+ PARTYLIST echo + persist),
//     LEVEL band refusal with name, country NOTFOUND, offline
//     repo-lookup add, unknown-name NOTFOUND, duplicate
//     ALREADYREG, del by chief (+ persist + PARTYLIST), del-chief
//     silent.
//   MATCHLIST: bracket roster with slots + per-match results.
//   Boot: TVIEW player reload two-pass (first-grade routing, party
//     attach, orphan drop).

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/fake_tournament_repository.h"
#include "../services/guild_registry.h"
#include "../services/month_rank_registry.h"
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

// MW_TOURNAMENT_ACK request head.
std::vector<std::byte> AckHead(std::uint32_t char_id,
                               std::uint32_t key,
                               std::uint16_t proto)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint32_t>(b, char_id);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, key);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, proto);
    return b;
}

// Read + verify a MW_TOURNAMENT_REQ reply head; returns a Reader
// positioned at the tail.
struct ReplyFrame
{
    std::vector<std::byte> body;
};

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
    using tworldsvr::TnmtPlayerBrief;

    const std::int64_t base_now =
        static_cast<std::int64_t>(std::time(nullptr));

    boost::asio::io_context io;
    tworldsvr::CharRegistry              chars;
    tworldsvr::GuildRegistry             guilds;
    tworldsvr::PeerRegistry              peers;
    tworldsvr::CtrlSvrSlot               ctrl_svr;
    tworldsvr::MonthRankRegistry         month_rank;
    tworldsvr::TournamentRegistry        tournaments;
    tworldsvr::FakeTournamentRepository  tournament_repo;

    // ---- state: tournament 5, two entries ---------------------------
    {
        tworldsvr::TournamentEntrySeed e1{};
        e1.entry_id = 1; e1.name = "Solo";
        e1.type = tnmt::kEntryPrivate;
        e1.cls = 0x3F; e1.fee = 500; e1.fee_back = 400;
        e1.permit_count = 2; e1.min_level = 10; e1.max_level = 0xFF;
        e1.rewards.push_back(
            tworldsvr::TnmtReward{1, 100, 1, 0x08, 1});  // class 3
        e1.rewards.push_back(
            tworldsvr::TnmtReward{2, 200, 2, 0x01, 0});  // class 0
        tworldsvr::TournamentEntrySeed e2{};
        e2.entry_id = 2; e2.name = "Party";
        e2.type = tnmt::kEntryParty;
        e2.cls = 0x3F; e2.min_level = 20; e2.max_level = 60;
        tournaments.SeedEntries(5, {e1, e2});
        tournaments.SetMaxLevel(137);
        tournaments.SetFirstGroupCount(3);

        std::vector<TournamentStep> steps;
        steps.push_back(
            TournamentStep{0, tnmt::kStepEnter, 600, base_now + 100, 0});
        steps.push_back(
            TournamentStep{0, tnmt::kStepQFinal, 0, 0, 0});
        tournaments.RebuildCurrent(5, steps);
    }

    // Fame first-grade: Alice (42) in slot 1 of country 1.
    {
        tworldsvr::MonthRankRegistry::FirstGrade fg{};
        fg[1][1].char_id = 42;
        month_rank.SetFirstGradeGroup(fg);
    }

    // ---- chars -------------------------------------------------------
    auto add_char = [&](std::uint32_t id, std::uint32_t key,
                        std::uint8_t country, std::uint8_t level,
                        std::uint8_t klass, const char* name,
                        std::uint32_t guild_id) {
        auto c = std::make_shared<tworldsvr::TChar>();
        c->char_id = id; c->key = key; c->country = country;
        c->level = level; c->klass = klass; c->name = name;
        c->guild_id = guild_id;
        chars.Insert(c);
        chars.Rename(id, name);   // Insert alone skips the name index
    };
    add_char(42, 0xA11CE, 1, 30, 3, "Alice", 900);
    add_char(43, 0xB0B00, 1, 30, 0, "Bob", 0);
    add_char(45, 0xDA7A0, 2, 30, 1, "Dana", 0);
    add_char(46, 0xE7E00, 1, 99, 1, "Eve", 0);
    add_char(47, 0x61A00, 3, 30, 1, "Gina", 0);
    {
        auto g = std::make_shared<tworldsvr::TGuild>();
        g->id = 900; g->name = "Order";
        guilds.Insert(g);
    }
    // Offline target for the repo-lookup path.
    tournament_repo.SeedChar(TnmtPlayerBrief{44, 1, "Frank", 35, 2});

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.ctrl_svr = &ctrl_svr;
    ctx.month_rank = &month_rank;
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
    tcp::socket p1(client_io);
    p1.connect(tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port));
    std::this_thread::sleep_for(20ms);
    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0442));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    // W6-41 replays the month-rank table to every joining peer when
    // ctx.month_rank is wired — drain it.
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_MONTHRANKLIST_REQ)); }

    const auto kAck = ToUint16(MessageId::MW_TOURNAMENT_ACK);
    const auto kReq = ToUint16(MessageId::MW_TOURNAMENT_REQ);

    // --- W6-52a: key mismatch + unknown char drops -------------------
    {
        auto b = AckHead(42, 0xDEAD,
            ToUint16(MessageId::MW_TOURNAMENTSCHEDULE_ACK));
        SendFramed(p1, kAck, b);
        b = AckHead(9999, 1,
            ToUint16(MessageId::MW_TOURNAMENTSCHEDULE_ACK));
        SendFramed(p1, kAck, b);
    }
    std::this_thread::sleep_for(30ms);

    // --- W6-52b: SCHEDULE echo (first frame → proves 52a drops) ------
    {
        SendFramed(p1, kAck, AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTSCHEDULE_ACK)));
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kReq);
        wire::Reader r(b);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t group = 0xFF, step = 0xFF, count = 0xFF;
        r.Read(id); r.Read(key); r.Read(proto);
        r.Read(group); r.Read(step); r.Read(count);
        EXPECT(id == 42 && key == 0xA11CE);
        EXPECT(proto ==
            ToUint16(MessageId::MW_TOURNAMENTSCHEDULE_REQ));
        EXPECT(group == 0 && step == tnmt::kStepReady);
        EXPECT(count == 1);                 // zero-period filtered
        std::uint8_t sg = 0xFF, ss = 0; std::int64_t start = 0;
        r.Read(sg); r.Read(ss); r.Read(start);
        EXPECT(sg == 0 && ss == tnmt::kStepEnter);
        EXPECT(start == base_now + 100);
        EXPECT(r.Eof());
    }

    // --- W6-52c: APPLY at READY → TIMEOUT ----------------------------
    {
        auto b = AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTAPPLY_ACK));
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WriteString(b, "HW-A");
        wire::WritePOD<std::uint32_t>(b, 0x01020304);
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        EXPECT(w == kReq);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t result = 0xFF;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(result);
        EXPECT(proto == ToUint16(MessageId::MW_TOURNAMENTAPPLY_REQ));
        EXPECT(result == tnmt::kResultTimeout);
        EXPECT(r.Eof());
    }

    // --- W6-52d: 1st-step APPLY — first-grade in, others out ---------
    EXPECT(tournaments.AdvanceStep(5, 0, tnmt::kStep1st).has_value());
    {
        // Alice is fame-slot 1 → SUCCESS into the 1st pool.
        auto b = AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTAPPLY_ACK));
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WriteString(b, "HW-A");
        wire::WritePOD<std::uint32_t>(b, 0x01020304);
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        EXPECT(w == kReq);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t result = 0xFF, entry = 0;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(result);
        r.Read(entry);
        EXPECT(result == tnmt::kResultSuccess);
        EXPECT(entry == 1);
        EXPECT(r.Eof());

        // APPLYINFO follow-up: entry 1 applied, 1st pool has Alice,
        // free slots 7, class-3 reward only, 0xFF → 137.
        auto [w2, rb2] = ReadFramed(p1);
        EXPECT(w2 == kReq);
        wire::Reader r2(rb2);
        std::uint16_t proto2 = 0; std::uint8_t ecount = 0;
        r2.Read(id); r2.Read(key); r2.Read(proto2); r2.Read(ecount);
        EXPECT(proto2 ==
            ToUint16(MessageId::MW_TOURNAMENTAPPLYINFO_REQ));
        EXPECT(ecount == 2);
        // entry 1
        std::uint8_t group = 0, eid = 0; std::string name;
        std::uint8_t type = 0; std::uint32_t cls = 0;
        std::uint8_t applied = 0; std::uint32_t fee = 0, feeb = 0;
        std::uint8_t permit = 0, minl = 0, maxl = 0, free1 = 0;
        std::uint16_t normal = 0; std::uint8_t rcount = 0;
        r2.Read(group); r2.Read(eid); r2.ReadString(name);
        r2.Read(type); r2.Read(cls); r2.Read(applied);
        r2.Read(fee); r2.Read(feeb); r2.Read(permit);
        r2.Read(minl); r2.Read(maxl); r2.Read(free1);
        r2.Read(normal); r2.Read(rcount);
        EXPECT(eid == 1 && name == "Solo");
        EXPECT(applied == 1);
        EXPECT(fee == 500 && feeb == 400 && permit == 2);
        EXPECT(minl == 10);
        EXPECT(maxl == 137);            // 0xFF translated
        EXPECT(free1 == 7);
        EXPECT(normal == 0);
        EXPECT(rcount == 1);            // class-3 filter
        std::uint8_t shield = 0, chart = 0, rc = 0;
        std::uint16_t item = 0;
        r2.Read(shield); r2.Read(chart); r2.Read(item); r2.Read(rc);
        EXPECT(shield == 1 && chart == 1 && item == 100 && rc == 1);
        std::uint8_t pcount = 0;
        r2.Read(pcount);
        EXPECT(pcount == 1);
        std::uint32_t pid = 0; std::uint8_t pcountry = 0;
        std::string pname; std::uint8_t plevel = 0, pcls = 0;
        std::uint32_t prank = 1, pmonth = 1;
        r2.Read(pid); r2.Read(pcountry); r2.ReadString(pname);
        r2.Read(plevel); r2.Read(pcls); r2.Read(prank);
        r2.Read(pmonth);
        EXPECT(pid == 42 && pname == "Alice");
        EXPECT(prank == 0 && pmonth == 0);   // dead legacy maps
    }
    std::this_thread::sleep_for(30ms);
    {
        const auto calls = tournament_repo.ApplyCalls();
        EXPECT(calls.size() == 1);
        if (!calls.empty())
        {
            EXPECT(calls[0].add && calls[0].char_id == 42);
            EXPECT(calls[0].entry_id == 1 && calls[0].chief_id == 42);
            EXPECT(calls[0].hwid == "HW-A");
            EXPECT(calls[0].ip_addr == 0x01020304);
        }
    }
    {
        // Bob is not first-grade → DISQUALIFY at the 1st step.
        auto b = AckHead(43, 0xB0B00,
            ToUint16(MessageId::MW_TOURNAMENTAPPLY_ACK));
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WriteString(b, "HW-B");
        wire::WritePOD<std::uint32_t>(b, 0x05060708);
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t result = 0xFF;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(result);
        EXPECT(result == tnmt::kResultDisqualify);
    }

    // --- W6-52e: NORMAL apply chain ----------------------------------
    {
        std::vector<TournamentStep> steps;
        steps.push_back(
            TournamentStep{0, tnmt::kStepEnter, 600, base_now + 100, 0});
        tournaments.RebuildCurrent(5, steps);   // clears players
        EXPECT(tournaments.PlayerCount() == 0);
        EXPECT(tournaments.AdvanceStep(5, 0,
            tnmt::kStepNormal).has_value());
    }
    {
        // Alice applies → SUCCESS into the normal pool.
        auto b = AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTAPPLY_ACK));
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WriteString(b, "HW-A");
        wire::WritePOD<std::uint32_t>(b, 0x01020304);
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t result = 0xFF, entry = 0;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(result);
        r.Read(entry);
        EXPECT(result == tnmt::kResultSuccess && entry == 1);
        auto [w2, rb2] = ReadFramed(p1);   // APPLYINFO follow-up
        EXPECT(w2 == kReq);
        wire::Reader r2(rb2);
        std::uint16_t proto2 = 0; std::uint8_t ecount = 0;
        r2.Read(id); r2.Read(key); r2.Read(proto2); r2.Read(ecount);
        std::uint8_t group = 0, eid = 0; std::string name;
        std::uint8_t type = 0; std::uint32_t cls = 0;
        std::uint8_t applied = 0; std::uint32_t fee = 0, feeb = 0;
        std::uint8_t permit = 0, minl = 0, maxl = 0, free1 = 0;
        std::uint16_t normal = 0;
        r2.Read(group); r2.Read(eid); r2.ReadString(name);
        r2.Read(type); r2.Read(cls); r2.Read(applied);
        r2.Read(fee); r2.Read(feeb); r2.Read(permit);
        r2.Read(minl); r2.Read(maxl); r2.Read(free1);
        r2.Read(normal);
        EXPECT(free1 == 8);            // 1st pool untouched
        EXPECT(normal == 1);           // normal pool
    }
    {
        // Re-apply → already-registered short SUCCESS.
        auto b = AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTAPPLY_ACK));
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WriteString(b, "HW-A");
        wire::WritePOD<std::uint32_t>(b, 0x01020304);
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t result = 0xFF;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(result);
        EXPECT(result == tnmt::kResultSuccess);
        EXPECT(r.Eof());               // short form, no entry byte
    }
    {
        // Bob with Alice's HWID → dup FAIL.
        auto b = AckHead(43, 0xB0B00,
            ToUint16(MessageId::MW_TOURNAMENTAPPLY_ACK));
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WriteString(b, "HW-A");
        wire::WritePOD<std::uint32_t>(b, 0x0A0B0C0D);
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t result = 0xFF;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(result);
        EXPECT(result == tnmt::kResultFail);
    }
    {
        // Gina (country 3 > TCONTRY_B) → FAIL.
        auto b = AckHead(47, 0x61A00,
            ToUint16(MessageId::MW_TOURNAMENTAPPLY_ACK));
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WriteString(b, "HW-G");
        wire::WritePOD<std::uint32_t>(b, 0x0F0F0F0F);
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t result = 0xFF;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(result);
        EXPECT(result == tnmt::kResultFail);
    }
    std::this_thread::sleep_for(30ms);
    EXPECT(tournament_repo.ApplyCalls().size() == 2);

    // --- W6-52f: JOINLIST gate + listing ------------------------------
    {
        // At NORMAL the PARTY gate blocks (silent).
        SendFramed(p1, kAck, AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTJOINLIST_ACK)));
        std::this_thread::sleep_for(30ms);
    }
    EXPECT(tournaments.AdvanceStep(5, 0,
        tnmt::kStepParty).has_value());
    {
        SendFramed(p1, kAck, AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTJOINLIST_ACK)));
        auto [w, rb] = ReadFramed(p1);   // proves the NORMAL drop
        EXPECT(w == kReq);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t ecount = 0;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(ecount);
        EXPECT(proto ==
            ToUint16(MessageId::MW_TOURNAMENTJOINLIST_REQ));
        EXPECT(ecount == 2);
        std::uint8_t group = 0, eid = 0; std::string name;
        std::uint8_t type = 0; std::uint32_t cls = 0;
        std::uint8_t applied = 0xFF, rcount = 0xFF, pcount = 0xFF;
        r.Read(group); r.Read(eid); r.ReadString(name);
        r.Read(type); r.Read(cls); r.Read(applied);
        r.Read(rcount);
        EXPECT(eid == 1 && applied == 1 && rcount == 1);
        std::uint8_t shield = 0, chart = 0, rc = 0;
        std::uint16_t item = 0;
        r.Read(shield); r.Read(chart); r.Read(item); r.Read(rc);
        r.Read(pcount);
        EXPECT(pcount == 0);          // select hasn't run yet
    }

    // --- W6-52g: party management -------------------------------------
    {
        // Online add: Bob.
        auto b = AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTPARTYADD_ACK));
        wire::WriteString(b, "Bob");
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        EXPECT(w == kReq);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t result = 0xFF; std::string name;
        std::uint32_t target = 0;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(result);
        r.ReadString(name); r.Read(target);
        EXPECT(proto ==
            ToUint16(MessageId::MW_TOURNAMENTPARTYADD_REQ));
        EXPECT(result == tnmt::kResultSuccess);
        EXPECT(name == "Bob" && target == 43);
        // PARTYLIST follow-up.
        auto [w2, rb2] = ReadFramed(p1);
        wire::Reader r2(rb2);
        std::uint16_t proto2 = 0; std::uint32_t chief = 0;
        std::uint8_t count = 0;
        r2.Read(id); r2.Read(key); r2.Read(proto2); r2.Read(chief);
        r2.Read(count);
        EXPECT(proto2 ==
            ToUint16(MessageId::MW_TOURNAMENTPARTYLIST_REQ));
        EXPECT(chief == 42 && count == 1);
        std::uint32_t mid = 0;
        r2.Read(mid);
        EXPECT(mid == 43);
    }
    {
        // Eve (level 99, band 20..60) → LEVEL with name.
        auto b = AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTPARTYADD_ACK));
        wire::WriteString(b, "Eve");
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t result = 0xFF; std::string name;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(result);
        r.ReadString(name);
        EXPECT(result == tnmt::kResultLevel);
        EXPECT(name == "Eve");
        EXPECT(r.Eof());
    }
    {
        // Dana (country 2 vs chief 1) → NOTFOUND.
        auto b = AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTPARTYADD_ACK));
        wire::WriteString(b, "Dana");
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t result = 0xFF;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(result);
        EXPECT(result == tnmt::kResultNotFound);
        EXPECT(r.Eof());
    }
    {
        // Offline Frank → repo lookup path.
        auto b = AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTPARTYADD_ACK));
        wire::WriteString(b, "Frank");
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t result = 0xFF; std::string name;
        std::uint32_t target = 0;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(result);
        r.ReadString(name); r.Read(target);
        EXPECT(result == tnmt::kResultSuccess);
        EXPECT(name == "Frank" && target == 44);
        auto [w2, rb2] = ReadFramed(p1);   // PARTYLIST (2 members)
        wire::Reader r2(rb2);
        std::uint16_t proto2 = 0; std::uint32_t chief = 0;
        std::uint8_t count = 0;
        r2.Read(id); r2.Read(key); r2.Read(proto2); r2.Read(chief);
        r2.Read(count);
        EXPECT(count == 2);
    }
    {
        // Unknown name → NOTFOUND (0-hit fan-in).
        auto b = AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTPARTYADD_ACK));
        wire::WriteString(b, "Nobody");
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t result = 0xFF;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(result);
        EXPECT(result == tnmt::kResultNotFound);
    }
    {
        // Bob again → ALREADYREG.
        auto b = AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTPARTYADD_ACK));
        wire::WriteString(b, "Bob");
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t result = 0xFF;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(result);
        EXPECT(result == tnmt::kResultAlreadyReg);
    }
    std::this_thread::sleep_for(30ms);
    {
        // [0] Alice 1st-step apply, [1] Alice NORMAL apply,
        // [2] Bob party add, [3] Frank party add.
        const auto calls = tournament_repo.ApplyCalls();
        EXPECT(calls.size() == 4);
        if (calls.size() == 4)
        {
            EXPECT(calls[2].add && calls[2].char_id == 43 &&
                   calls[2].entry_id == 2 && calls[2].chief_id == 42);
            EXPECT(calls[3].add && calls[3].char_id == 44 &&
                   calls[3].entry_id == 2 && calls[3].chief_id == 42);
        }
    }
    {
        // PARTYDEL of the chief herself → silent; of Bob → list.
        auto b = AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTPARTYDEL_ACK));
        wire::WritePOD<std::uint32_t>(b, 42);
        SendFramed(p1, kAck, b);
        std::this_thread::sleep_for(30ms);

        b = AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTPARTYDEL_ACK));
        wire::WritePOD<std::uint32_t>(b, 43);
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);   // proves the chief-del drop
        EXPECT(w == kReq);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint32_t chief = 0; std::uint8_t count = 0xFF;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(chief);
        r.Read(count);
        EXPECT(proto ==
            ToUint16(MessageId::MW_TOURNAMENTPARTYLIST_REQ));
        EXPECT(chief == 42 && count == 1);
        std::uint32_t mid = 0;
        r.Read(mid);
        EXPECT(mid == 44);              // Frank remains
    }
    std::this_thread::sleep_for(30ms);
    {
        const auto calls = tournament_repo.ApplyCalls();
        EXPECT(!calls.empty());
        const auto& last = calls.back();
        EXPECT(!last.add && last.char_id == 43);
    }
    EXPECT(!tournaments.HasPlayer(43));

    // --- W6-52h: MATCHLIST with a bracket roster ----------------------
    {
        TournamentRegistry::TnmtPlayerSeed seed{};
        seed.brief = TnmtPlayerBrief{50, 1, "Zed", 40, 3};
        seed.result[tnmt::kMatchQFinal] = tnmt::kWinWin;
        EXPECT(tournaments.AddPlayerAtStep(1, seed,
            tnmt::kStepMatch, 50));
    }
    {
        SendFramed(p1, kAck, AckHead(42, 0xA11CE,
            ToUint16(MessageId::MW_TOURNAMENTMATCHLIST_ACK)));
        auto [w, rb] = ReadFramed(p1);
        EXPECT(w == kReq);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t ecount = 0;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(ecount);
        EXPECT(proto ==
            ToUint16(MessageId::MW_TOURNAMENTMATCHLIST_REQ));
        EXPECT(ecount == 2);
        // entry 1 with Zed in the bracket.
        std::uint8_t group = 0, eid = 0; std::string name;
        std::uint8_t type = 0; std::uint32_t cls = 0;
        std::uint8_t applied = 0, rcount = 0;
        r.Read(group); r.Read(eid); r.ReadString(name);
        r.Read(type); r.Read(cls); r.Read(applied); r.Read(rcount);
        for (std::uint8_t i = 0; i < rcount; ++i)
        {
            std::uint8_t s8 = 0, c8 = 0, n8 = 0; std::uint16_t i16 = 0;
            r.Read(s8); r.Read(c8); r.Read(i16); r.Read(n8);
        }
        std::uint8_t pcount = 0;
        r.Read(pcount);
        EXPECT(pcount == 1);
        std::uint8_t slot = 0xFF;
        std::uint32_t pid = 0; std::uint8_t pcountry = 0;
        std::string pname; std::uint8_t plevel = 0, pcls = 0;
        std::uint32_t prank = 1, pmonth = 1;
        std::uint8_t r0 = 0xFF, r1v = 0xFF, r2v = 0xFF;
        r.Read(slot); r.Read(pid); r.Read(pcountry);
        r.ReadString(pname); r.Read(plevel); r.Read(pcls);
        r.Read(prank); r.Read(pmonth);
        r.Read(r0); r.Read(r1v); r.Read(r2v);
        EXPECT(slot == tnmt::kTournamentSlot);
        EXPECT(pid == 50 && pname == "Zed");
        EXPECT(r0 == tnmt::kWinWin && r1v == tnmt::kWinNone);
    }

    // --- W6-52i: boot player reload (registry-direct) -----------------
    {
        tworldsvr::TournamentRegistry       reg2;
        tworldsvr::FakeTournamentRepository repo2;
        tworldsvr::TnmtEventTimeRow t{};
        t.tour_id = 7;
        t.time.week = 1; t.time.day = 1; t.time.start_sec = 7200;
        repo2.SeedEventTime(t);
        repo2.SeedEventSchedule(7,
            {tworldsvr::TnmtEventStepRow{tnmt::kStep1st, 300},
             tworldsvr::TnmtEventStepRow{tnmt::kStepEnter, 600},
             tworldsvr::TnmtEventStepRow{tnmt::kStepEnd, 30}});
        tworldsvr::TournamentEntrySeed e{};
        e.entry_id = 2; e.name = "Cup"; e.type = tnmt::kEntryParty;
        repo2.SeedEventEntries(7, {e});
        repo2.SeedCurrentStep(tworldsvr::TnmtCurrentStep{
            7, 0, tnmt::kStepEnter});
        // Chief Hana (fame first-grade), member Ivan, orphan Jan.
        tworldsvr::TnmtPlayerRow hana{};
        hana.char_id = 60; hana.name = "Hana"; hana.chief_id = 60;
        hana.cls = 1; hana.country = 1; hana.level = 50;
        hana.entry_id = 2; hana.step = tnmt::kStepQFinal;
        hana.result = tnmt::kWinWin; hana.hwid = "h1";
        hana.ip_addr = 7;
        repo2.SeedPlayerRow(hana);
        tworldsvr::TnmtPlayerRow ivan{};
        ivan.char_id = 61; ivan.name = "Ivan"; ivan.chief_id = 60;
        ivan.cls = 2; ivan.country = 1; ivan.level = 48;
        ivan.entry_id = 2;
        repo2.SeedPlayerRow(ivan);
        tworldsvr::TnmtPlayerRow jan{};
        jan.char_id = 62; jan.name = "Jan"; jan.chief_id = 99;
        jan.entry_id = 2;
        repo2.SeedPlayerRow(jan);

        tworldsvr::LoadTournamentState(reg2, repo2, base_now,
            [](std::uint8_t country, std::uint32_t char_id)
            { return country == 1 && char_id == 60; });

        EXPECT(reg2.CurrentId() == 7);
        EXPECT(reg2.CurrentStep() == tnmt::kStepEnter);
        EXPECT(reg2.PlayerCount() == 2);
        EXPECT(reg2.HasPlayer(60));
        EXPECT(reg2.HasPlayer(61));
        EXPECT(!reg2.HasPlayer(62));    // orphan dropped
        // Ivan hangs off Hana's party.
        const auto party = reg2.PartyListReply(60);
        EXPECT(party.ok);
        EXPECT(party.members.size() == 1);
        if (!party.members.empty())
            EXPECT(party.members[0].char_id == 61);
        // Hana routed into the 1st pool (fame + 1st step present).
        EXPECT(reg2.AdvanceStep(7, 0, tnmt::kStepNormal).has_value());
        const auto info = reg2.ApplyInfoReply(60);
        EXPECT(info.ok);
        EXPECT(info.entries.size() == 1);
        if (!info.entries.empty())
        {
            EXPECT(info.entries[0].free_first == 7);
            EXPECT(info.entries[0].first_pool.size() == 1);
            if (!info.entries[0].first_pool.empty())
                EXPECT(info.entries[0].first_pool[0].char_id == 60);
        }
    }

    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("test_tournament_player_handlers: all passed\n");
    else
        std::fprintf(stderr,
            "test_tournament_player_handlers: %d failure(s)\n",
            g_fails);
    return g_fails == 0 ? 0 : 1;
}
