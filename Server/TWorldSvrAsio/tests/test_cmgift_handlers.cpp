// W6-45 wire tests: CMGift family.
//
//   CT_CMGIFT_REQ (tool=1) direct gift → MW_CMGIFT_REQ delivery on
//     the target's main map (all 11 fields byte-checked).
//   Unknown gift → CT_CMGIFT_ACK(CMGIFT_ID) to the ctrl-svr.
//   MW_CMGIFT_ACK (tool=0) take-type gift with a DUPLICATE CanTake
//     verdict → err-gift swap → delivery as CMGIFT_ERRPOST with
//     (requested=20, actual=10).
//   Offline target → MW_CMGIFTRESULT_REQ(CMGIFT_TARGET) to the GM's
//     main map.
//   CT_CMGIFTLIST_REQ → catalogue snapshot to the ctrl-svr.
//   CT_CMGIFTCHARTUPDATE_REQ → ADD adopts the repo-assigned id,
//     DEL drops; refreshed catalogue ack.

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/cmgift_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/fake_cmgift_repository.h"
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
GiftReqBody(const std::string& target, std::uint16_t gift,
            std::uint32_t manager)
{
    namespace wire = tworldsvr::wire;
    std::vector<std::byte> b;
    wire::WriteString(b, target);
    wire::WritePOD<std::uint16_t>(b, gift);
    wire::WritePOD<std::uint32_t>(b, manager);
    return b;
}

