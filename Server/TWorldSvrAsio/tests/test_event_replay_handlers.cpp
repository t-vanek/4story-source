// W6-32 wire test: event replay on map-peer connect.
//
// A new map-server's RW_RELAYSVR_REQ triggers an EventRegistry
// snapshot walk; world re-fires SendMwEventUpdateReq for every
// active event so the joining map sees the same state as everyone
// else. Legacy parity SSHandler.cpp:662-664.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/event_registry.h"
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
#include <set>
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

// Build a TEventInfo carrying the legacy outer header + a dw_index +
// b_id + a configurable opaque trailer.
tworldsvr::TEventInfo MakeEvent(std::uint32_t dw_index,
                                std::uint8_t  b_id,
                                std::uint8_t  event_id,
                                std::uint16_t value,
                                std::vector<std::byte> trailer)
{
    namespace wire = tworldsvr::wire;
    tworldsvr::TEventInfo info{};
    info.event_id = event_id;
    info.value    = value;
    info.dw_index = dw_index;
    info.b_id     = b_id;
    wire::WritePOD<std::uint32_t>(info.body, dw_index);
    wire::WritePOD<std::uint8_t>(info.body, b_id);
    info.body.insert(info.body.end(), trailer.begin(), trailer.end());
    return info;
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
    tworldsvr::EventRegistry  events;

    // Pre-seed two active events before the server starts accepting.
    // OnRelaysvrReq must snapshot + re-emit both to the joining peer.
    const std::vector<std::byte> trailer1{
        std::byte{0x11}, std::byte{0x22}, std::byte{0x33}};
    const std::vector<std::byte> trailer2{
        std::byte{0xAA}, std::byte{0xBB}};
    events.Set(MakeEvent(/*dw_index=*/0xDEAD0001, /*b_id=*/1,
        /*event_id=*/1, /*value=*/100, trailer1));
    events.Set(MakeEvent(/*dw_index=*/0xDEAD0002, /*b_id=*/2,
        /*event_id=*/2, /*value=*/200, trailer2));
    EXPECT(events.Size() == 2);

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.events = &events; ctx.nation = 0;

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

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0042));

    // Expected packet order on this peer:
    //   1. RW_RELAYSVR_ACK (the W3a-2 handshake reply)
    //   2. N x MW_EVENTUPDATE_REQ (the replay walk — one per active
    //      event). Snapshot order is unordered_map iteration order,
    //      so we collect by dw_index and assert as a set.
    {
        auto [w, _] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK));
    }
    std::set<std::uint32_t> seen_indexes;
    for (int i = 0; i < 2; ++i)
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_EVENTUPDATE_REQ));
        wire::Reader r(b);
        std::uint8_t  event_id = 0, b_id = 0;
        std::uint16_t value = 0;
        std::uint32_t dw_index = 0;
        r.Read(event_id); r.Read(value); r.Read(dw_index); r.Read(b_id);

        if (dw_index == 0xDEAD0001)
        {
            EXPECT(event_id == 1);
            EXPECT(value == 100);
            EXPECT(b_id == 1);
            EXPECT(r.Remaining() == trailer1.size());
            for (std::size_t j = 0; j < trailer1.size(); ++j)
            {
                std::byte t{};
                r.Read(t);
                EXPECT(t == trailer1[j]);
            }
        }
        else if (dw_index == 0xDEAD0002)
        {
            EXPECT(event_id == 2);
            EXPECT(value == 200);
            EXPECT(b_id == 2);
            EXPECT(r.Remaining() == trailer2.size());
            for (std::size_t j = 0; j < trailer2.size(); ++j)
            {
                std::byte t{};
                r.Read(t);
                EXPECT(t == trailer2[j]);
            }
        }
        else
        {
            EXPECT(false);   // unexpected dw_index
        }
        seen_indexes.insert(dw_index);
    }
    EXPECT(seen_indexes.size() == 2);
    EXPECT(seen_indexes.count(0xDEAD0001) == 1);
    EXPECT(seen_indexes.count(0xDEAD0002) == 1);

    // --- Second connect with the registry empty → no replay packets.
    events.Erase(0xDEAD0001);
    events.Erase(0xDEAD0002);
    EXPECT(events.Size() == 0);

    tcp::socket p2(client_io);
    p2.connect(ep);
    std::this_thread::sleep_for(20ms);

    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0043));
    {
        auto [w, _] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK));
    }
    // p1 already registered, so p2 triggers a RELAYCONNECT to p1.
    {
        auto [w, _] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ));
    }
    // p2 should NOT receive any MW_EVENTUPDATE_REQ — verify by
    // sending a follow-up packet and confirming the next reply is
    // for that follow-up (not an old replay frame).
    SendFramed(p2, ToUint16(MessageId::CT_EVENTMSG_REQ), []{
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WritePOD<std::uint8_t>(b, 1);
        wire::WriteString(b, "post-replay sentinel");
        return b;
    }());
    {
        auto [w, _] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_EVENTMSG_REQ));
    }
    {
        auto [w, _] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_EVENTMSG_REQ));
    }

    p1.close(); p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_event_replay_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_event_replay_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
