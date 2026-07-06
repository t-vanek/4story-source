// W6-48 tests: lucky-event scheduler + expiry queue.
//
//   Scheduler (direct, controlled clock): notice fires once 5 min
//     before the slot (pre-wrapped NetString), the present fires at
//     the slot and the entry reschedules a week ahead; announce-only
//     entries notice at the slot itself.
//   Expiry queue (wire): SM_EVENTEXPIRED_REQ insert/remove (+ the
//     legacy remove-miss-inserts quirk), SM_EVENTEXPIRED_ACK
//     immediate dispatch (EXPIRED_GT ends the tactics contract),
//     RunExpiredSweep pops due entries through the same dispatch.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/event_quarter_scheduler.h"
#include "../services/expired_buffer.h"
#include "../services/guild_registry.h"
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

std::vector<std::byte>
ExpiredReqBody(std::uint8_t insert, std::uint8_t type,
               std::int64_t when, std::uint32_t v1, std::uint32_t v2)
{
    namespace wire = tworldsvr::wire;
    std::vector<std::byte> b;
    wire::WritePOD<std::uint8_t>(b, insert);
    wire::WritePOD<std::uint8_t>(b, type);
    wire::WritePOD<std::int64_t>(b, when);
    wire::WritePOD<std::uint32_t>(b, v1);
    wire::WritePOD<std::uint32_t>(b, v2);
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

    // --- Scheduler unit pass (controlled clock) ---------------------
    {
        const std::int64_t now =
            static_cast<std::int64_t>(std::time(nullptr));
        // Build an entry whose weekly slot is `now + 10 min`.
        std::time_t slot_t = static_cast<std::time_t>(now + 600);
        std::tm     slot{};
#ifdef _WIN32
        localtime_s(&slot, &slot_t);
#else
        localtime_r(&slot_t, &slot);
#endif
        tworldsvr::EventQuarterEntry e{};
        e.id = 1;
        e.day = static_cast<std::uint8_t>(slot.tm_wday + 1);
        e.hour = static_cast<std::uint8_t>(slot.tm_hour);
        e.minute = static_cast<std::uint8_t>(slot.tm_min);
        e.present = "P1";
        e.announce = "come to the plaza";

        tworldsvr::EventQuarterScheduler sch;
        sch.LoadFrom({e}, now);
        const auto loaded = sch.Get(1);
        EXPECT(loaded.has_value());
        const std::int64_t slot_time = loaded->next_time;
        EXPECT(slot_time >= now && slot_time <= now + 660);

        // Well before the notice window: nothing.
        auto a0 = sch.Tick(slot_time - 400);
        EXPECT(!a0.announce && !a0.present);
        // Inside the 5-minute window: announce fires once,
        // pre-wrapped ("0000" + 4-hex-length + body).
        auto a1 = sch.Tick(slot_time - 200);
        EXPECT(a1.announce.has_value());
        if (a1.announce)
        {
            EXPECT(a1.announce->rfind("0000", 0) == 0);
            EXPECT(a1.announce->find("come to the plaza") != std::string::npos);
        }
        auto a2 = sch.Tick(slot_time - 100);
        EXPECT(!a2.announce);            // only once
        // Just past the slot: present fires + weekly reschedule.
        // (At exact equality the legacy math re-slots the same time
        // — GetNextEventTime only bumps a week when the slot is
        // strictly in the past; quirk kept.)
        auto a3 = sch.Tick(slot_time + 1);
        EXPECT(a3.present.has_value());
        if (a3.present)
            EXPECT(a3.present->present == "P1");
        const auto after = sch.Get(1);
        EXPECT(after.has_value());
        EXPECT(after->next_time >= slot_time + 6 * 24 * 3600);

        // Announce-only entry: notice at the slot itself.
        tworldsvr::EventQuarterEntry ann{};
        ann.id = 2;
        ann.day = e.day; ann.hour = e.hour; ann.minute = e.minute;
        ann.announce = "just words";
        tworldsvr::EventQuarterScheduler sch2;
        sch2.LoadFrom({ann}, now);
        const auto t2 = sch2.Get(2)->next_time;
        auto b0 = sch2.Tick(t2 - 200);
        EXPECT(!b0.announce);            // no early notice
        auto b1 = sch2.Tick(t2);
        EXPECT(b1.announce.has_value());
        EXPECT(!b1.present);
    }

    // --- Wire pass: expiry queue ------------------------------------
    boost::asio::io_context io;
    tworldsvr::CharRegistry   chars;
    tworldsvr::GuildRegistry  guilds;
    tworldsvr::PeerRegistry   peers;
    tworldsvr::CtrlSvrSlot    ctrl_svr;
    tworldsvr::ExpiredBuffer  expired;

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.ctrl_svr = &ctrl_svr;
    ctx.expired = &expired;
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

    // Guild 900 with tactics member 555; char 555 back-pointer set.
    {
        auto g = std::make_shared<tworldsvr::TGuild>();
        g->id = 900; g->name = "Tacts";
        tworldsvr::TTacticsMember tm{};
        tm.id = 555; tm.name = "Merc";
        g->tactics_members.push_back(tm);
        guilds.Insert(g);
    }
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 555);
        wire::WritePOD<std::uint32_t>(b, 0xC5);
        wire::WritePOD<std::uint32_t>(b, 0x7f000001);
        wire::WritePOD<std::uint16_t>(b, 33500);
        wire::WritePOD<std::uint32_t>(b, 700);
        SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), b);
    }
    for (int i = 0; i < 1000 && !chars.Find(555); ++i)
        std::this_thread::sleep_for(10ms);
    if (auto c = chars.Find(555))
    { std::lock_guard g(c->lock); c->tactics_guild_id = 900; }

    const auto now = static_cast<std::int64_t>(std::time(nullptr));

    // --- W6-48a: REQ maintenance -----------------------------------
    SendFramed(p1, ToUint16(MessageId::SM_EVENTEXPIRED_REQ),
        ExpiredReqBody(1, tworldsvr::kExpiredTactics, now - 5, 900, 555));
    for (int i = 0; i < 1000 && expired.Size() != 1; ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(expired.Size() == 1);
    // Exact remove drops it…
    SendFramed(p1, ToUint16(MessageId::SM_EVENTEXPIRED_REQ),
        ExpiredReqBody(0, tworldsvr::kExpiredTactics, now - 5, 900, 555));
    for (int i = 0; i < 1000 && expired.Size() != 0; ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(expired.Size() == 0);
    // …and a remove-miss INSERTS (legacy quirk, kept + documented).
    SendFramed(p1, ToUint16(MessageId::SM_EVENTEXPIRED_REQ),
        ExpiredReqBody(0, tworldsvr::kExpiredGuildWanted, now + 999,
                       111, 0));
    for (int i = 0; i < 1000 && expired.Size() != 1; ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(expired.Size() == 1);

    // --- W6-48b: due-pop sweep dispatches the tactics end ----------
    SendFramed(p1, ToUint16(MessageId::SM_EVENTEXPIRED_REQ),
        ExpiredReqBody(1, tworldsvr::kExpiredTactics, now - 1, 900, 555));
    for (int i = 0; i < 1000 && expired.Size() != 2; ++i)
        std::this_thread::sleep_for(10ms);
    boost::asio::co_spawn(io, tworldsvr::handlers::RunExpiredSweep(ctx),
        boost::asio::detached);
    for (int i = 0; i < 1000 && expired.Size() != 1; ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(expired.Size() == 1);         // only the future entry left
    {
        auto g = guilds.Find(900);
        std::lock_guard lk(g->lock);
        EXPECT(g->tactics_members.empty());
    }
    if (auto c = chars.Find(555))
    {
        std::lock_guard g(c->lock);
        EXPECT(c->tactics_guild_id == 0);
    }

    // --- W6-48c: ACK dispatch-now (re-seed + direct) ----------------
    {
        auto g = guilds.Find(900);
        std::lock_guard lk(g->lock);
        tworldsvr::TTacticsMember tm{};
        tm.id = 555; tm.name = "Merc";
        g->tactics_members.push_back(tm);
    }
    if (auto c = chars.Find(555))
    { std::lock_guard g(c->lock); c->tactics_guild_id = 900; }
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, tworldsvr::kExpiredTactics);
        wire::WritePOD<std::int64_t>(b, now);
        wire::WritePOD<std::uint32_t>(b, 900);
        wire::WritePOD<std::uint32_t>(b, 555);
        SendFramed(p1, ToUint16(MessageId::SM_EVENTEXPIRED_ACK), b);
    }
    {
        bool gone = false;
        for (int i = 0; i < 1000 && !gone; ++i)
        {
            {
                auto g = guilds.Find(900);
                std::lock_guard lk(g->lock);
                gone = g->tactics_members.empty();
            }
            if (!gone)                 // sleep OUTSIDE the guild lock
                std::this_thread::sleep_for(10ms);
        }
        EXPECT(gone);
    }

    p1.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_evqt_expired_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_evqt_expired_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
