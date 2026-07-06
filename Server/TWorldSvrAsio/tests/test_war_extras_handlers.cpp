// W6-43 wire tests: war/castle extras.
//
//   MW_ENDWAR_ACK / MW_SKYGARDENOCCUPY_ACK — field-checked broadcasts.
//   MW_WARCOUNTRYBALANCE_ACK — reply from the WarCountryIndex
//     (classification incl. aid-country + gap bucketing); key
//     mismatch / out-of-range level are silent.
//   CT_CASTLEGUILDCHG_REQ — unknown guild → full-layout fail ACK to
//     the sender; known guilds → broadcast + success ACK.
//   Castle-apply persist — the W5-2 apply path now records
//     TSaveCastleApplicant(castle, target, camp) via the fake repo.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/fake_war_ops_repository.h"
#include "../services/guild_registry.h"
#include "../services/peer_registry.h"
#include "../services/war_country_index.h"
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

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    namespace wire = tworldsvr::wire;

    boost::asio::io_context io;
    tworldsvr::CharRegistry         chars;
    tworldsvr::GuildRegistry        guilds;
    tworldsvr::PeerRegistry         peers;
    tworldsvr::CtrlSvrSlot          ctrl_svr;
    tworldsvr::WarCountryIndex      war_index;
    tworldsvr::FakeWarOpsRepository war_ops;

    // Index: D-country lvl 135 ×2 (gap 0), C-country lvl 148 (gap 1)
    // via a direct aid-country row; a neutral (no-aid country 2) and
    // an under-level row must be skipped.
    war_index.Rebuild({
        {1001, 0, false, 0, 135},
        {1002, 0, false, 0, 139},
        {1003, 2, true,  1, 148},   // aid → C
        {1004, 2, false, 0, 150},   // no aid → neutral, skipped
        {1005, 1, false, 0, 120},   // under base level, skipped
    });
    EXPECT(war_index.Count(0, 0) == 2);
    EXPECT(war_index.Count(1, 1) == 1);
    EXPECT(war_index.TotalSize() == 3);

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.ctrl_svr = &ctrl_svr;
    ctx.war_index = &war_index;
    ctx.war_ops = &war_ops;
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
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0443));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    // --- W6-43a: ENDWAR broadcast ----------------------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint16_t>(b, 12);
        SendFramed(p1, ToUint16(MessageId::MW_ENDWAR_ACK), b);
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_ENDWAR_REQ));
        wire::Reader r(b);
        std::uint16_t castle = 0;
        r.Read(castle);
        EXPECT(castle == 12);
    }

    // --- W6-43b: SKYGARDEN broadcast (field order type,id,country) --
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 2);
        wire::WritePOD<std::uint16_t>(b, 7);
        wire::WritePOD<std::uint8_t>(b, 1);
        SendFramed(p2, ToUint16(MessageId::MW_SKYGARDENOCCUPY_ACK), b);
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_SKYGARDENOCCUPY_REQ));
        wire::Reader r(b);
        std::uint8_t type = 0, country = 0;
        std::uint16_t id = 0;
        r.Read(type); r.Read(id); r.Read(country);
        EXPECT(type == 2);
        EXPECT(id == 7);
        EXPECT(country == 1);
    }

    // --- W6-43c: WARCOUNTRYBALANCE reply ----------------------------
    // Seed char 100 (key 0xA1) on p1, level 149 → gap 1.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 100);
        wire::WritePOD<std::uint32_t>(b, 0xA1);
        wire::WritePOD<std::uint32_t>(b, 0x7f000001);
        wire::WritePOD<std::uint16_t>(b, 33500);
        wire::WritePOD<std::uint32_t>(b, 500);
        SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), b);
    }
    for (int i = 0; i < 1000 && !chars.Find(100); ++i)
        std::this_thread::sleep_for(10ms);
    if (auto c = chars.Find(100))
    { std::lock_guard g(c->lock); c->level = 149; }

    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 100);
        wire::WritePOD<std::uint32_t>(b, 0xA1);
        SendFramed(p1, ToUint16(MessageId::MW_WARCOUNTRYBALANCE_ACK), b);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_WARCOUNTRYBALANCE_REQ));
        wire::Reader r(b);
        std::uint32_t cid = 0, key = 0, cd = 0, cc = 0;
        std::uint8_t gap = 0xFF;
        r.Read(cid); r.Read(key); r.Read(cd); r.Read(cc); r.Read(gap);
        EXPECT(cid == 100);
        EXPECT(key == 0xA1);
        EXPECT(cd == 0);      // D gap-1 bucket empty
        EXPECT(cc == 1);      // C gap-1 has 1003
        EXPECT(gap == 1);
    }
    {   // wrong key → silent (sentinel: ENDWAR next)
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 100);
        wire::WritePOD<std::uint32_t>(b, 0xBAD);
        SendFramed(p1, ToUint16(MessageId::MW_WARCOUNTRYBALANCE_ACK), b);
    }
    std::this_thread::sleep_for(30ms);

    // --- W6-43d: CASTLEGUILDCHG fail + success ----------------------
    auto chg_body = [&](std::uint32_t def, std::uint32_t atk)
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint16_t>(b, 3);        // castle
        wire::WritePOD<std::uint32_t>(b, def);
        wire::WritePOD<std::uint32_t>(b, atk);
        wire::WritePOD<std::uint32_t>(b, 77);       // manager
        wire::WritePOD<std::int64_t>(b, 1751800000);
        return b;
    };
    // Unknown guilds → fail ACK straight to the sender (p2).
    SendFramed(p2, ToUint16(MessageId::CT_CASTLEGUILDCHG_REQ),
        chg_body(900, 901));
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::CT_CASTLEGUILDCHG_ACK));
        wire::Reader r(b);
        std::uint32_t manager = 0; std::uint8_t ret = 0xFF;
        r.Read(manager); r.Read(ret);
        EXPECT(manager == 77);
        EXPECT(ret == 0);
    }
    // Seed both guilds → success.
    {
        auto g1 = std::make_shared<tworldsvr::TGuild>();
        g1->id = 900; g1->name = "Defenders";
        guilds.Insert(g1);
        auto g2 = std::make_shared<tworldsvr::TGuild>();
        g2->id = 901; g2->name = "Attackers";
        guilds.Insert(g2);
    }
    SendFramed(p2, ToUint16(MessageId::CT_CASTLEGUILDCHG_REQ),
        chg_body(900, 901));
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CASTLEGUILDCHG_REQ));
        wire::Reader r(b);
        std::uint16_t castle = 0;
        std::uint32_t def_id = 0, atk_id = 0;
        std::string def, atk;
        std::int64_t when = 0;
        r.Read(castle); r.Read(def_id); r.ReadString(def);
        r.Read(atk_id); r.ReadString(atk); r.Read(when);
        EXPECT(castle == 3);
        EXPECT(def_id == 900 && def == "Defenders");
        EXPECT(atk_id == 901 && atk == "Attackers");
        EXPECT(when == 1751800000);
    }
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::CT_CASTLEGUILDCHG_ACK));
        wire::Reader r(b);
        std::uint32_t manager = 0; std::uint8_t ret = 0;
        std::uint16_t castle = 0; std::uint32_t def_id = 0;
        std::string def;
        r.Read(manager); r.Read(ret); r.Read(castle);
        r.Read(def_id); r.ReadString(def);
        EXPECT(manager == 77);
        EXPECT(ret == 1);
        EXPECT(castle == 3);
        EXPECT(def_id == 900 && def == "Defenders");
    }

    // --- W6-43e: castle-apply persist -------------------------------
    // Char 100 is chief + member of guild 900; applies to castle 5.
    {
        auto g = guilds.Find(900);
        std::lock_guard lk(g->lock);
        g->chief_char_id = 100;
        tworldsvr::TGuildMember m{};
        m.char_id = 100; m.guild_id = 900;
        g->members.push_back(m);
    }
    if (auto c = chars.Find(100))
    { std::lock_guard g(c->lock); c->guild_id = 900; }
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 100);      // char
        wire::WritePOD<std::uint32_t>(b, 0xA1);     // key
        wire::WritePOD<std::uint16_t>(b, 5);        // castle
        wire::WritePOD<std::uint32_t>(b, 100);      // target
        wire::WritePOD<std::uint8_t>(b, 2);         // camp
        SendFramed(p1, ToUint16(MessageId::MW_CASTLEAPPLY_ACK), b);
    }
    {   // success reply to the requester
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CASTLEAPPLY_REQ));
    }
    // applicant-count broadcast for castle 5 on both maps
    for (auto* s : {&p1, &p2})
    {
        auto [w, _] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CASTLEAPPLICANTCOUNT_REQ));
    }
    for (int i = 0; i < 1000 && war_ops.SaveCalls().empty(); ++i)
        std::this_thread::sleep_for(10ms);
    {
        const auto calls = war_ops.SaveCalls();
        EXPECT(calls.size() == 1);
        if (!calls.empty())
        {
            EXPECT(std::get<0>(calls[0]) == 5);
            EXPECT(std::get<1>(calls[0]) == 100);
            EXPECT(std::get<2>(calls[0]) == 2);
        }
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_war_extras_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_war_extras_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
