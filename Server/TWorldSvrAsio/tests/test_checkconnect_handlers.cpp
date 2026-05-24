// W6-18 wire test: CHECKCONNECT_ACK — position + connection reconcile.
//
// Three map peers (svr 0x42 main / 0x43 / 0x44). A char connected to
// 0x42+0x43:
//   * count=0 → update position + CHECKMAIN sweep to every con.
//   * count>0 listing a new server (0x44) → update position + ROUTELIST
//     the new server via the main map.
//   * unknown char → DELCHAR.

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

// MW_CHECKCONNECT_ACK body (SSHandler.cpp:3866).
std::vector<std::byte> CheckConnectBody(std::uint32_t char_id, std::uint32_t key,
                                        std::uint8_t channel, std::uint16_t map_id,
                                        float px, float py, float pz,
                                        const std::vector<std::uint8_t>& servers)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint8_t>(b, channel);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, map_id);
    tworldsvr::wire::WritePOD<float>(b, px);
    tworldsvr::wire::WritePOD<float>(b, py);
    tworldsvr::wire::WritePOD<float>(b, pz);
    tworldsvr::wire::WritePOD<std::uint8_t>(
        b, static_cast<std::uint8_t>(servers.size()));
    for (std::uint8_t s : servers)
        tworldsvr::wire::WritePOD<std::uint8_t>(b, s);
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
    establish(100, 0xA1);   // count=0 sweep
    establish(200, 0xB0);   // ROUTELIST a new server
    EXPECT(cons_size(100) == 2);
    EXPECT(cons_size(200) == 2);

    auto map_of = [&](std::uint32_t id) -> std::uint16_t {
        auto c = chars.Find(id);
        std::lock_guard g(c->lock);
        return c->map_id;
    };

    // --- count=0: position update + CHECKMAIN sweep to every con ----
    {
        SendFramed(p1, ToUint16(MessageId::MW_CHECKCONNECT_ACK),
                   CheckConnectBody(100, 0xA1, /*channel=*/5, /*map=*/600,
                                    7.0f, 8.0f, 9.0f, /*servers=*/{}));
        auto check = [&](tcp::socket& s) {
            auto [w, got] = ReadFramed(s);
            EXPECT(w == ToUint16(MessageId::MW_CHECKMAIN_REQ));
            wire::Reader r(got);
            std::uint32_t cid = 0, key = 0; std::uint8_t channel = 0;
            std::uint16_t map_id = 0; float x = 0, y = 0, z = 0;
            r.Read(cid); r.Read(key); r.Read(channel); r.Read(map_id);
            r.Read(x); r.Read(y); r.Read(z);
            EXPECT(cid == 100); EXPECT(channel == 5); EXPECT(map_id == 600);
        };
        check(p1);   // 0x42
        check(p2);   // 0x43
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        { if (map_of(100) == 600) { ok = true; break; }
          std::this_thread::sleep_for(10ms); }
        EXPECT(ok);
    }

    // --- count>0 with a new server (0x44): ROUTELIST via main -------
    {
        SendFramed(p1, ToUint16(MessageId::MW_CHECKCONNECT_ACK),
                   CheckConnectBody(200, 0xB0, /*channel=*/6, /*map=*/700,
                                    1.0f, 2.0f, 3.0f,
                                    /*servers=*/{0x42, 0x43, 0x44}));
        auto [w, got] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_ROUTELIST_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0; std::uint8_t cnt = 0, sid = 0;
        r.Read(cid); r.Read(key); r.Read(cnt); r.Read(sid);
        EXPECT(cid == 200); EXPECT(key == 0xB0);
        EXPECT(cnt == 1); EXPECT(sid == 0x44);
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        { if (map_of(200) == 700) { ok = true; break; }
          std::this_thread::sleep_for(10ms); }
        EXPECT(ok);
        EXPECT(cons_size(200) == 2);   // 0x42/0x43 retained, none dropped
    }

    // --- unknown char → DELCHAR ------------------------------------
    {
        SendFramed(p3, ToUint16(MessageId::MW_CHECKCONNECT_ACK),
                   CheckConnectBody(999, 0xCC, 0, 0, 0, 0, 0, {}));
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
        std::printf("PASS test_tworldsvr_asio_checkconnect_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_checkconnect_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
