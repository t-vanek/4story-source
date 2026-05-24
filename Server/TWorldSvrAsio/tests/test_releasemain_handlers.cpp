// W6-15 wire test: RELEASEMAIN_ACK — main-session handoff forward.
//
// Two map peers (svr 0x42/0x43). After a W6-14 handoff re-points a
// char's main at 0x43, the *old* main (0x42) replies RELEASEMAIN_ACK;
// world forwards the released char verbatim to the new main (0x43) as
// MW_ENTERSVR_REQ and records the old main in chg_main_id.
//   * normal: old main (0x42) → ENTERSVR_REQ to new main (0x43).
//   * new main offline → INVALIDCHAR(release_main=1) to the old main.
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

// RELEASEMAIN_ACK body: BYTE db_load, DWORD char_id, key, + opaque tail.
std::vector<std::byte> ReleaseMainBody(std::uint8_t db_load,
                                       std::uint32_t char_id,
                                       std::uint32_t key,
                                       std::uint32_t marker)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD<std::uint8_t>(b, db_load);
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD(b, marker);   // stand-in for saved state
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

    // Two chars, main 0x42, with a con on 0x43 (serialise the inserts
    // so p1's insert wins the main slot before p2 adds its con).
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
    establish(100, 0xA1);
    establish(200, 0xB0);
    EXPECT(cons_size(100) == 2);
    EXPECT(cons_size(200) == 2);

    // Simulate the post-handoff state: char 100's main is now 0x43
    // (new main); char 200's main points at an offline server (0x99).
    if (auto a = chars.Find(100))
    { std::lock_guard g(a->lock); a->main_server_id = 0x43; }
    if (auto b = chars.Find(200))
    { std::lock_guard g(b->lock); b->main_server_id = 0x99; }

    // --- normal: old main (0x42=p1) releases char 100 --------------
    {
        SendFramed(p1, ToUint16(MessageId::MW_RELEASEMAIN_ACK),
                   ReleaseMainBody(1, 100, 0xA1, 0xDEADBEEF));
        // New main (0x43=p2) gets the verbatim body re-tagged ENTERSVR.
        auto [w, got] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_ENTERSVR_REQ));
        wire::Reader r(got);
        std::uint8_t db = 0; std::uint32_t cid = 0, key = 0, marker = 0;
        r.Read(db); r.Read(cid); r.Read(key); r.Read(marker);
        EXPECT(db == 1); EXPECT(cid == 100); EXPECT(key == 0xA1);
        EXPECT(marker == 0xDEADBEEF);

        // chg_main_id records the old main (0x42).
        bool ok = false;
        for (int i = 0; i < 200; ++i)
        {
            auto c = chars.Find(100);
            std::lock_guard g(c->lock);
            if (c->chg_main_id == 0x42) { ok = true; break; }
            std::this_thread::sleep_for(10ms);
        }
        EXPECT(ok);
    }

    // --- new main offline: char 200 main=0x99 → INVALIDCHAR --------
    {
        SendFramed(p1, ToUint16(MessageId::MW_RELEASEMAIN_ACK),
                   ReleaseMainBody(1, 200, 0xB0, 0));
        auto [w, got] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_INVALIDCHAR_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0; std::uint8_t release_main = 0;
        r.Read(cid); r.Read(key); r.Read(release_main);
        EXPECT(cid == 200); EXPECT(key == 0xB0);
        EXPECT(release_main == 1);
    }

    // --- unknown char → DELCHAR ------------------------------------
    {
        SendFramed(p2, ToUint16(MessageId::MW_RELEASEMAIN_ACK),
                   ReleaseMainBody(1, 999, 0xCC, 0));
        auto [w, got] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_DELCHAR_REQ));
        wire::Reader r(got);
        std::uint32_t cid = 0, key = 0; std::uint8_t logout = 0, save = 1;
        r.Read(cid); r.Read(key); r.Read(logout); r.Read(save);
        EXPECT(cid == 999); EXPECT(logout == 1); EXPECT(save == 0);
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_releasemain_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_releasemain_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
