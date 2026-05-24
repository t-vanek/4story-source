// W4-22 wire test: fresh-login ENTERSVR completion.
//
// Two chars on a single map peer, exercising the full CHARINFO_REQ +
// ROUTE_REQ + FRIENDLIST_REQ + (optional) CHATBAN_REQ chain that the
// extended OnEnterSvrAck emits after the W4-20 identity load:
//
//   * Alice (guildless, no party, chat-banned) → CHARINFO with zeros
//     for guild/tactics/party, ROUTE_REQ, empty FRIENDLIST_REQ,
//     CHATBAN_REQ enforcing the active ban.
//   * Bob (in a guild + party, no ban) → CHARINFO populated with the
//     guild meta + member's duty/peer/castle/camp + party id +
//     obtain_type + chief_id + title_id + rank_point; ROUTE_REQ;
//     empty FRIENDLIST_REQ. No CHATBAN_REQ — verified by following
//     the FRIENDLIST_REQ with a re-emitted ENTERSVR and confirming
//     the next packet is *its* CHARINFO_REQ (so the CHATBAN slot
//     was indeed skipped).

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
#include <ctime>
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

// The 25-field MW_ENTERSVR_ACK packet (SSHandler.cpp:1218).
std::vector<std::byte> EnterSvrBody(std::uint32_t char_id, std::uint32_t key,
                                    const std::string& name,
                                    std::uint8_t level, std::uint32_t region,
                                    std::uint8_t channel, std::uint16_t map_id,
                                    float pos_x, float pos_y, float pos_z,
                                    std::uint16_t title_id,
                                    std::uint32_t rank_point)
{
    using namespace tworldsvr::wire;
    std::vector<std::byte> b;
    WritePOD<std::uint32_t>(b, char_id);
    WritePOD<std::uint32_t>(b, key);
    WriteString(b, name);
    WritePOD<std::uint8_t>(b, level);
    WritePOD<std::uint8_t>(b, 0);          // real_sex
    WritePOD<std::uint8_t>(b, 1);          // class
    WritePOD<std::uint8_t>(b, 0);          // race
    WritePOD<std::uint8_t>(b, 0);          // sex
    WritePOD<std::uint8_t>(b, 0);          // face
    WritePOD<std::uint8_t>(b, 0);          // hair
    WritePOD<std::uint8_t>(b, 0);          // helmet_hide
    WritePOD<std::uint8_t>(b, 0);          // country
    WritePOD<std::uint8_t>(b, 0);          // aid_country
    WritePOD<std::uint32_t>(b, region);
    WritePOD<std::uint8_t>(b, channel);
    WritePOD<std::uint16_t>(b, map_id);
    WritePOD<float>(b, pos_x);
    WritePOD<float>(b, pos_y);
    WritePOD<float>(b, pos_z);
    WritePOD<std::uint8_t>(b, 0);          // logout
    WritePOD<std::uint8_t>(b, 0);          // save
    WritePOD<std::uint8_t>(b, 0);          // result (0 = ok)
    WritePOD<std::uint16_t>(b, title_id);
    WritePOD<std::uint32_t>(b, rank_point);
    WritePOD<std::uint32_t>(b, 0x7f000001);// user_ip
    return b;
}

struct CharInfo
{
    std::uint32_t cid, key;
    std::uint32_t guild_id;
    std::uint8_t  guild_country;
    std::string   guild_name;
    std::uint32_t fame, fame_color;
    std::uint32_t tactics_id;
    std::string   tactics_name;
    std::uint8_t  duty, peer;
    std::uint16_t castle;
    std::uint8_t  camp;
    std::uint16_t party_id;
    std::uint8_t  obtain_type;
    std::uint32_t party_chief;
    std::uint16_t title_id;
    std::uint32_t rank_point;
    std::uint8_t  bow_release;
};

