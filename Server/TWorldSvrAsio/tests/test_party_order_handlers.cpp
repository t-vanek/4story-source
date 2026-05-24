// W3b-6 wire test: PT_ORDER round-robin loot (MW_PARTYORDERTAKEITEM)
// over a three-peer loopback session.
//
// Party id=100 [Alice 42/peer1 0x42, Bob 200/peer2 0x43,
// Carol 400/peer3 0x44]; member order [42,200,400] → the loot
// cursor seeds on Alice.
//
// Scenarios:
//   1. Drop (all eligible) → Alice's turn; item forwarded to p1.
//   2. Drop (all eligible) → cursor advanced → Bob; item to p2.
//   3. Drop (only Carol eligible, cursor on Carol) → Carol; p3 +
//      item field round-trip through the cabinet codec verified.
//   4. Drop on a stale party id → MIT_NOTFOUND to the reporter.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/guild_cabinet_codec.h"
#include "../services/guild_registry.h"
#include "../services/party_constants.h"
#include "../services/party_registry.h"
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

namespace party = tworldsvr::party;

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

tworldsvr::TGuildCabinetItem MakeItem()
{
    tworldsvr::TGuildCabinetItem it;
    it.id        = 0xABCDEF12;
    it.item_id_b = 7;
    it.item_id_w = 1234;
    it.level     = 5;
    it.count     = 3;
    it.magic.emplace_back(2, 500);
    return it;
}

std::vector<std::byte> OrderBody(std::uint32_t char_id, std::uint32_t key,
                                  std::uint16_t party_id,
                                  const std::vector<std::uint32_t>& members,
                                  const tworldsvr::TGuildCabinetItem& item)
{
    std::vector<std::byte> b;
    using namespace tworldsvr::wire;
    WritePOD<std::uint32_t>(b, char_id);
    WritePOD<std::uint32_t>(b, key);
    WritePOD<std::uint16_t>(b, party_id);
    WritePOD<std::uint8_t>(b, /*server_id=*/2);
    WritePOD<std::uint8_t>(b, /*channel=*/1);
    WritePOD<std::uint16_t>(b, /*map_id=*/50);
    WritePOD<std::uint32_t>(b, /*mon_id=*/0xDEAD);
    WritePOD<std::uint16_t>(b, /*temp_mon_id=*/9);
    WritePOD<std::uint8_t>(b, static_cast<std::uint8_t>(members.size()));
    for (auto m : members) WritePOD<std::uint32_t>(b, m);
    tworldsvr::WriteCabinetItem(b, item);
    return b;
}

struct OrderReq {
    std::uint32_t char_id = 0, key = 0, mon_id = 0;
    std::uint8_t  server_id = 0, channel = 0;
    std::uint16_t map_id = 0, temp_mon_id = 0;
    tworldsvr::TGuildCabinetItem item;
};
OrderReq DecodeOrderReq(const std::vector<std::byte>& b)
{
    OrderReq o{};
    tworldsvr::wire::Reader r(b);
    r.Read(o.char_id); r.Read(o.key); r.Read(o.server_id); r.Read(o.channel);
    r.Read(o.map_id); r.Read(o.mon_id); r.Read(o.temp_mon_id);
    tworldsvr::ReadCabinetItem(r, o.item);
    return o;
}

