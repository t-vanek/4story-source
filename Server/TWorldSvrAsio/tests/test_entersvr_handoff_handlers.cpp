// W6-16 wire test: ENTERSVR_ACK main-session handoff completion.
//
// After a handoff (W6-14/W6-15), the new main replies MW_ENTERSVR_ACK.
// When the char's chg_main_id is a normal map id, world clears it and
// asks the new main for the full server list (MAPSVRLIST_REQ → W6-13
// reconcile) instead of running the fresh-login path. A chg_main_id of
// BR_SERVER_ID (50) is excluded — that falls through to fresh login.
//
// Two ENTERSVR_ACKs are sent on one socket (processed in order): the
// BR-excluded char first (must NOT emit MAPSVRLIST), then the handoff
// char. Reading exactly one MAPSVRLIST for the handoff char proves the
// BR char emitted nothing.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
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

// The 25-field MW_ENTERSVR_ACK packet (SSHandler.cpp:1218), with the
// channel / map / pos fields parameterised for the MAPSVRLIST assert.
std::vector<std::byte> EnterSvrBody(std::uint32_t char_id, std::uint32_t key,
                                    const std::string& name,
                                    std::uint8_t channel, std::uint16_t map_id,
                                    float px, float py, float pz)
{
    using namespace tworldsvr::wire;
    std::vector<std::byte> b;
    WritePOD<std::uint32_t>(b, char_id);
    WritePOD<std::uint32_t>(b, key);
    WriteString(b, name);
    WritePOD<std::uint8_t>(b, 10);         // level
    WritePOD<std::uint8_t>(b, 0);          // real_sex
    WritePOD<std::uint8_t>(b, 1);          // class
    WritePOD<std::uint8_t>(b, 0);          // race
    WritePOD<std::uint8_t>(b, 0);          // sex
    WritePOD<std::uint8_t>(b, 0);          // face
    WritePOD<std::uint8_t>(b, 0);          // hair
    WritePOD<std::uint8_t>(b, 0);          // helmet_hide
    WritePOD<std::uint8_t>(b, 0);          // country
    WritePOD<std::uint8_t>(b, 0);          // aid_country
    WritePOD<std::uint32_t>(b, 0);         // region
    WritePOD<std::uint8_t>(b, channel);    // channel
    WritePOD<std::uint16_t>(b, map_id);    // map_id
    WritePOD<float>(b, px);                // pos_x
    WritePOD<float>(b, py);                // pos_y
    WritePOD<float>(b, pz);                // pos_z
    WritePOD<std::uint8_t>(b, 0);          // logout
    WritePOD<std::uint8_t>(b, 0);          // save
    WritePOD<std::uint8_t>(b, 0);          // result (0 = ok)
    WritePOD<std::uint16_t>(b, 0);         // title_id
    WritePOD<std::uint32_t>(b, 0);         // rank_point
    WritePOD<std::uint32_t>(b, 0x7f000001);// user_ip
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
    tworldsvr::CharRegistry   chars;
    tworldsvr::GuildRegistry  guilds;
    tworldsvr::PeerRegistry   peers;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.nation = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0; svr_cfg.max_connections = 8; svr_cfg.ctx = ctx;
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

    auto cons_size = [&](std::uint32_t id) -> std::size_t {
        auto c = chars.Find(id);
        if (!c) return 0;
        std::lock_guard g(c->lock);
        return c->cons.size();
    };
    auto establish = [&](std::uint32_t id, std::uint32_t key) {
        SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK),
                   AddCharBody(id, key));
        for (int i = 0; i < 200 && !chars.Find(id); ++i)
            std::this_thread::sleep_for(10ms);
        SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK),
                   AddCharBody(id, key));
        for (int i = 0; i < 200 && cons_size(id) != 2; ++i)
            std::this_thread::sleep_for(10ms);
    };
    establish(100, 0xA1);   // handoff char
    establish(200, 0xB0);   // BR-excluded char
    EXPECT(cons_size(100) == 2);
    EXPECT(cons_size(200) == 2);

    // char 100: normal handoff (old main = 0x42). char 200: handoff
    // into the BR battleground (chg_main_id = 50) — must be excluded.
    if (auto a = chars.Find(100))
    { std::lock_guard g(a->lock); a->chg_main_id = 0x42; }
    if (auto b = chars.Find(200))
    { std::lock_guard g(b->lock); b->chg_main_id = 50; }

    // BR-excluded char first (no MAPSVRLIST), then the handoff char.
    SendFramed(p2, ToUint16(MessageId::MW_ENTERSVR_ACK),
               EnterSvrBody(200, 0xB0, "CharB", 1, 1, 0, 0, 0));
    SendFramed(p2, ToUint16(MessageId::MW_ENTERSVR_ACK),
               EnterSvrBody(100, 0xA1, "CharA", 4, 800, 11.0f, 22.0f, 33.0f));

    // p2's first (and only) reply is the handoff char's MAPSVRLIST — if
    // the BR char had wrongly emitted one, it would arrive first.
    {
        auto [w, got] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_MAPSVRLIST_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0; std::uint8_t channel = 0;
        std::uint16_t map_id = 0; float x = 0, y = 0, z = 0;
        r.Read(cid); r.Read(key); r.Read(channel); r.Read(map_id);
        r.Read(x); r.Read(y); r.Read(z);
        EXPECT(cid == 100); EXPECT(key == 0xA1);
        EXPECT(channel == 4); EXPECT(map_id == 800);
        EXPECT(x == 11.0f); EXPECT(z == 33.0f);
    }

    // Handoff char's flag cleared; BR char's flag left intact.
    {
        auto a = chars.Find(100);
        std::lock_guard g(a->lock);
        EXPECT(a->chg_main_id == 0);
    }
    {
        auto b = chars.Find(200);
        std::lock_guard g(b->lock);
        EXPECT(b->chg_main_id == 50);
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_entersvr_handoff_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_entersvr_handoff_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
