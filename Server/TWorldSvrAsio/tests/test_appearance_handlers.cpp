// W4-14 wire test: per-character visual state sync.
//
// Alice 42 is connected to two map servers (peer1 0x42 = main,
// peer2 0x43 = secondary):
//   - MW_PETRIDING_ACK from p1 → MW_PETRIDING_REQ to the *other*
//     session (p2) carrying the mount id.
//   - MW_HELMETHIDE_ACK from p1 → MW_HELMETHIDE_REQ echoed back to p1.

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

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;

    boost::asio::io_context io;
    tworldsvr::CharRegistry   chars;
    tworldsvr::GuildRegistry  guilds;
    tworldsvr::PeerRegistry   peers;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.nation = 0;

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

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(0x0043));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    const std::uint32_t kKey = 0xA1;
    // Two connections for the same char: p1 (main 0x42) + p2 (0x43).
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, kKey));
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, kKey));
    for (int i = 0; i < 100; ++i)
    {
        auto c = chars.Find(42);
        if (c) { std::lock_guard g(c->lock); if (c->cons.size() == 2) break; }
        std::this_thread::sleep_for(10ms);
    }
    {
        auto c = chars.Find(42);
        EXPECT(c != nullptr);
        if (c) { std::lock_guard g(c->lock); EXPECT(c->cons.size() == 2); }
    }

    // --- PETRIDING: fan to the other session (p2) -----------------
    {
        std::vector<std::byte> b;
        tworldsvr::wire::WritePOD<std::uint32_t>(b, 42);
        tworldsvr::wire::WritePOD<std::uint32_t>(b, kKey);
        tworldsvr::wire::WritePOD<std::uint32_t>(b, 0x1234);  // riding
        SendFramed(p1, ToUint16(MessageId::MW_PETRIDING_ACK), b);
    }
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_PETRIDING_REQ));
        tworldsvr::wire::Reader r(b);
        std::uint32_t cid = 0, key = 0, riding = 0;
        r.Read(cid); r.Read(key); r.Read(riding);
        EXPECT(cid == 42); EXPECT(key == kKey); EXPECT(riding == 0x1234);
    }
    {
        auto c = chars.Find(42);
        if (c) { std::lock_guard g(c->lock); EXPECT(c->riding == 0x1234); }
    }

    // --- HELMETHIDE: confirm back to the originating map (p1) ------
    {
        std::vector<std::byte> b;
        tworldsvr::wire::WritePOD<std::uint32_t>(b, 42);
        tworldsvr::wire::WritePOD<std::uint32_t>(b, kKey);
        tworldsvr::wire::WritePOD<std::uint8_t>(b, 1);  // hide
        SendFramed(p1, ToUint16(MessageId::MW_HELMETHIDE_ACK), b);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_HELMETHIDE_REQ));
        tworldsvr::wire::Reader r(b);
        std::uint32_t cid = 0, key = 0; std::uint8_t hide = 0;
        r.Read(cid); r.Read(key); r.Read(hide);
        EXPECT(cid == 42); EXPECT(key == kKey); EXPECT(hide == 1);
    }
    {
        auto c = chars.Find(42);
        if (c) { std::lock_guard g(c->lock); EXPECT(c->helmet_hide == 1); }
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_appearance_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_appearance_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