void AddChar(boost::asio::ip::tcp::socket& s, std::uint32_t id,
             std::uint32_t key, std::uint32_t user)
{
    namespace wire = tworldsvr::wire;
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    std::vector<std::byte> b;
    wire::WritePOD<std::uint32_t>(b, id);
    wire::WritePOD<std::uint32_t>(b, key);
    wire::WritePOD<std::uint32_t>(b, 0x7f000001);
    wire::WritePOD<std::uint16_t>(b, 33500);
    wire::WritePOD<std::uint32_t>(b, user);
    SendFramed(s, ToUint16(MessageId::MW_ADDCHAR_ACK), b);
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
    tworldsvr::CharRegistry         chars;
    tworldsvr::GuildRegistry        guilds;
    tworldsvr::PeerRegistry         peers;
    tworldsvr::CtrlSvrSlot          ctrl_svr;
    tworldsvr::CmGiftRegistry       cmgifts;
    tworldsvr::FakeCmGiftRepository gift_repo;

    // Gift 10: direct (take_type=0). Gift 20: take-type with
    // err_gift_id=10. Gift 30: tool-only.
    cmgifts.LoadFrom({
        {10, 1, 500, 2, 0, 1, 0, 0, "Starter", "Enjoy"},
        {20, 2, 900, 1, 1, 1, 0, 10, "Event", "Grats"},
        {30, 3, 100, 1, 0, 1, 1, 0, "ToolOnly", "Ops"},
    });

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers; ctx.ctrl_svr = &ctrl_svr;
    ctx.cmgifts = &cmgifts;
    ctx.cmgift_repo = &gift_repo;
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
    SendFramed(pc, ToUint16(MessageId::CT_CTRLSVR_REQ), {});
    std::this_thread::sleep_for(20ms);
    EXPECT(ctrl_svr.Get() != nullptr);

    // Alice (200) on p1/0x42; GM (300) on p2/0x43. Names via the
    // registry name index.
    AddChar(p1, 200, 0xA1, 500);
    AddChar(p2, 300, 0xB2, 501);
    for (int i = 0; i < 1000 && (!chars.Find(200) || !chars.Find(300));
         ++i)
        std::this_thread::sleep_for(10ms);
    chars.Rename(200, "Alice");
    chars.Rename(300, "Gimli");

    // --- W6-45a: operator direct gift → delivery -------------------
    SendFramed(pc, ToUint16(MessageId::CT_CMGIFT_REQ),
        GiftReqBody("Alice", 10, 77));
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CMGIFT_REQ));
        wire::Reader r(b);
        std::uint32_t target = 0, value = 0, gm = 0;
        std::uint8_t type = 0, cnt = 0, tool = 0, ret = 0xFF;
        std::uint16_t req_id = 0, act_id = 0;
        std::string title, msg;
        r.Read(target); r.Read(type); r.Read(value); r.Read(cnt);
        r.ReadString(title); r.ReadString(msg); r.Read(gm);
        r.Read(tool); r.Read(req_id); r.Read(act_id); r.Read(ret);
        EXPECT(target == 200);
        EXPECT(type == 1);
        EXPECT(value == 500);
        EXPECT(cnt == 2);
        EXPECT(title == "Starter");
        EXPECT(msg == "Enjoy");
        EXPECT(gm == 77);
        EXPECT(tool == 1);
        EXPECT(req_id == 10 && act_id == 10);
        EXPECT(ret == tworldsvr::kCmGiftSuccess);
    }

    // --- W6-45b: unknown gift → CMGIFT_ID to ctrl -------------------
    SendFramed(pc, ToUint16(MessageId::CT_CMGIFT_REQ),
        GiftReqBody("Alice", 99, 77));
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_CMGIFT_ACK));
        wire::Reader r(b);
        std::uint8_t ret = 0xFF; std::uint32_t mgr = 0;
        r.Read(ret); r.Read(mgr);
        EXPECT(ret == tworldsvr::kCmGiftId);
        EXPECT(mgr == 77);
    }

    // --- W6-45c: take-type + DUPLICATE → err-gift as ERRPOST -------
    gift_repo.SetCanTake("Alice", tworldsvr::kCmGiftDuplicate);
    {
        std::vector<std::byte> b;
        wire::WriteString(b, "Alice");
        wire::WritePOD<std::uint16_t>(b, 20);
        wire::WritePOD<std::uint32_t>(b, 300);   // GM char
        SendFramed(p2, ToUint16(MessageId::MW_CMGIFT_ACK), b);
    }
    {
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_CMGIFT_REQ));
        wire::Reader r(b);
        std::uint32_t target = 0, value = 0, gm = 0;
        std::uint8_t type = 0, cnt = 0, tool = 0, ret = 0xFF;
        std::uint16_t req_id = 0, act_id = 0;
        std::string title, msg;
        r.Read(target); r.Read(type); r.Read(value); r.Read(cnt);
        r.ReadString(title); r.ReadString(msg); r.Read(gm);
        r.Read(tool); r.Read(req_id); r.Read(act_id); r.Read(ret);
        EXPECT(target == 200);
        EXPECT(title == "Starter");     // the err gift (10) payload
        EXPECT(gm == 300);
        EXPECT(tool == 0);
        EXPECT(req_id == 20);
        EXPECT(act_id == 10);
        EXPECT(ret == tworldsvr::kCmGiftErrPost);
        const auto ct = gift_repo.CanTakeCalls();
        EXPECT(ct.size() == 1);
        if (!ct.empty())
        {
            EXPECT(ct[0].first == "Alice");
            EXPECT(ct[0].second == 20);
        }
    }

    // --- W6-45d: offline target → CMGIFT_TARGET to the GM ----------
    {
        std::vector<std::byte> b;
        wire::WriteString(b, "Nobody");
        wire::WritePOD<std::uint16_t>(b, 10);
        wire::WritePOD<std::uint32_t>(b, 300);
        SendFramed(p2, ToUint16(MessageId::MW_CMGIFT_ACK), b);
    }
    {
        auto [w, b] = ReadFramed(p2);   // GM main = 0x43 = p2
        EXPECT(w == ToUint16(MessageId::MW_CMGIFTRESULT_REQ));
        wire::Reader r(b);
        std::uint8_t ret = 0xFF; std::uint32_t gm = 0;
        r.Read(ret); r.Read(gm);
        EXPECT(ret == tworldsvr::kCmGiftTarget);
        EXPECT(gm == 300);
    }

    // --- W6-45e: catalogue list -------------------------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 88);
        SendFramed(pc, ToUint16(MessageId::CT_CMGIFTLIST_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_CMGIFTLIST_ACK));
        wire::Reader r(b);
        std::uint32_t mgr = 0; std::uint16_t count = 0;
        r.Read(mgr); r.Read(count);
        EXPECT(mgr == 88);
        EXPECT(count == 3);
        std::uint16_t id = 0; std::uint8_t ty = 0;
        r.Read(id); r.Read(ty);
        EXPECT(id == 10 && ty == 1);
    }

    // --- W6-45f: chart update — ADD adopts repo id, DEL drops -------
    gift_repo.SetNextAddId(40);
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint16_t>(b, 2);          // entry count
        wire::WritePOD<std::uint8_t>(b, tworldsvr::kCguAdd);
        wire::WritePOD<std::uint16_t>(b, 0);          // id (assigned)
        wire::WritePOD<std::uint8_t>(b, 4);           // type
        wire::WritePOD<std::uint32_t>(b, 250);        // value
        wire::WritePOD<std::uint8_t>(b, 1);           // count
        wire::WritePOD<std::uint8_t>(b, 0);           // take_type
        wire::WritePOD<std::uint8_t>(b, 1);           // max_take
        wire::WritePOD<std::uint8_t>(b, 0);           // tool_only
        wire::WritePOD<std::uint16_t>(b, 0);          // err_id
        wire::WriteString(b, "Fresh");
        wire::WriteString(b, "New gift");
        wire::WritePOD<std::uint8_t>(b, tworldsvr::kCguDel);
        wire::WritePOD<std::uint16_t>(b, 30);
        wire::WritePOD<std::uint32_t>(b, 99);         // manager
        SendFramed(pc, ToUint16(MessageId::CT_CMGIFTCHARTUPDATE_REQ), b);
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_CMGIFTLIST_ACK));
        wire::Reader r(b);
        std::uint32_t mgr = 0; std::uint16_t count = 0;
        r.Read(mgr); r.Read(count);
        EXPECT(mgr == 99);
        EXPECT(count == 3);              // 10, 20, 40 (30 deleted)
    }
    EXPECT(cmgifts.Get(40).has_value());
    EXPECT(!cmgifts.Get(30).has_value());
    if (auto g = cmgifts.Get(40))
        EXPECT(g->title == "Fresh");
    EXPECT(gift_repo.AddCalls().size() == 1);
    EXPECT(gift_repo.DelCalls().size() == 1);

    p1.close(); p2.close(); pc.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_cmgift_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_cmgift_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