struct AddResult {
    std::uint32_t char_id = 0, key = 0, mon_id = 0;
    std::uint8_t  channel = 0; std::uint16_t map_id = 0;
    std::uint8_t  item_id = 0, result = 0;
};
AddResult DecodeAddResult(const std::vector<std::byte>& b)
{
    AddResult a{};
    tworldsvr::wire::Reader r(b);
    r.Read(a.char_id); r.Read(a.key); r.Read(a.channel); r.Read(a.map_id);
    r.Read(a.mon_id); r.Read(a.item_id); r.Read(a.result);
    return a;
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
    tworldsvr::PartyRegistry  parties;
    tworldsvr::PeerRegistry   peers;
    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.parties = &parties; ctx.peers = &peers; ctx.nation = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0; svr_cfg.max_connections = 6; svr_cfg.ctx = ctx;
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

    auto reg = [&](tcp::socket& s, std::uint16_t wid) {
        SendFramed(s, ToUint16(MessageId::RW_RELAYSVR_REQ), RelaysvrBody(wid));
        auto [w, _] = ReadFramed(s);
        EXPECT(w == ToUint16(MessageId::RW_RELAYSVR_ACK));
    };
    auto drain = [&](tcp::socket& s) {
        auto [w, _] = ReadFramed(s);
        EXPECT(w == ToUint16(MessageId::MW_RELAYCONNECT_REQ));
    };
    reg(p1, 0x0042);
    reg(p2, 0x0043); drain(p1);
    reg(p3, 0x0044); drain(p1); drain(p2);
    EXPECT(peers.Size() == 3);

    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(42, 0xA1));
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(200, 0xB0));
    SendFramed(p3, ToUint16(MessageId::MW_ADDCHAR_ACK), AddCharBody(400, 0xCA));
    for (int i = 0; i < 100; ++i)
    {
        if (chars.Find(42) && chars.Find(200) && chars.Find(400)) break;
        std::this_thread::sleep_for(10ms);
    }
    EXPECT(chars.Find(42) && chars.Find(200) && chars.Find(400));

    constexpr std::uint16_t kPid = 100;
    {
        auto pty = std::make_shared<tworldsvr::TParty>();
        pty->id = kPid; pty->chief_char_id = 42;
        pty->AddMember(42);   // seeds order cursor on 42
        pty->AddMember(200);
        pty->AddMember(400);
        EXPECT(parties.Insert(pty));
    }

    const auto item = MakeItem();
    const std::vector<std::uint32_t> all{42, 200, 400};

    // --- Scenario 1: cursor on Alice → item to p1 -------------------
    SendFramed(p1, ToUint16(MessageId::MW_PARTYORDERTAKEITEM_ACK),
        OrderBody(42, 0xA1, kPid, all, item));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_PARTYORDERTAKEITEM_REQ));
        auto o = DecodeOrderReq(b);
        EXPECT(o.char_id == 42); EXPECT(o.key == 0xA1);
        EXPECT(o.server_id == 2); EXPECT(o.channel == 1);
        EXPECT(o.map_id == 50); EXPECT(o.mon_id == 0xDEAD);
        EXPECT(o.temp_mon_id == 9);
        // Item round-tripped through the cabinet codec.
        EXPECT(o.item.id == 0xABCDEF12);
        EXPECT(o.item.item_id_b == 7); EXPECT(o.item.item_id_w == 1234);
        EXPECT(o.item.level == 5); EXPECT(o.item.count == 3);
        EXPECT(o.item.magic.size() == 1);
        if (o.item.magic.size() == 1)
        { EXPECT(o.item.magic[0].first == 2);
          EXPECT(o.item.magic[0].second == 500); }
    }

    // --- Scenario 2: cursor advanced → Bob → item to p2 -------------
    SendFramed(p2, ToUint16(MessageId::MW_PARTYORDERTAKEITEM_ACK),
        OrderBody(200, 0xB0, kPid, all, item));
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_PARTYORDERTAKEITEM_REQ));
        auto o = DecodeOrderReq(b);
        EXPECT(o.char_id == 200); EXPECT(o.key == 0xB0);
    }

    // --- Scenario 3: only Carol eligible (cursor on Carol) → p3 -----
    SendFramed(p3, ToUint16(MessageId::MW_PARTYORDERTAKEITEM_ACK),
        OrderBody(400, 0xCA, kPid, std::vector<std::uint32_t>{400}, item));
    {
        auto [w, b] = ReadFramed(p3);
        EXPECT(w == ToUint16(MessageId::MW_PARTYORDERTAKEITEM_REQ));
        auto o = DecodeOrderReq(b);
        EXPECT(o.char_id == 400); EXPECT(o.key == 0xCA);
    }

    // --- Scenario 4: stale party id → MIT_NOTFOUND ------------------
    SendFramed(p1, ToUint16(MessageId::MW_PARTYORDERTAKEITEM_ACK),
        OrderBody(42, 0xA1, /*party=*/999, all, item));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_ADDITEMRESULT_REQ));
        auto a = DecodeAddResult(b);
        EXPECT(a.char_id == 42); EXPECT(a.channel == 1);
        EXPECT(a.map_id == 50); EXPECT(a.mon_id == 0xDEAD);
        EXPECT(a.item_id == 7);
        EXPECT(a.result == party::kMonItemTakeNotFound);
    }

    p1.close(); p2.close(); p3.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_party_order_handlers "
                    "(4 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_party_order_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
