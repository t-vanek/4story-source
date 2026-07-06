// W6-46 wire tests: EVENTQUARTER operator tools.
//
//   CT_EVENTQUARTERLIST_REQ — day-filtered listing with resolved
//     item names, ACK'd to the identified ctrl-svr (byte-checked
//     LUCKYEVENT codec round-trip); empty day → count=0.
//   CT_EVENTQUARTERUPDATE_REQ — EK_ADD adopts the repo-assigned id
//     + resolved names; the repo call trace carries the parsed
//     event; unidentified ctrl slot → silent drop (sentinel).

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/fake_lucky_event_repository.h"
#include "../services/guild_registry.h"
#include "../services/lucky_event_codec.h"
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
#include <string>
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
    tworldsvr::CharRegistry             chars;
    tworldsvr::GuildRegistry            guilds;
    tworldsvr::PeerRegistry             peers;
    tworldsvr::CtrlSvrSlot              ctrl_svr;
    tworldsvr::FakeLuckyEventRepository lucky_repo;
    {
        tworldsvr::LuckyEvent e{};
        e.id = 7; e.day = 3; e.hour = 14; e.minute = 30;
        e.item_id[0] = 100; e.item_id[1] = 200;
        e.count = 5;
        e.present = "P"; e.announce = "A"; e.title = "T";
        e.message = "M";
        lucky_repo.SeedRow(e);
    }

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.ctrl_svr = &ctrl_svr;
    ctx.lucky_repo = &lucky_repo;
    ctx.nation = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0; svr_cfg.max_connections = 4; svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket p1(client_io), pc(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep); pc.connect(ep);
    std::this_thread::sleep_for(20ms);

    SendFramed(p1, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0442));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }

    // --- W6-46a: unidentified ctrl slot → silent drop ----------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 50);
        wire::WritePOD<std::uint8_t>(b, 3);
        SendFramed(pc, ToUint16(MessageId::CT_EVENTQUARTERLIST_REQ), b);
    }
    std::this_thread::sleep_for(30ms);

    SendFramed(pc, ToUint16(MessageId::CT_CTRLSVR_REQ), {});
    std::this_thread::sleep_for(20ms);
    EXPECT(ctrl_svr.Get() != nullptr);

    // --- W6-46b: day listing (first frame pc ever gets — proves the
    // 46a drop) ------------------------------------------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 51);
        wire::WritePOD<std::uint8_t>(b, 3);
        SendFramed(pc, ToUint16(MessageId::CT_EVENTQUARTERLIST_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_EVENTQUARTERLIST_ACK));
        wire::Reader r(b);
        std::uint32_t mgr = 0; std::uint16_t count = 0;
        r.Read(mgr); r.Read(count);
        EXPECT(mgr == 51);
        EXPECT(count == 1);
        tworldsvr::LuckyEvent e{};
        EXPECT(tworldsvr::ReadLuckyEvent(r, e));
        EXPECT(e.id == 7);
        EXPECT(e.day == 3 && e.hour == 14 && e.minute == 30);
        EXPECT(e.item_id[0] == 100 && e.item_id[1] == 200);
        EXPECT(e.count == 5);
        EXPECT(e.present == "P" && e.announce == "A");
        EXPECT(e.title == "T" && e.message == "M");
        EXPECT(e.item_name[0] == "item100");
        EXPECT(e.item_name[1] == "item200");
        EXPECT(e.item_name[2].empty());
    }
    // Empty day → count 0.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 52);
        wire::WritePOD<std::uint8_t>(b, 6);
        SendFramed(pc, ToUint16(MessageId::CT_EVENTQUARTERLIST_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_EVENTQUARTERLIST_ACK));
        wire::Reader r(b);
        std::uint32_t mgr = 0; std::uint16_t count = 0xFFFF;
        r.Read(mgr); r.Read(count);
        EXPECT(mgr == 52);
        EXPECT(count == 0);
    }

    // --- W6-46c: EK_ADD adopts the repo id + names -------------------
    lucky_repo.SetNextAddId(70);
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 53);
        wire::WritePOD<std::uint8_t>(b, tworldsvr::kEkAdd);
        tworldsvr::LuckyEvent e{};
        e.id = 0; e.day = 5; e.hour = 9; e.minute = 15;
        e.item_id[0] = 300;
        e.count = 1;
        e.present = "PP"; e.announce = "AA"; e.title = "TT";
        e.message = "MM";
        tworldsvr::WriteLuckyEvent(b, e);
        SendFramed(pc, ToUint16(MessageId::CT_EVENTQUARTERUPDATE_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_EVENTQUARTERUPDATE_ACK));
        wire::Reader r(b);
        std::uint8_t ret = 0xFF, type = 0xFF;
        std::uint32_t mgr = 0;
        r.Read(ret); r.Read(mgr); r.Read(type);
        EXPECT(ret == 0);
        EXPECT(mgr == 53);
        EXPECT(type == tworldsvr::kEkAdd);
        tworldsvr::LuckyEvent e{};
        EXPECT(tworldsvr::ReadLuckyEvent(r, e));
        EXPECT(e.id == 70);                 // repo-assigned
        EXPECT(e.day == 5);
        EXPECT(e.item_name[0] == "item300");
        const auto calls = lucky_repo.UpdateCalls();
        EXPECT(calls.size() == 1);
        if (!calls.empty())
        {
            EXPECT(calls[0].first == tworldsvr::kEkAdd);
            EXPECT(calls[0].second.day == 5);
            EXPECT(calls[0].second.title == "TT");
        }
    }

    p1.close(); pc.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_eventquarter_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_eventquarter_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
