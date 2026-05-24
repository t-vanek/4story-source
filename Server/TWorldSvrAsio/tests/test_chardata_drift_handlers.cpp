// W6-23 wire test: CHARDATA_ACK non-ready ENTERCHAR_REQ fan-out (the
// "drift" path). Closes W6-20's deferred branch — when CHARDATA_ACK
// arrives but some cons haven't ENTERCHAR_ACKed yet, world fans the
// fat ENTERCHAR_REQ composite to each non-ready con; each map loads
// the char and replies ENTERCHAR_ACK (W6-20) to flip its con ready.
//
// Two map peers (0x42 main + 0x43). Char Bob in guild + party, with
// 0x42 manually marked ready and 0x43 still not-ready. CHARDATA_ACK
// from the main → ENTERCHAR_REQ on the non-ready con's peer (0x43)
// carrying the guild + party composite + the opaque recall-mon tail
// preserved verbatim. Then ENTERCHAR_ACK from 0x43 closes the loop
// and CheckMainCon broadcasts MW_CHECKMAIN_REQ to both cons.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/guild_registry.h"
#include "../services/party_registry.h"
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

// MW_CHARDATA_ACK = 10 named fields + opaque tail. The tail mimics
// the minimal legacy shape: BYTE recall_count = 0 + STRING comment = "".
std::vector<std::byte> CharDataBody(std::uint32_t char_id, std::uint32_t key,
                                    std::uint8_t start_act, std::uint8_t level,
                                    std::uint32_t max_hp, std::uint32_t hp,
                                    std::uint32_t max_mp, std::uint32_t mp,
                                    std::uint8_t country, std::uint8_t mode,
                                    const std::vector<std::byte>& tail)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, start_act);
    tworldsvr::wire::WritePOD(b, level);
    tworldsvr::wire::WritePOD(b, max_hp);
    tworldsvr::wire::WritePOD(b, hp);
    tworldsvr::wire::WritePOD(b, max_mp);
    tworldsvr::wire::WritePOD(b, mp);
    tworldsvr::wire::WritePOD(b, country);
    tworldsvr::wire::WritePOD(b, mode);
    b.insert(b.end(), tail.begin(), tail.end());
    return b;
}

