// W6-39 wire test: the four APEX (Taiwan) stubs accept + drop their
// packets exactly like the shipped `__TW_APEX`-disabled binary — no
// reply, no relay, no char teardown (SM_APEXKILLUSER's enabled-build
// CloseChar must NOT happen). Sentinel: a CASHSHOPSTOP broadcast is
// the next frame both maps see.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
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
#include <memory>
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
    tworldsvr::CharRegistry  chars;
    tworldsvr::GuildRegistry guilds;
    tworldsvr::PeerRegistry  peers;
    tworldsvr::CtrlSvrSlot   ctrl_svr;

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.ctrl_svr = &ctrl_svr;
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

    // Seed a char so SM_APEXKILLUSER has a would-be victim.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 100);         // char_id
        wire::WritePOD<std::uint32_t>(b, 0xA1);        // key
        wire::WritePOD<std::uint32_t>(b, 0x7f000001);  // ip
        wire::WritePOD<std::uint16_t>(b, 33500);       // port
        wire::WritePOD<std::uint32_t>(b, 500);         // user_id
        SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), b);
    }
    for (int i = 0; i < 200 && !chars.Find(100); ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(chars.Size() == 1);

    // Fire all four APEX packets (arbitrary payloads).
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 100);   // char_id
        wire::WritePOD<std::int32_t>(b, 4);      // len
        wire::WritePOD<std::uint32_t>(b, 0xAA55AA55);
        SendFramed(p1, ToUint16(MessageId::SM_APEXDATA_REQ), b);
    }
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 100);   // char_id
        wire::WritePOD<std::int32_t>(b, 1);      // action
        SendFramed(p1, ToUint16(MessageId::SM_APEXKILLUSER_REQ), b);
    }
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 100);
        wire::WritePOD<std::uint32_t>(b, 0xA1);
        wire::WritePOD<std::int32_t>(b, 4);
        wire::WritePOD<std::uint32_t>(b, 0x11223344);
        SendFramed(p1, ToUint16(MessageId::MW_APEXDATA_ACK), b);
    }
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 100);
        wire::WritePOD<std::uint32_t>(b, 0xA1);
        wire::WritePOD<std::int32_t>(b, 7);
        SendFramed(p1, ToUint16(MessageId::MW_APEXSTART_ACK), b);
    }
    std::this_thread::sleep_for(50ms);

    // The char must survive (the enabled-build KILLUSER → CloseChar
    // must NOT run) and no frame may have been emitted.
    EXPECT(chars.Find(100) != nullptr);

    // Sentinel: the next frame on both maps is the CASHSHOPSTOP echo.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 55);
        SendFramed(p1, ToUint16(MessageId::CT_CASHSHOPSTOP_REQ), b);
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CASHSHOPSTOP_REQ));
        wire::Reader r(b);
        std::uint8_t type = 0;
        r.Read(type);
        EXPECT(type == 55);
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_apex_stub_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_apex_stub_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
