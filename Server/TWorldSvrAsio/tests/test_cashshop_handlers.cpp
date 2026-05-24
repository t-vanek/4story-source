// W6-33 wire tests: cash-shop sale + global stop relays + replay on
// map-peer connect.
//
// CT_CASHITEMSALE_REQ:
//   - activate (value!=0) → store + broadcast MW_CASHITEMSALE_REQ
//   - deactivate (value==0) → zero sale_value on every item in the
//     stored row, broadcast the zeroed row; miss on dw_index = drop
//   - replay-on-connect: a joining map gets every active campaign
//
// CT_CASHSHOPSTOP_REQ → MW_CASHSHOPSTOP_REQ broadcast (always with
// send_player=1, legacy default).

#include "../handlers/handlers.h"
#include "../services/cash_item_sale_registry.h"
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

// CT_CASHITEMSALE_REQ body builder.
std::vector<std::byte>
SaleBody(std::uint32_t dw_index, std::uint16_t value,
         const std::vector<std::pair<std::uint16_t, std::uint8_t>>& items)
{
    namespace wire = tworldsvr::wire;
    std::vector<std::byte> b;
    wire::WritePOD<std::uint32_t>(b, dw_index);
    wire::WritePOD<std::uint16_t>(b, value);
    wire::WritePOD<std::uint16_t>(b,
        static_cast<std::uint16_t>(items.size()));
    for (auto& it : items)
    {
        wire::WritePOD<std::uint16_t>(b, it.first);
        wire::WritePOD<std::uint8_t>(b, it.second);
    }
    return b;
}

struct SalePacket
{
    std::uint32_t dw_index = 0;
    std::uint16_t value    = 0;
    std::vector<std::pair<std::uint16_t, std::uint8_t>> items;
};

