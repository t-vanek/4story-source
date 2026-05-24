// W6-14 wire test: CHECKMAIN_ACK — main-session confirmation.
//
// Three map peers (svr 0x42/0x43/0x44). A char connected to 0x42+0x43
// with main 0x42 and a dead con on 0x44:
//   * CHECKMAIN_ACK from the main (0x42) → CLOSECHAR the dead con
//     (0x44) and CONRESULT the live set (0x42,0x43) back to the main.
//   * CHECKMAIN_ACK from a non-main (0x43) → RELEASEMAIN to the old
//     main (0x42) and re-point the char's main at 0x43.
//   * CHECKMAIN_ACK for an unknown char → DELCHAR.

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

std::vector<std::byte> CharKeyBody(std::uint32_t char_id, std::uint32_t key)
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
    tcp::socket p1(client_io), p2(client_io), p3(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep); p2.connect(ep); p3.connect(ep);
    std::this_thread::sleep_for(20ms);

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0043));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }
    SendFramed(p3, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0044));
    { auto [w, _] = ReadFramed(p3);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    auto cons_size = [&](std::uint32_t id) -> std::size_t {
        auto c = chars.Find(id);
        if (!c) return 0;
        std::lock_guard g(c->lock);
        return c->cons.size();
    };
    // Char A (100) + Char B (200): each connected to 0x42 (main) + 0x43.
    // The two ADDCHARs land on different sockets with no cross-socket
    // ordering guarantee, so serialise them: let p1's insert establish
    // main=0x42 before p2 adds its con (otherwise main could race to
    // 0x43, since the first insert wins).
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
    establish(100, 0xA1);
    establish(200, 0xB0);
    EXPECT(cons_size(100) == 2);
    EXPECT(cons_size(200) == 2);

    // Char A gets a dead con on 0x44 (p3) + a known position.
    if (auto a = chars.Find(100))
    {
        std::lock_guard g(a->lock);
        a->dead_cons.push_back(0x44);
        a->channel = 1; a->map_id = 7;
    }
    // Char B gets a known position for the RELEASEMAIN handoff.
    if (auto b = chars.Find(200))
    {
        std::lock_guard g(b->lock);
        b->channel = 3; b->map_id = 900;
        b->pos_x = 5.0f; b->pos_y = 6.0f; b->pos_z = 7.0f;
    }

    // --- main confirmed: CHECKMAIN_ACK from the main (0x42 = p1) ----
    {
        SendFramed(p1, ToUint16(MessageId::MW_CHECKMAIN_ACK),
                   CharKeyBody(100, 0xA1));
        // p3 (0x44) gets CLOSECHAR for the dead con.
        auto [wc, gc] = ReadFramed(p3);
        EXPECT(wc == ToUint16(MessageId::MW_CLOSECHAR_REQ));
        { wire::Reader r(gc); std::uint32_t cid = 0, key = 0;
          r.Read(cid); r.Read(key);
          EXPECT(cid == 100); EXPECT(key == 0xA1); }
        // p1 (main) gets CONRESULT with the live set {0x42,0x43}.
        auto [wr, gr] = ReadFramed(p1);
        EXPECT(wr == ToUint16(MessageId::MW_CONRESULT_REQ));
        wire::Reader r(gr);
        std::uint32_t cid = 0, key = 0;
        std::uint8_t result = 0xFF, cnt = 0, s0 = 0, s1 = 0;
        r.Read(cid); r.Read(key); r.Read(result); r.Read(cnt);
        r.Read(s0); r.Read(s1);
        EXPECT(cid == 100); EXPECT(key == 0xA1);
        EXPECT(result == 0);   // CN_SUCCESS
        EXPECT(cnt == 2); EXPECT(s0 == 0x42); EXPECT(s1 == 0x43);

        // dead_cons drained.
        bool drained = false;
        for (int i = 0; i < 200; ++i)
        {
            auto c = chars.Find(100);
            std::lock_guard g(c->lock);
            if (c->dead_cons.empty()) { drained = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(drained);
    }

    // --- main handoff: CHECKMAIN_ACK from a non-main (0x43 = p2) ----
    {
        SendFramed(p2, ToUint16(MessageId::MW_CHECKMAIN_ACK),
                   CharKeyBody(200, 0xB0));
        // Old main (0x42 = p1) gets RELEASEMAIN with the char's pos.
        auto [w, got] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_RELEASEMAIN_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0; std::uint8_t channel = 0;
        std::uint16_t map_id = 0; float x = 0, y = 0, z = 0;
        r.Read(cid); r.Read(key); r.Read(channel); r.Read(map_id);
        r.Read(x); r.Read(y); r.Read(z);
        EXPECT(cid == 200); EXPECT(key == 0xB0);
        EXPECT(channel == 3); EXPECT(map_id == 900);
        EXPECT(x == 5.0f); EXPECT(z == 7.0f);

        // main re-pointed at the responder (0x43).
        bool repointed = false;
        for (int i = 0; i < 200; ++i)
        {
            auto c = chars.Find(200);
            std::lock_guard g(c->lock);
            if (c->main_server_id == 0x43) { repointed = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(repointed);
    }

    // --- error branch: CHECKMAIN_ACK for an unknown char → DELCHAR --
    {
        SendFramed(p3, ToUint16(MessageId::MW_CHECKMAIN_ACK),
                   CharKeyBody(999, 0xCC));
        auto [w, got] = ReadFramed(p3);
        EXPECT(w == ToUint16(MessageId::MW_DELCHAR_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0; std::uint8_t logout = 0, save = 1;
        r.Read(cid); r.Read(key); r.Read(logout); r.Read(save);
        EXPECT(cid == 999); EXPECT(logout == 1); EXPECT(save == 0);
    }

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_checkmain_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_checkmain_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
