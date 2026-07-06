// W6-40 wire tests: GM item tools.
//
// CT_ITEMFIND_REQ → repo search (LIKE name OR exact id) →
//   CT_ITEMFIND_ACK(count, manager_id, rows) to the *identified*
//   ctrl-svr slot; empty result still ACKs with count=0; no ctrl-svr
//   → silent drop.
// MW_ADDITEM_ACK → found char: verbatim 9-field forward to the main
//   map as MW_ADDITEM_REQ; missing char / key mismatch →
//   MW_ADDITEMRESULT_REQ(…, MIT_NOTFOUND) back to the reporter.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/fake_item_state_repository.h"
#include "../services/guild_registry.h"
#include "../services/party_constants.h"
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

std::vector<std::byte>
FindBody(std::uint32_t manager, std::uint16_t item_id,
         const std::string& pattern)
{
    namespace wire = tworldsvr::wire;
    std::vector<std::byte> b;
    wire::WritePOD<std::uint32_t>(b, manager);
    wire::WritePOD<std::uint16_t>(b, item_id);
    wire::WriteString(b, pattern);
    return b;
}

std::vector<std::byte>
AddItemBody(std::uint32_t char_id, std::uint32_t key)
{
    namespace wire = tworldsvr::wire;
    std::vector<std::byte> b;
    wire::WritePOD<std::uint32_t>(b, char_id);
    wire::WritePOD<std::uint32_t>(b, key);
    wire::WritePOD<std::uint8_t>(b, 0x66);      // reporting server id
    wire::WritePOD<std::uint8_t>(b, 2);         // channel
    wire::WritePOD<std::uint16_t>(b, 31);       // map_id
    wire::WritePOD<std::uint32_t>(b, 777);      // mon_id
    wire::WritePOD<std::uint8_t>(b, 1);         // inven
    wire::WritePOD<std::uint8_t>(b, 9);         // slot
    wire::WritePOD<std::uint8_t>(b, 42);        // item_id
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
    tworldsvr::CharRegistry            chars;
    tworldsvr::GuildRegistry           guilds;
    tworldsvr::PeerRegistry            peers;
    tworldsvr::CtrlSvrSlot             ctrl_svr;
    tworldsvr::FakeItemStateRepository item_repo;
    item_repo.AddRow(100, 1, "Sword of Dawn");
    item_repo.AddRow(200, 0, "Dawnbreaker Shield");
    item_repo.AddRow(300, 2, "Potion");

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.ctrl_svr = &ctrl_svr;
    ctx.item_state_repo = &item_repo;
    ctx.nation = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0; svr_cfg.max_connections = 4; svr_cfg.ctx = ctx;
    tworldsvr::WorldServer server(io, svr_cfg);
    const std::uint16_t port = server.Port();
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(20ms);

    boost::asio::io_context client_io;
    tcp::socket p1(client_io), p2(client_io), pc(client_io);
    const auto ep = tcp::endpoint(
        boost::asio::ip::make_address_v4("127.0.0.1"), port);
    p1.connect(ep); p2.connect(ep); pc.connect(ep);
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

    // --- W6-40a: ITEMFIND before ctrl-svr identifies → drop --------
    SendFramed(pc, ToUint16(MessageId::CT_ITEMFIND_REQ),
        FindBody(50, 100, ""));
    std::this_thread::sleep_for(30ms);

    SendFramed(pc, ToUint16(MessageId::CT_CTRLSVR_REQ), {});
    std::this_thread::sleep_for(20ms);
    EXPECT(ctrl_svr.Get() != nullptr);

    // --- W6-40b: name-pattern search -------------------------------
    // "%Dawn%" matches "Sword of Dawn" + "Dawnbreaker Shield";
    // item_id=0 matches nothing by id. The ACK is pc's FIRST frame —
    // proving W6-40a leaked nothing.
    SendFramed(pc, ToUint16(MessageId::CT_ITEMFIND_REQ),
        FindBody(51, 0, "%Dawn%"));
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_ITEMFIND_ACK));
        wire::Reader r(b);
        std::uint16_t count = 0; std::uint32_t manager = 0;
        r.Read(count); r.Read(manager);
        EXPECT(count == 2);
        EXPECT(manager == 51);
        std::uint16_t id = 0; std::uint8_t st = 0; std::string name;
        r.Read(id); r.Read(st); r.ReadString(name);
        EXPECT(id == 100 && st == 1 && name == "Sword of Dawn");
        r.Read(id); r.Read(st); r.ReadString(name);
        EXPECT(id == 200 && st == 0 && name == "Dawnbreaker Shield");
    }

    // --- W6-40c: exact-id search + empty-result ACK -----------------
    SendFramed(pc, ToUint16(MessageId::CT_ITEMFIND_REQ),
        FindBody(52, 300, "no-such-name"));
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_ITEMFIND_ACK));
        wire::Reader r(b);
        std::uint16_t count = 0; std::uint32_t manager = 0;
        r.Read(count); r.Read(manager);
        EXPECT(count == 1);
        EXPECT(manager == 52);
        std::uint16_t id = 0; std::uint8_t st = 0; std::string name;
        r.Read(id); r.Read(st); r.ReadString(name);
        EXPECT(id == 300 && st == 2 && name == "Potion");
    }
    SendFramed(pc, ToUint16(MessageId::CT_ITEMFIND_REQ),
        FindBody(53, 0, "zzz"));
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_ITEMFIND_ACK));
        wire::Reader r(b);
        std::uint16_t count = 0xFFFF; std::uint32_t manager = 0;
        r.Read(count); r.Read(manager);
        EXPECT(count == 0);          // empty result still ACKs
        EXPECT(manager == 53);
    }

    // --- W6-40d: ADDITEM route to main map --------------------------
    // Seed char 900 (key 0xC3) with main on 0x42 (p1). p2 reports the
    // grant; p1 must receive the *verbatim* 9-field payload as
    // MW_ADDITEM_REQ.
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 900);
        wire::WritePOD<std::uint32_t>(b, 0xC3);
        wire::WritePOD<std::uint32_t>(b, 0x7f000001);
        wire::WritePOD<std::uint16_t>(b, 33500);
        wire::WritePOD<std::uint32_t>(b, 700);
        SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK), b);
    }
    for (int i = 0; i < 1000 && !chars.Find(900); ++i)
        std::this_thread::sleep_for(10ms);

    const auto payload = AddItemBody(900, 0xC3);
    SendFramed(p2, ToUint16(MessageId::MW_ADDITEM_ACK), payload);
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_ADDITEM_REQ));
        EXPECT(b == payload);        // byte-for-byte forward
    }

    // --- W6-40e: unknown char / key mismatch → MIT_NOTFOUND ---------
    SendFramed(p2, ToUint16(MessageId::MW_ADDITEM_ACK),
        AddItemBody(900, 0xBAD));    // wrong key
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_ADDITEMRESULT_REQ));
        wire::Reader r(b);
        std::uint32_t char_id = 0, key = 0, mon_id = 0;
        std::uint8_t  channel = 0, item = 0, result = 0;
        std::uint16_t map_id = 0;
        r.Read(char_id); r.Read(key); r.Read(channel); r.Read(map_id);
        r.Read(mon_id); r.Read(item); r.Read(result);
        EXPECT(char_id == 900);
        EXPECT(key == 0xBAD);
        EXPECT(channel == 2);
        EXPECT(map_id == 31);
        EXPECT(mon_id == 777);
        EXPECT(item == 42);
        EXPECT(result == tworldsvr::party::kMonItemTakeNotFound);
    }

    p1.close(); p2.close(); pc.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_itemtools_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_itemtools_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