SalePacket ParseSale(const std::vector<std::byte>& body)
{
    namespace wire = tworldsvr::wire;
    wire::Reader r(body);
    SalePacket out{};
    std::uint16_t count = 0;
    r.Read(out.dw_index); r.Read(out.value); r.Read(count);
    out.items.reserve(count);
    for (std::uint16_t i = 0; i < count; ++i)
    {
        std::uint16_t id = 0;
        std::uint8_t  sv = 0;
        r.Read(id); r.Read(sv);
        out.items.emplace_back(id, sv);
    }
    return out;
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
    tworldsvr::CharRegistry        chars;
    tworldsvr::GuildRegistry       guilds;
    tworldsvr::PeerRegistry        peers;
    tworldsvr::EventRegistry       events;
    tworldsvr::CashItemSaleRegistry cash_sales;
    tworldsvr::HandlerContext      ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.events = &events;
    ctx.cash_sales = &cash_sales;
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
        RelaysvrBody(0x0042));
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    SendFramed(p2, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0043));
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    // --- W6-33a: activate → store + broadcast ----------------------
    SendFramed(p1, ToUint16(MessageId::CT_CASHITEMSALE_REQ),
        SaleBody(/*dw_index=*/100, /*value=*/50,
            {{1001, 25}, {1002, 50}, {1003, 75}}));
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CASHITEMSALE_REQ));
        auto pkt = ParseSale(b);
        EXPECT(pkt.dw_index == 100);
        EXPECT(pkt.value == 50);
        EXPECT(pkt.items.size() == 3);
        EXPECT(pkt.items[0].first == 1001);
        EXPECT(pkt.items[0].second == 25);
        EXPECT(pkt.items[1].first == 1002);
        EXPECT(pkt.items[1].second == 50);
        EXPECT(pkt.items[2].first == 1003);
        EXPECT(pkt.items[2].second == 75);
    }
    EXPECT(cash_sales.Size() == 1);

    // --- W6-33b: deactivate → zero sale_value + broadcast ----------
    SendFramed(p1, ToUint16(MessageId::CT_CASHITEMSALE_REQ),
        SaleBody(/*dw_index=*/100, /*value=*/0, {}));
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CASHITEMSALE_REQ));
        auto pkt = ParseSale(b);
        EXPECT(pkt.dw_index == 100);
        EXPECT(pkt.value == 0);
        // Items stay (legacy parity — the entry is kept so replay
        // still shows it), but every sale_value is now 0.
        EXPECT(pkt.items.size() == 3);
        for (auto& it : pkt.items)
            EXPECT(it.second == 0);
    }
    EXPECT(cash_sales.Size() == 1);   // still present, just zeroed

    // --- W6-33c: deactivate miss → silent drop ---------------------
    // Use a dw_index we never sent → no peer should receive anything.
    // We sentinel-test it by sending a CASHSHOPSTOP next and asserting
    // that's the next frame both peers see.
    SendFramed(p1, ToUint16(MessageId::CT_CASHITEMSALE_REQ),
        SaleBody(/*dw_index=*/999, /*value=*/0, {}));
    std::this_thread::sleep_for(30ms);

    // --- W6-33d: CASHSHOPSTOP broadcast ----------------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 7);   // type
        SendFramed(p1, ToUint16(MessageId::CT_CASHSHOPSTOP_REQ), b);
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CASHSHOPSTOP_REQ));
        wire::Reader r(b);
        std::uint8_t type = 0, send_player = 0;
        r.Read(type); r.Read(send_player);
        EXPECT(type == 7);
        EXPECT(send_player == 1);   // legacy default
    }

    // --- W6-33e: replay-on-connect ---------------------------------
    // A third peer joining should receive the (zeroed) row for
    // dw_index=100 because the registry still has it.
    tcp::socket p3(client_io);
    p3.connect(ep);
    std::this_thread::sleep_for(20ms);
    SendFramed(p3, ToUint16(MessageId::RW_RELAYSVR_REQ),
        RelaysvrBody(0x0044));
    { auto [w, _] = ReadFramed(p3);
      EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK)); }
    {
        auto [w, b] = ReadFramed(p3);
        EXPECT(w == ToUint16(MessageId::MW_CASHITEMSALE_REQ));
        auto pkt = ParseSale(b);
        EXPECT(pkt.dw_index == 100);
        EXPECT(pkt.value == 0);
        EXPECT(pkt.items.size() == 3);
        for (auto& it : pkt.items)
            EXPECT(it.second == 0);
    }
    // The p1 / p2 receive the RELAYCONNECT broadcast from p3's join.
    { auto [w, _] = ReadFramed(p1);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }
    { auto [w, _] = ReadFramed(p2);
      EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ)); }

    // --- W6-34: CMGift result relay (in-game GM path, tool=0) ------
    // Seed a GM char on p1 (wID=0x0042, so main_server_id LOBYTE=0x42).
    {
        auto gm = std::make_shared<tworldsvr::TChar>();
        gm->char_id        = 1234;
        gm->key            = 0xABCDEF01;
        gm->name           = "GM_Alice";
        gm->main_server_id = 0x42;
        chars.Insert(gm);
    }
    // p3 (an arbitrary map peer) reports the gift result. World should
    // route MW_CMGIFTRESULT_REQ to p1 (GM's main map).
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 0);      // result (CMGIFT_SUCCESS=0)
        wire::WritePOD<std::uint8_t>(b, 0);      // tool=0 (in-game GM)
        wire::WritePOD<std::uint32_t>(b, 1234);  // gm_id
        SendFramed(p3, ToUint16(MessageId::MW_CMGIFTRESULT_ACK), b);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CMGIFTRESULT_REQ));
        wire::Reader r(b);
        std::uint8_t  result = 0xFF;
        std::uint32_t gm_id  = 0;
        r.Read(result); r.Read(gm_id);
        EXPECT(result == 0);
        EXPECT(gm_id == 1234);
    }

    // --- W6-34b: tool=1 admin path → log + drop (no broadcast) -----
    // Send tool=1; expect no peer to receive anything. Sentinel: send
    // a CASHSHOPSTOP afterward and confirm both p1+p2 receive that.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 0);
        wire::WritePOD<std::uint8_t>(b, 1);     // tool=1 (admin)
        wire::WritePOD<std::uint32_t>(b, 1234);
        SendFramed(p3, ToUint16(MessageId::MW_CMGIFTRESULT_ACK), b);
    }
    std::this_thread::sleep_for(30ms);
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 99);    // sentinel type
        SendFramed(p3, ToUint16(MessageId::CT_CASHSHOPSTOP_REQ), b);
    }
    for (auto* s : {&p1, &p2, &p3})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CASHSHOPSTOP_REQ));
        wire::Reader r(b);
        std::uint8_t type = 0, send_player = 0;
        r.Read(type); r.Read(send_player);
        EXPECT(type == 99);   // sentinel — confirms no admin-path frame
    }

    // --- W6-34c: unknown gm_id → silent drop -----------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 0);
        wire::WritePOD<std::uint8_t>(b, 0);     // tool=0
        wire::WritePOD<std::uint32_t>(b, 9999); // unknown char
        SendFramed(p3, ToUint16(MessageId::MW_CMGIFTRESULT_ACK), b);
    }
    std::this_thread::sleep_for(30ms);
    // Sentinel: another CASHSHOPSTOP must be the next frame everywhere.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 100);
        SendFramed(p3, ToUint16(MessageId::CT_CASHSHOPSTOP_REQ), b);
    }
    for (auto* s : {&p1, &p2, &p3})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_CASHSHOPSTOP_REQ));
        wire::Reader r(b);
        std::uint8_t type = 0, send_player = 0;
        r.Read(type); r.Read(send_player);
        EXPECT(type == 100);
    }

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_cashshop_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_cashshop_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