CharInfo ParseCharInfo(const std::vector<std::byte>& body)
{
    tworldsvr::wire::Reader r(body);
    CharInfo c{};
    r.Read(c.cid); r.Read(c.key);
    r.Read(c.guild_id); r.Read(c.guild_country);
    r.ReadString(c.guild_name);
    r.Read(c.fame); r.Read(c.fame_color);
    r.Read(c.tactics_id); r.ReadString(c.tactics_name);
    r.Read(c.duty); r.Read(c.peer);
    r.Read(c.castle); r.Read(c.camp);
    r.Read(c.party_id); r.Read(c.obtain_type); r.Read(c.party_chief);
    r.Read(c.title_id); r.Read(c.rank_point);
    r.Read(c.bow_release);
    return c;
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
    tcp::socket p1(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep);
    std::this_thread::sleep_for(20ms);

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }

    // Seed the guild + party Bob will join (Bob is char 200).
    {
        auto gd = std::make_shared<tworldsvr::TGuild>();
        gd->id            = 10;
        gd->name          = "Eagles";
        gd->country       = 1;
        gd->fame          = 500;
        gd->fame_color    = 0x80FF;
        tworldsvr::TGuildMember m{};
        m.char_id = 200; m.duty = 1; m.peer = 2; m.castle = 50; m.camp = 1;
        gd->members.push_back(m);
        EXPECT(guilds.Insert(gd));
    }
    {
        auto pt = std::make_shared<tworldsvr::TParty>();
        pt->id            = 7;
        pt->obtain_type   = 1;
        pt->chief_char_id = 200;
        pt->AddMember(200);
        EXPECT(parties.Insert(pt));
    }

    // ADDCHAR for both — Alice (42) + Bob (200) — and seed Alice's
    // chat_ban_time + Bob's guild_id / party_id back-pointers.
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, 0xA1));
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(200, 0xB0));
    for (int i = 0; i < 200 && (!chars.Find(42) || !chars.Find(200)); ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(chars.Find(42) != nullptr);
    EXPECT(chars.Find(200) != nullptr);
    const std::int64_t future = static_cast<std::int64_t>(std::time(nullptr))
                              + 3600;     // 1 hour from now
    {
        auto a = chars.Find(42);
        std::lock_guard g(a->lock);
        a->chat_ban_time = future;
        a->name          = "Alice";
    }
    {
        auto b = chars.Find(200);
        std::lock_guard g(b->lock);
        b->guild_id = 10;
        b->party_id = 7;
        b->name     = "Bob";
    }

    // --- Test A: Alice (no guild / party, chat-banned) -------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_ENTERSVR_ACK),
                   EnterSvrBody(42, 0xA1, "Alice", /*level=*/10,
                       /*region=*/5, /*channel=*/3, /*map_id=*/100,
                       1.0f, 2.0f, 3.0f, /*title=*/0, /*rank=*/0));
        // 1) CHARINFO_REQ — all zeros in guild/tactics/party slots.
        auto [w1, b1] = ReadFramed(p1);
        EXPECT(w1 == ToUint16(MessageId::MW_CHARINFO_REQ));
        CharInfo c = ParseCharInfo(b1);
        EXPECT(c.cid == 42);
        EXPECT(c.key == 0xA1);
        EXPECT(c.guild_id == 0);
        EXPECT(c.guild_country == 2);   // TCONTRY_N (default in payload)
        EXPECT(c.guild_name.empty());
        EXPECT(c.tactics_id == 0);
        EXPECT(c.tactics_name.empty());
        EXPECT(c.duty == 0);
        EXPECT(c.peer == 0);
        EXPECT(c.castle == 0);
        EXPECT(c.party_id == 0);
        EXPECT(c.party_chief == 0);
        EXPECT(c.title_id == 0);
        EXPECT(c.rank_point == 0);

        // 2) ROUTE_REQ — channel/map/pos preserved.
        auto [w2, b2] = ReadFramed(p1);
        EXPECT(w2 == ToUint16(MessageId::MW_ROUTE_REQ));
        wire::Reader r2(b2);
        std::uint32_t cid = 0, key = 0; std::uint8_t ch = 0;
        std::uint16_t map_id = 0; float x = 0, y = 0, z = 0;
        r2.Read(cid); r2.Read(key); r2.Read(ch); r2.Read(map_id);
        r2.Read(x); r2.Read(y); r2.Read(z);
        EXPECT(cid == 42);
        EXPECT(ch == 3);
        EXPECT(map_id == 100);
        EXPECT(x == 1.0f); EXPECT(y == 2.0f); EXPECT(z == 3.0f);

        // 3) FRIENDLIST_REQ — empty roster (Alice has no friends here).
        auto [w3, b3] = ReadFramed(p1);
        EXPECT(w3 == ToUint16(MessageId::MW_FRIENDLIST_REQ));
        wire::Reader r3(b3);
        std::uint32_t fcid = 0, fkey = 0, soul = 0;
        std::uint8_t  gc = 0, fc = 0;
        r3.Read(fcid); r3.Read(fkey); r3.Read(soul);
        r3.Read(gc); EXPECT(gc == 0);
        r3.Read(fc); EXPECT(fc == 0);
        EXPECT(fcid == 42);
        EXPECT(soul == 0);              // sentinel (deferred)

        // 4) CHATBAN_REQ — active ban enforced on the new connection.
        auto [w4, b4] = ReadFramed(p1);
        EXPECT(w4 == ToUint16(MessageId::MW_CHATBAN_REQ));
        wire::Reader r4(b4);
        std::string   name_in;
        std::int64_t  ban_time = 0;
        std::uint8_t  result = 0xFF;
        std::uint32_t bcid = 0xDEAD, bkey = 0xBEEF;
        r4.ReadString(name_in); r4.Read(ban_time); r4.Read(result);
        r4.Read(bcid); r4.Read(bkey);
        EXPECT(name_in == "Alice");
        EXPECT(ban_time == future);
        EXPECT(bcid == 0);              // not a GM-issued echo
        EXPECT(bkey == 0);
    }

    // --- Test B: Bob (in guild + party, no ban) --------------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_ENTERSVR_ACK),
                   EnterSvrBody(200, 0xB0, "Bob", /*level=*/40,
                       /*region=*/9, /*channel=*/5, /*map_id=*/777,
                       11.0f, 22.0f, 33.0f, /*title=*/99, /*rank=*/12345));
        // 1) CHARINFO_REQ — populated with the guild + party state.
        auto [w1, b1] = ReadFramed(p1);
        EXPECT(w1 == ToUint16(MessageId::MW_CHARINFO_REQ));
        CharInfo c = ParseCharInfo(b1);
        EXPECT(c.cid == 200);
        EXPECT(c.key == 0xB0);
        EXPECT(c.guild_id == 10);
        EXPECT(c.guild_country == 1);
        EXPECT(c.guild_name == "Eagles");
        EXPECT(c.fame == 500);
        EXPECT(c.fame_color == 0x80FF);
        EXPECT(c.duty == 1);
        EXPECT(c.peer == 2);
        EXPECT(c.castle == 50);
        EXPECT(c.camp == 1);
        EXPECT(c.party_id == 7);
        EXPECT(c.obtain_type == 1);
        EXPECT(c.party_chief == 200);
        EXPECT(c.title_id == 99);
        EXPECT(c.rank_point == 12345);
        EXPECT(c.bow_release == 0);

        // 2) ROUTE_REQ — same shape.
        auto [w2, b2] = ReadFramed(p1);
        EXPECT(w2 == ToUint16(MessageId::MW_ROUTE_REQ));
        wire::Reader r2(b2);
        std::uint32_t cid = 0, key = 0; std::uint8_t ch = 0;
        std::uint16_t map_id = 0; float x = 0, y = 0, z = 0;
        r2.Read(cid); r2.Read(key); r2.Read(ch); r2.Read(map_id);
        r2.Read(x); r2.Read(y); r2.Read(z);
        EXPECT(cid == 200);
        EXPECT(ch == 5);
        EXPECT(map_id == 777);
        EXPECT(x == 11.0f); EXPECT(z == 33.0f);

        // 3) FRIENDLIST_REQ — empty (Bob has no friends here).
        auto [w3, _] = ReadFramed(p1);
        EXPECT(w3 == ToUint16(MessageId::MW_FRIENDLIST_REQ));

        // No CHATBAN_REQ — verified by re-emitting ENTERSVR and
        // confirming the very next packet is *its* CHARINFO_REQ. If
        // Bob's path had emitted a CHATBAN_REQ, that would arrive
        // first and trip the wID check below.
        SendFramed(p1, ToUint16(MessageId::MW_ENTERSVR_ACK),
                   EnterSvrBody(200, 0xB0, "Bob", 40, 9, 5, 777,
                       11.0f, 22.0f, 33.0f, 99, 12345));
        auto [w_next, _next] = ReadFramed(p1);
        EXPECT(w_next == ToUint16(MessageId::MW_CHARINFO_REQ));
        // Drain the follow-up ROUTE + FRIENDLIST so the close is clean.
        { auto [w, _b] = ReadFramed(p1); (void)_b; (void)w; }
        { auto [w, _b] = ReadFramed(p1); (void)_b; (void)w; }
    }

    p1.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_entersvr_fresh_login_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_entersvr_fresh_login_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
