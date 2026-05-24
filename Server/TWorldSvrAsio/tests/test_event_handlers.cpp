// W6-1 wire test: timed-event broadcast.
//
// EVENTQUARTER fans the present event (with a single server-chosen
// bucket) to every map peer; EVENTQUARTERNOTIFY broadcasts a
// world-chat announcement to every peer.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/chat_constants.h"
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

} // namespace

int main()
{
    using boost::asio::ip::tcp;
    using namespace std::chrono_literals;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    namespace wire = tworldsvr::wire;
    namespace chat = tworldsvr::chat;

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

    // --- EVENTQUARTER → both peers, same bucket -------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 3);    // day
        wire::WritePOD<std::uint8_t>(b, 12);   // hour
        wire::WritePOD<std::uint8_t>(b, 30);   // minute
        wire::WriteString(b, "GiftBox");
        SendFramed(p1, ToUint16(MessageId::SM_EVENTQUARTER_REQ), b);
    }
    int select_seen = -1;
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_EVENTQUARTER_REQ));
        wire::Reader r(b);
        std::uint8_t day = 0, hour = 0, minute = 0, select = 0;
        std::string present;
        r.Read(day); r.Read(hour); r.Read(minute); r.Read(select);
        r.ReadString(present);
        EXPECT(day == 3); EXPECT(hour == 12); EXPECT(minute == 30);
        EXPECT(present == "GiftBox"); EXPECT(select < 100);
        if (select_seen < 0) select_seen = select;
        else EXPECT(select == select_seen);   // same bucket on every map
    }

    // --- EVENTQUARTERNOTIFY → world-chat announcement -------------
    {
        std::vector<std::byte> b;
        wire::WriteString(b, "Event starts now!");
        SendFramed(p1, ToUint16(MessageId::SM_EVENTQUARTERNOTIFY_REQ), b);
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CHAT_REQ));
        wire::Reader r(b);
        std::uint32_t cid = 0, key = 0, sender_id = 0, target = 0;
        std::uint8_t channel = 0, country = 0, war = 0, type = 0, group = 0;
        std::string sender_name, talk;
        r.Read(cid); r.Read(key); r.Read(channel); r.Read(sender_id);
        r.ReadString(sender_name); r.Read(country); r.Read(war);
        r.Read(type); r.Read(group); r.Read(target); r.ReadString(talk);
        EXPECT(group == chat::kWorld);
        EXPECT(talk == "Event starts now!");
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_event_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_event_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