std::vector<std::byte> EnterCharAckBody(std::uint32_t char_id, std::uint32_t key)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
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
    tworldsvr::PartyRegistry parties;
    tworldsvr::PeerRegistry  peers;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.parties = &parties; ctx.peers = &peers; ctx.nation = 0;

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

    // p1=0x42 (Bob's main), p2=0x43 (his other con). Drain the
    // RELAYCONNECT fan-out the second registration causes on p1.
    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0043));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    // Seed Bob's guild + party.
    {
        auto gd = std::make_shared<tworldsvr::TGuild>();
        gd->id = 10; gd->name = "Eagles"; gd->country = 1;
        gd->fame = 500; gd->fame_color = 0x80FF;
        tworldsvr::TGuildMember m{};
        m.char_id = 200; m.duty = 1; m.peer = 2;
        m.castle = 50; m.camp = 1;
        gd->members.push_back(m);
        EXPECT(guilds.Insert(gd));
    }
    {
        auto pt = std::make_shared<tworldsvr::TParty>();
        pt->id = 7; pt->obtain_type = 1; pt->chief_char_id = 200;
        pt->AddMember(200);
        EXPECT(parties.Insert(pt));
    }

    // Bob's cons: main 0x42 manually marked ready, 0x43 not-ready.
    auto cons_size = [&](std::uint32_t id) -> std::size_t {
        auto c = chars.Find(id);
        if (!c) return 0;
        std::lock_guard g(c->lock);
        return c->cons.size();
    };
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK),
               AddCharBody(200, 0xB0));
    for (int i = 0; i < 200 && !chars.Find(200); ++i)
        std::this_thread::sleep_for(10ms);
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK),
               AddCharBody(200, 0xB0));
    for (int i = 0; i < 200 && cons_size(200) != 2; ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(cons_size(200) == 2);

    {
        auto b = chars.Find(200);
        std::lock_guard g(b->lock);
        b->name      = "Bob";
        b->guild_id  = 10;
        b->party_id  = 7;
        b->channel   = 4;
        b->map_id    = 777;
        b->pos_x = 11.0f; b->pos_y = 22.0f; b->pos_z = 33.0f;
        b->klass         = 3;
        b->aid_country   = 2;
        b->helmet_hide   = 1;
        b->riding        = 999;
        b->soulmate.target = 42;
        b->soulmate.name   = "Alice";
        // Mark the main con ready (legacy ENTERCHAR_ACK would have
        // arrived for the main during login); leave 0x43 not-ready.
        for (auto& c : b->cons)
            if (c.server_id == 0x42) { c.ready = true; break; }
    }

    // Opaque CHARDATA_ACK tail: empty recall list + empty comment.
    std::vector<std::byte> tail;
    wire::WritePOD<std::uint8_t>(tail, 0);            // recall_count
    wire::WriteString          (tail, "");            // comment

    // --- Test A: drift fan-out — ENTERCHAR_REQ on the unready con --
    {
        SendFramed(p1, ToUint16(MessageId::MW_CHARDATA_ACK),
                   CharDataBody(200, 0xB0,
                       /*start_act=*/2, /*level=*/40,
                       /*max_hp=*/1000, /*hp=*/950,
                       /*max_mp=*/500,  /*mp=*/480,
                       /*country=*/1, /*mode=*/5, tail));
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_ENTERCHAR_REQ));
        wire::Reader r(b);
        std::uint32_t cid = 0, key = 0;
        std::uint8_t  start_act = 0;
        std::string   name;
        std::uint16_t map_id = 0;
        float         px = 0, py = 0, pz = 0;
        std::uint32_t guild_id = 0, fame = 0, fame_color = 0;
        std::string   guild_name, tactics_name, soulmate_name;
        std::uint8_t  duty = 0, peer_b = 0, camp = 0;
        std::uint16_t castle = 0;
        std::uint32_t tactics_id = 0, party_chief = 0;
        std::uint16_t party_id_in = 0, commander = 0;
        std::uint8_t  party_type = 0, level = 0, helmet_hide = 0;
        std::uint8_t  country = 0, aid_country = 0, mode = 0, klass = 0;
        std::uint32_t riding = 0, soulmate = 0, soul_silence = 0;
        std::int64_t  chat_ban_time = 0;
        r.Read(cid); r.Read(key); r.Read(start_act);
        r.ReadString(name); r.Read(map_id);
        r.Read(px); r.Read(py); r.Read(pz);
        r.Read(guild_id); r.Read(fame); r.Read(fame_color);
        r.ReadString(guild_name);
        r.Read(duty); r.Read(peer_b); r.Read(castle); r.Read(camp);
        r.Read(tactics_id); r.ReadString(tactics_name);
        r.Read(party_id_in); r.Read(party_type); r.Read(party_chief);
        r.Read(commander);
        r.Read(level); r.Read(helmet_hide);
        r.Read(country); r.Read(aid_country); r.Read(mode);
        r.Read(riding); r.Read(chat_ban_time);
        r.Read(soulmate); r.Read(soul_silence);
        r.ReadString(soulmate_name);
        r.Read(klass);
        EXPECT(cid == 200);
        EXPECT(key == 0xB0);
        EXPECT(start_act == 2);
        EXPECT(name == "Bob");
        EXPECT(map_id == 777);
        EXPECT(px == 11.0f); EXPECT(pz == 33.0f);
        EXPECT(guild_id == 10);
        EXPECT(fame == 500);
        EXPECT(fame_color == 0x80FF);
        EXPECT(guild_name == "Eagles");
        EXPECT(duty == 1);
        EXPECT(peer_b == 2);
        EXPECT(castle == 50);
        EXPECT(camp == 1);
        EXPECT(party_id_in == 7);
        EXPECT(party_type == 1);
        EXPECT(party_chief == 200);
        EXPECT(commander == 0);            // not in a corps
        EXPECT(level == 40);
        EXPECT(helmet_hide == 1);
        EXPECT(country == 1);              // from inbound packet
        EXPECT(aid_country == 2);
        EXPECT(mode == 5);
        EXPECT(riding == 999);
        EXPECT(soulmate == 42);
        EXPECT(soulmate_name == "Alice");
        EXPECT(klass == 3);

        // Opaque tail (recall=0 + STRING="") preserved verbatim.
        std::uint8_t  rc = 0xFF;
        std::int32_t  comment_len = -1;
        r.Read(rc); r.Read(comment_len);
        EXPECT(rc == 0);
        EXPECT(comment_len == 0);

        // Char's level + HP/MP refreshed by the handler.
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        {
            auto bobch = chars.Find(200);
            std::lock_guard g(bobch->lock);
            if (bobch->level == 40 && bobch->hp == 950 && bobch->max_hp == 1000)
            { ok = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(ok);
    }

    // --- Test B: ENTERCHAR_ACK from p2 closes the loop -> CHECKMAIN
    {
        SendFramed(p2, ToUint16(MessageId::MW_ENTERCHAR_ACK),
                   EnterCharAckBody(200, 0xB0));
        auto [w1, _] = ReadFramed(p1);
        EXPECT(w1 == ToUint16(MessageId::MW_CHECKMAIN_REQ));
        auto [w2, _2] = ReadFramed(p2);
        EXPECT(w2 == ToUint16(MessageId::MW_CHECKMAIN_REQ));
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_chardata_drift_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_chardata_drift_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
