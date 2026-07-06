// W6-53 wire tests: tournament match / result / betting engine.
//
//   Registry-direct: the rival-country bracket pairing (8 first-pool
//     players from two countries → alternating countries on
//     even/odd slot pairs).
//   Wire: SM_TOURNAMENT_REQ at MATCH → select (party-member
//     fee-back payback: TTournamentPayback trace + the MW_POSTRECV
//     mail byte-walk) + the MW_TOURNAMENTMATCH_REQ roster broadcast;
//     MW_TOURNAMENTENTERGATE_ACK ticket banking (key-mismatch drop);
//     EVENTLIST / EVENTJOIN / EVENTINFO (join, re-join reset, the
//     integer-division rate quirk, payouts math);
//     MW_TOURNAMENTRESULT_ACK (result marks broadcast + FINAL-win
//     BATPOINT payout + TTournamentResult trace; non-bracket step
//     silence).
//   Boot: step>=PARTY resume re-runs the select before the member
//     pass (chief seeded into the bracket, member re-attached).

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

tworldsvr::TournamentRegistry::TnmtPlayerSeed
MakeSeed(std::uint32_t id, std::uint8_t country, const char* name)
{
    tworldsvr::TournamentRegistry::TnmtPlayerSeed s{};
    s.brief = tworldsvr::TnmtPlayerBrief{id, country, name, 40, 1};
    return s;
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

    const std::int64_t base_now =
        static_cast<std::int64_t>(std::time(nullptr));

    // ---- registry-direct: rival-country bracket pairing ------------
    {
        TournamentRegistry reg;
        tworldsvr::TournamentEntrySeed e{};
        e.entry_id = 1; e.name = "Solo"; e.type = tnmt::kEntryPrivate;
        reg.SeedEntries(5, {e});
        std::vector<TournamentStep> steps;
        steps.push_back(TournamentStep{0, tnmt::kStepMatch, 600,
            base_now + 50, 0});
        reg.RebuildCurrent(5, steps);

        // 4 country-1 + 4 country-2 first-pool chiefs.
        for (std::uint32_t id = 50; id < 54; ++id)
            EXPECT(reg.AddPlayerAtStep(1,
                MakeSeed(id, 1, ("c1_" + std::to_string(id)).c_str()),
                tnmt::kStep1st, id));
        for (std::uint32_t id = 56; id < 60; ++id)
            EXPECT(reg.AddPlayerAtStep(1,
                MakeSeed(id, 2, ("c2_" + std::to_string(id)).c_str()),
                tnmt::kStep1st, id));

        const auto dues = reg.SelectPlayers();
        EXPECT(dues.empty());
        EXPECT(reg.CurrentSelected());
        EXPECT(reg.PlayerCount() == 8);

        EXPECT(reg.AdvanceStep(5, 0, tnmt::kStepMatch).has_value());
        const auto match = reg.MatchReply();
        EXPECT(match.ok);
        EXPECT(match.rows.size() == 8);
        std::uint8_t slot_country[8] = {};
        bool slot_seen[8] = {};
        for (const auto& row : match.rows)
        {
            EXPECT(row.slot_id < 8);
            EXPECT(row.chief_id == row.row.char_id);
            if (row.slot_id < 8)
            {
                slot_country[row.slot_id] = row.row.country;
                slot_seen[row.slot_id] = true;
            }
        }
        for (int s = 0; s < 8; ++s)
            EXPECT(slot_seen[s]);
        // The odd-slot rival preference pairs opposite countries.
        for (int s = 0; s < 8; s += 2)
            EXPECT(slot_country[s] != slot_country[s + 1]);
    }

    // ---- wire harness ------------------------------------------------
    boost::asio::io_context io;
    tworldsvr::CharRegistry              chars;
    tworldsvr::GuildRegistry             guilds;
    tworldsvr::PeerRegistry              peers;
    tworldsvr::CtrlSvrSlot               ctrl_svr;
    tworldsvr::TournamentRegistry        tournaments;
    tworldsvr::FakeTournamentRepository  tournament_repo;

    // Tournament 5: entry 1 (private, fee_back 1'234'567) + entry 2
    // (party). Bracket source: 8 chiefs (2 countries) + Alice's party
    // member Mia (the deterministic fee-back leftover).
    {
        tworldsvr::TournamentEntrySeed e1{};
        e1.entry_id = 1; e1.name = "Solo";
        e1.type = tnmt::kEntryPrivate; e1.fee_back = 1234567;
        tworldsvr::TournamentEntrySeed e2{};
        e2.entry_id = 2; e2.name = "Party";
        e2.type = tnmt::kEntryParty;
        tournaments.SeedEntries(5, {e1, e2});

        std::vector<TournamentStep> steps;
        steps.push_back(TournamentStep{0, tnmt::kStepMatch, 600,
            base_now + 50, 0});
        tournaments.RebuildCurrent(5, steps);

        for (std::uint32_t id = 50; id < 54; ++id)
            EXPECT(tournaments.AddPlayerAtStep(1,
                MakeSeed(id, 1, ("c1_" + std::to_string(id)).c_str()),
                tnmt::kStep1st, id));
        for (std::uint32_t id = 56; id < 60; ++id)
            EXPECT(tournaments.AddPlayerAtStep(1,
                MakeSeed(id, 2, ("c2_" + std::to_string(id)).c_str()),
                tnmt::kStep1st, id));
        // Alice (chief 50)'s party member — unselected → fee-back.
        EXPECT(tournaments.AddPlayerAtStep(1, MakeSeed(61, 1, "Mia"),
            tnmt::kStepParty, 50));
    }

    // Online chars: Mia (payback mail target) + Beth (the bettor).
    auto add_char = [&](std::uint32_t id, std::uint32_t key,
                        const char* name, std::uint8_t msi) {
        auto c = std::make_shared<tworldsvr::TChar>();
        c->char_id = id; c->key = key; c->name = name;
        c->country = 1; c->level = 50; c->klass = 2;
        c->main_server_id = msi;
        chars.Insert(c);
        chars.Rename(id, name);
    };
    add_char(61, 0x0061, "Mia", 0x42);
    add_char(70, 0x0070, "Beth", 0x42);

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
    tcp::socket p1(client_io);
    p1.connect(tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port));
    std::this_thread::sleep_for(20ms);
    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0442));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    // W6-53 join replay (step READY < MATCH → info only).
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_TOURNAMENTINFO_REQ)); }

    // --- W6-53a: MATCH step advance → select + payback + roster ------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint16_t>(b, 5);
        wire::WritePOD<std::uint8_t>(b, 0);
        wire::WritePOD<std::uint8_t>(b, tnmt::kStepMatch);
        wire::WritePOD<std::uint32_t>(b, 600);
        SendFramed(p1, ToUint16(MessageId::SM_TOURNAMENT_REQ), b);
    }
    { // 1. ENABLE broadcast (the W6-51 leg).
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_TOURNAMENTENABLE_REQ));
    }
    { // 2. Mia's fee-back mail (post 9000; 1'234'567 → 1g 234s 567c).
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_POSTRECV_REQ));
        wire::Reader r(b);
        std::uint32_t post_id = 0, zero = 1;
        std::string op_name, char_name, title, msg;
        std::uint8_t post_type = 0;
        std::uint32_t gold = 0, silver = 0, copper = 0;
        std::uint8_t tail = 1;
        r.Read(post_id); r.Read(zero);
        r.ReadString(op_name); r.ReadString(char_name);
        r.ReadString(title); r.ReadString(msg);
        r.Read(post_type);
        r.Read(gold); r.Read(silver); r.Read(copper);
        r.Read(tail);
        EXPECT(post_id == 9000);
        EXPECT(zero == 0);
        EXPECT(op_name.empty());
        EXPECT(char_name == "Mia");
        EXPECT(title == "00000000" && msg == "00000000");
        EXPECT(post_type == 1);        // POST_PACKATE
        EXPECT(gold == 1 && silver == 234 && copper == 567);
        EXPECT(tail == 0);
        EXPECT(r.Eof());
    }
    { // 3. Bracket roster broadcast.
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_TOURNAMENTMATCH_REQ));
        wire::Reader r(b);
        std::uint8_t count = 0;
        r.Read(count);
        EXPECT(count == 8);
        std::uint8_t slot_country[8] = {};
        for (std::uint8_t i = 0; i < count; ++i)
        {
            std::uint8_t entry = 0, slot = 0xFF;
            std::uint32_t id = 0; std::uint8_t country = 0;
            std::string name; std::uint8_t level = 0, cls = 0;
            std::uint32_t chief = 0;
            std::uint8_t r0 = 1, r1v = 1, r2v = 1;
            r.Read(entry); r.Read(slot); r.Read(id); r.Read(country);
            r.ReadString(name); r.Read(level); r.Read(cls);
            r.Read(chief);
            r.Read(r0); r.Read(r1v); r.Read(r2v);
            EXPECT(entry == 1);
            EXPECT(slot < 8);
            EXPECT(chief == id);
            EXPECT(r0 == 0 && r1v == 0 && r2v == 0);
            if (slot < 8)
                slot_country[slot] = country;
        }
        EXPECT(r.Eof());
        for (int s = 0; s < 8; s += 2)
            EXPECT(slot_country[s] != slot_country[s + 1]);
    }
    std::this_thread::sleep_for(30ms);
    EXPECT(!tournaments.HasPlayer(61));      // Mia dropped
    EXPECT(tournaments.PlayerCount() == 8);
    {
        const auto calls = tournament_repo.PaybackCalls();
        EXPECT(calls.size() == 1);
        if (!calls.empty())
        {
            EXPECT(calls[0].char_id == 61);
            EXPECT(calls[0].gold == 1);
            EXPECT(calls[0].silver == 234);
            EXPECT(calls[0].copper == 567);
        }
    }
    EXPECT(tournament_repo.SaveStatusCalls().empty());

    // --- W6-53b: EnterGate ticket banking -----------------------------
    EXPECT(tournaments.AdvanceStep(5, 0, tnmt::kStepEnter).has_value());
    { // key mismatch → dropped (verified via the zero-sum EVENTLIST).
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 70);
        wire::WritePOD<std::uint32_t>(b, 0xDEAD);
        wire::WritePOD<std::uint32_t>(b, 5000);
        wire::WritePOD<std::uint8_t>(b, 1);
        SendFramed(p1,
            ToUint16(MessageId::MW_TOURNAMENTENTERGATE_ACK), b);
    }
    std::this_thread::sleep_for(30ms);
    { // good key → 5000/100 (TOURNAMENT_BASEPRIZE) = 50 tickets.
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 70);
        wire::WritePOD<std::uint32_t>(b, 0x0070);
        wire::WritePOD<std::uint32_t>(b, 5000);
        wire::WritePOD<std::uint8_t>(b, 1);
        SendFramed(p1,
            ToUint16(MessageId::MW_TOURNAMENTENTERGATE_ACK), b);
    }
    std::this_thread::sleep_for(30ms);

    const auto kAck = ToUint16(MessageId::MW_TOURNAMENT_ACK);
    const auto kReq = ToUint16(MessageId::MW_TOURNAMENT_REQ);

    // --- W6-53c: EVENTLIST before any bet -----------------------------
    {
        SendFramed(p1, kAck, AckHead(70, 0x0070,
            ToUint16(MessageId::MW_TOURNAMENTEVENTLIST_ACK)));
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kReq);
        wire::Reader r(b);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t base = 0; std::uint32_t sum = 0;
        std::uint8_t count = 0;
        r.Read(id); r.Read(key); r.Read(proto);
        r.Read(base); r.Read(sum); r.Read(count);
        EXPECT(proto ==
            ToUint16(MessageId::MW_TOURNAMENTEVENTLIST_REQ));
        EXPECT(base == 50);            // BASEPRIZE 100 / 2 entries
        EXPECT(sum == 50);             // the key-mismatch gate held
        EXPECT(count == 2);
        std::uint8_t eid = 0; std::string name; std::uint8_t type = 0;
        std::string batter; std::uint8_t bcountry = 0;
        float rate = 1.f; std::uint32_t amount = 1;
        r.Read(eid); r.ReadString(name); r.Read(type);
        r.ReadString(batter); r.Read(bcountry);
        r.Read(rate); r.Read(amount);
        EXPECT(eid == 1);
        EXPECT(batter.empty());
        EXPECT(bcountry == tnmt::kCountryN);
        EXPECT(rate == 0.f && amount == 0);
    }

    // --- W6-53d: EVENTJOIN + re-join reset -----------------------------
    {
        auto b = AckHead(70, 0x0070,
            ToUint16(MessageId::MW_TOURNAMENTEVENTJOIN_ACK));
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WritePOD<std::uint32_t>(b, 53);
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        EXPECT(w == kReq);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t result = 0xFF;
        r.Read(id); r.Read(key); r.Read(proto); r.Read(result);
        EXPECT(proto ==
            ToUint16(MessageId::MW_TOURNAMENTEVENTJOIN_REQ));
        EXPECT(result == tnmt::kResultSuccess);
    }
    {
        // Re-join onto 57 — resets the 53 bet first.
        auto b = AckHead(70, 0x0070,
            ToUint16(MessageId::MW_TOURNAMENTEVENTJOIN_ACK));
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WritePOD<std::uint32_t>(b, 57);
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        EXPECT(w == kReq);
    }
    { // Unknown target → silent (sentinel = next EVENTLIST frame).
        auto b = AckHead(70, 0x0070,
            ToUint16(MessageId::MW_TOURNAMENTEVENTJOIN_ACK));
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WritePOD<std::uint32_t>(b, 9999);
        SendFramed(p1, kAck, b);
        std::this_thread::sleep_for(30ms);
    }

    // --- W6-53e: EVENTLIST with the standing bet ----------------------
    {
        SendFramed(p1, kAck, AckHead(70, 0x0070,
            ToUint16(MessageId::MW_TOURNAMENTEVENTLIST_ACK)));
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == kReq);
        wire::Reader r(b);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t base = 0; std::uint32_t sum = 0;
        std::uint8_t count = 0;
        r.Read(id); r.Read(key); r.Read(proto);
        r.Read(base); r.Read(sum); r.Read(count);
        std::uint8_t eid = 0; std::string name; std::uint8_t type = 0;
        std::string batter; std::uint8_t bcountry = 0xFF;
        float rate = 0.f; std::uint32_t amount = 0;
        r.Read(eid); r.ReadString(name); r.Read(type);
        r.ReadString(batter); r.Read(bcountry);
        r.Read(rate); r.Read(amount);
        EXPECT(batter == "c2_57");     // the re-join target
        EXPECT(bcountry == 2);
        EXPECT(rate == 1.f);           // int div: 50 / 50
        EXPECT(amount == 2500);        // base 50 * ticket 50 * rate 1
    }

    // --- W6-53f: EVENTINFO roster + rates ------------------------------
    {
        auto b = AckHead(70, 0x0070,
            ToUint16(MessageId::MW_TOURNAMENTEVENTINFO_ACK));
        wire::WritePOD<std::uint8_t>(b, 1);
        SendFramed(p1, kAck, b);
        auto [w, rb] = ReadFramed(p1);
        EXPECT(w == kReq);
        wire::Reader r(rb);
        std::uint32_t id = 0, key = 0; std::uint16_t proto = 0;
        std::uint8_t entry = 0, base = 0; std::uint32_t sum = 0;
        std::uint8_t count = 0;
        r.Read(id); r.Read(key); r.Read(proto);
        r.Read(entry); r.Read(base); r.Read(sum); r.Read(count);
        EXPECT(proto ==
            ToUint16(MessageId::MW_TOURNAMENTEVENTINFO_REQ));
        EXPECT(entry == 1 && base == 50 && sum == 50);
        EXPECT(count == 8);
        bool saw_57 = false;
        for (std::uint8_t i = 0; i < count; ++i)
        {
            std::uint32_t pid = 0; std::uint8_t country = 0;
            std::string guild, name;
            std::uint8_t level = 0, cls = 0;
            std::uint32_t rank = 0, month = 0;
            float rate = -1.f; std::uint8_t party = 0xFF;
            r.Read(pid); r.Read(country); r.ReadString(guild);
            r.ReadString(name); r.Read(level); r.Read(cls);
            r.Read(rank); r.Read(month); r.Read(rate);
            r.Read(party);
            // Chief 50 still lists the dropped member Mia — the
            // legacy counterpart holds a *freed* pointer here (its
            // party iteration is UB); the port keeps the ref alive,
            // which is the only well-defined rendering.
            EXPECT(party == (pid == 50 ? 1 : 0));
            for (std::uint8_t m = 0; m < party; ++m)
            {
                std::uint32_t mid = 0; std::uint8_t mcountry = 0;
                std::string mguild, mname;
                std::uint8_t mlevel = 0, mcls = 0;
                std::uint32_t mrank = 0, mmonth = 0;
                r.Read(mid); r.Read(mcountry); r.ReadString(mguild);
                r.ReadString(mname); r.Read(mlevel); r.Read(mcls);
                r.Read(mrank); r.Read(mmonth);
                EXPECT(mid == 61 && mname == "Mia");
            }
            if (pid == 57)
            {
                saw_57 = true;
                EXPECT(rate == 1.f);   // int div: 50 / 50
            }
            else
                EXPECT(rate == 0.f);
        }
        EXPECT(saw_57);
        EXPECT(r.Eof());
    }

    // --- W6-53g: non-bracket result step is silent --------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, tnmt::kStepEnter);
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WritePOD<std::uint32_t>(b, 57);
        wire::WritePOD<std::uint32_t>(b, 50);
        wire::WritePOD<std::uint32_t>(b, 0);
        wire::WritePOD<std::uint32_t>(b, 0);
        SendFramed(p1,
            ToUint16(MessageId::MW_TOURNAMENTRESULT_ACK), b);
        std::this_thread::sleep_for(30ms);
        EXPECT(tournament_repo.ResultCalls().empty());
    }

    // --- W6-53h: FINAL result → marks + broadcast + payout ------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, tnmt::kStepFinal);
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WritePOD<std::uint32_t>(b, 57);   // win: Beth's pick
        wire::WritePOD<std::uint32_t>(b, 50);   // lose
        wire::WritePOD<std::uint32_t>(b, 111);
        wire::WritePOD<std::uint32_t>(b, 222);
        SendFramed(p1,
            ToUint16(MessageId::MW_TOURNAMENTRESULT_ACK), b);
    }
    { // Result fan-out.
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_TOURNAMENTRESULT_REQ));
        wire::Reader r(b);
        std::uint16_t tid = 0; std::uint8_t step = 0, ret = 0;
        std::uint32_t win = 0, lose = 0, blue = 0, red = 0;
        std::uint8_t count = 0xFF;
        r.Read(tid); r.Read(step); r.Read(ret);
        r.Read(win); r.Read(lose); r.Read(blue); r.Read(red);
        r.Read(count);
        EXPECT(tid == 5);
        EXPECT(step == tnmt::kStepFinal && ret == 1);
        EXPECT(win == 57 && lose == 50);
        EXPECT(blue == 111 && red == 222);
        EXPECT(count == 1);            // loser 50's member Mia
        std::uint32_t member = 0;
        r.Read(member);
        EXPECT(member == 61);
        EXPECT(r.Eof());
    }
    { // Beth's BATPOINT payout on her main map.
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_TOURNAMENTBATPOINT_REQ));
        wire::Reader r(b);
        std::uint32_t id = 0; std::string name;
        std::uint32_t amount = 0;
        r.Read(id); r.ReadString(name); r.Read(amount);
        EXPECT(id == 70 && name == "Beth");
        EXPECT(amount == 2500);        // base 50 * ticket 50 * rate 1
        EXPECT(r.Eof());
    }
    std::this_thread::sleep_for(30ms);
    {
        const auto calls = tournament_repo.ResultCalls();
        EXPECT(calls.size() == 1);
        if (!calls.empty())
        {
            EXPECT(calls[0].step == tnmt::kStepFinal);
            EXPECT(calls[0].ret == 1);
            EXPECT(calls[0].win_id == 57 && calls[0].lose_id == 50);
        }
    }
    { // The marks landed: MATCHLIST shows 57's FINAL win.
        const auto match = tournaments.MatchReply();
        EXPECT(match.ok);
        for (const auto& row : match.rows)
        {
            if (row.row.char_id == 57)
                EXPECT(row.result[tnmt::kMatchFinal] ==
                       tnmt::kWinWin);
            if (row.row.char_id == 50)
                EXPECT(row.result[tnmt::kMatchFinal] ==
                       tnmt::kWinLose);
        }
    }

    // --- W6-53i: boot step>=PARTY resume re-runs the select -----------
    {
        tworldsvr::TournamentRegistry       reg2;
        tworldsvr::FakeTournamentRepository repo2;
        tworldsvr::TnmtEventTimeRow t{};
        t.tour_id = 7;
        t.time.week = 1; t.time.day = 1; t.time.start_sec = 7200;
        repo2.SeedEventTime(t);
        repo2.SeedEventSchedule(7,
            {tworldsvr::TnmtEventStepRow{tnmt::kStepEnter, 600},
             tworldsvr::TnmtEventStepRow{tnmt::kStepEnd, 30}});
        tworldsvr::TournamentEntrySeed e{};
        e.entry_id = 2; e.name = "Cup"; e.type = tnmt::kEntryParty;
        repo2.SeedEventEntries(7, {e});
        repo2.SeedCurrentStep(tworldsvr::TnmtCurrentStep{
            7, 0, tnmt::kStepEnter});
        tworldsvr::TnmtPlayerRow hana{};
        hana.char_id = 60; hana.name = "Hana"; hana.chief_id = 60;
        hana.country = 1; hana.level = 50; hana.entry_id = 2;
        repo2.SeedPlayerRow(hana);
        tworldsvr::TnmtPlayerRow ivan{};
        ivan.char_id = 61; ivan.name = "Ivan"; ivan.chief_id = 60;
        ivan.country = 1; ivan.level = 48; ivan.entry_id = 2;
        repo2.SeedPlayerRow(ivan);

        tworldsvr::LoadTournamentState(reg2, repo2, base_now);

        // ENTER (5) >= PARTY (3): the select re-ran — Hana got a
        // bracket slot (chief in the player pool), Ivan re-attached.
        EXPECT(reg2.CurrentStep() == tnmt::kStepEnter);
        EXPECT(reg2.CurrentSelected());
        EXPECT(reg2.PlayerCount() == 2);
        const auto party = reg2.PartyListReply(60);
        EXPECT(party.ok && party.members.size() == 1);
        EXPECT(reg2.AdvanceStep(7, 0, tnmt::kStepMatch).has_value());
        const auto match = reg2.MatchReply();
        EXPECT(match.ok);
        EXPECT(match.rows.size() == 2);   // Hana + Ivan (party ref)
        bool hana_in_bracket = false;
        for (const auto& row : match.rows)
            if (row.row.char_id == 60)
            {
                hana_in_bracket = true;
                EXPECT(row.slot_id == 0);
            }
        EXPECT(hana_in_bracket);
        EXPECT(repo2.PaybackCalls().empty());
    }

    // --- W6-53j: the gate ticket survives a tournament rebuild -------
    // (legacy m_dwTicket lives on the TCHARACTER; TournamentClear
    // only wipes the batting maps).
    {
        TournamentRegistry reg3;
        tworldsvr::TournamentEntrySeed e{};
        e.entry_id = 1; e.name = "Solo"; e.type = tnmt::kEntryPrivate;
        reg3.SeedEntries(9, {e});
        std::vector<TournamentStep> steps;
        steps.push_back(TournamentStep{0, tnmt::kStepEnter, 600,
            base_now + 50, 0});
        reg3.RebuildCurrent(9, steps);
        EXPECT(reg3.AdvanceStep(9, 0, tnmt::kStepEnter).has_value());
        reg3.EnterGate(80, 700, true);          // 7 tickets banked

        reg3.RebuildCurrent(9, steps);          // re-election clear
        EXPECT(reg3.AdvanceStep(9, 0, tnmt::kStepEnter).has_value());
        TournamentRegistry::TnmtPlayerSeed seed{};
        seed.brief = tworldsvr::TnmtPlayerBrief{81, 1, "Pia", 40, 1};
        EXPECT(reg3.AddPlayerAtStep(1, seed, tnmt::kStepMatch, 81));

        // The surviving ticket passes the EVENTINFO gate and stakes
        // a real bet.
        EXPECT(reg3.EventJoin(80, 1, 81));
        const auto info = reg3.EventInfoReply(80, 1);
        EXPECT(info.ok);
        EXPECT(info.players.size() == 1);
        if (!info.players.empty())
            EXPECT(info.players[0].rate == 0.f);  // sum 0 / bet 7
    }

    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("test_tournament_match_handlers: all passed\n");
    else
        std::fprintf(stderr,
            "test_tournament_match_handlers: %d failure(s)\n",
            g_fails);
    return g_fails == 0 ? 0 : 1;
}
