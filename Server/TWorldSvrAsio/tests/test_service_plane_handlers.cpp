// W6-38 wire tests: service / control plane.
//
//   CT_SERVICEMONITOR_ACK  → CT_SERVICEMONITOR_REQ echo with live
//                            counters on the same socket
//   CT_SERVICEDATACLEAR_ACK → active-user set rebuilt from chars
//   CT_HELPMESSAGE_REQ     → MW_HELPMESSAGE_REQ broadcast + repo save
//   SM_QUITSERVICE_REQ     → log-only stub (sentinel-verified)
//   SM_DELSESSION_REQ      → map teardown: chars with a con on the
//                            departing map are CloseChar()d (DELCHAR
//                            fan-out to their other cons), the
//                            TCURRENTUSER clear fires, the sender's
//                            socket is force-closed; non-map senders
//                            only get the socket close

#include "../handlers/handlers.h"
#include "../services/char_registry.h"
#include "../services/ctrl_svr_slot.h"
#include "../services/fake_service_ops_repository.h"
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

// MW_ADDCHAR_ACK body (char joins the sender's map).
std::vector<std::byte>
AddCharBody(std::uint32_t char_id, std::uint32_t key, std::uint32_t user_id)
{
    std::vector<std::byte> b;
    tworldsvr::wire::WritePOD(b, char_id);
    tworldsvr::wire::WritePOD(b, key);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, 0x7f000001);
    tworldsvr::wire::WritePOD<std::uint16_t>(b, 33500);
    tworldsvr::wire::WritePOD<std::uint32_t>(b, user_id);
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
    tworldsvr::FakeServiceOpsRepository ops_repo;

    tworldsvr::HandlerContext ctx{};
    ctx.io = &io; ctx.chars = &chars; ctx.guilds = &guilds;
    ctx.peers = &peers;
    ctx.ctrl_svr = &ctrl_svr;
    ctx.service_ops = &ops_repo;
    ctx.group_id = 7;
    ctx.nation = 0;

    tworldsvr::WorldServerConfig svr_cfg{};
    svr_cfg.port = 0; svr_cfg.max_connections = 8; svr_cfg.ctx = ctx;
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

    // Map peers register with SVRGRP_MAPSVR (4) in HIBYTE — the
    // DELSESSION gate keys on it. pc identifies as the ctrl-svr.
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

    // Seed chars: A(100) on maps 0x42+0x43 (user 500), B(200) on
    // 0x43 only (user 501).
    SendFramed(p1, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(100, 0xA1, 500));
    for (int i = 0; i < 1000 && !chars.Find(100); ++i)
        std::this_thread::sleep_for(10ms);
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(100, 0xA1, 500));
    SendFramed(p2, ToUint16(MessageId::MW_ADDCHAR_ACK),
        AddCharBody(200, 0xB2, 501));
    for (int i = 0; i < 1000 && !chars.Find(200); ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(chars.Size() == 2);
    EXPECT(chars.ActiveUserCount() == 2);

    // --- W6-38a: SERVICEMONITOR echo -------------------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint32_t>(b, 0xDEAD0042);
        SendFramed(pc, ToUint16(MessageId::CT_SERVICEMONITOR_ACK), b);
    }
    {
        auto [w, b] = ReadFramed(pc);
        EXPECT(w == ToUint16(MessageId::CT_SERVICEMONITOR_REQ));
        wire::Reader r(b);
        std::uint32_t tick = 0, sess = 0, ch = 0, users = 0;
        r.Read(tick); r.Read(sess); r.Read(ch); r.Read(users);
        EXPECT(tick == 0xDEAD0042);
        EXPECT(sess == 3);    // 2 map peers + ctrl slot
        EXPECT(ch == 2);
        EXPECT(users == 2);
    }

    // --- W6-38b: SERVICEDATACLEAR rebuild --------------------------
    // Pollute the active-user set with a stale id; the resync must
    // drop it and keep exactly the users derived from live chars.
    chars.MarkUserActive(9999);
    EXPECT(chars.ActiveUserCount() == 3);
    SendFramed(pc, ToUint16(MessageId::CT_SERVICEDATACLEAR_ACK), {});
    for (int i = 0; i < 1000 && chars.ActiveUserCount() != 2; ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(chars.ActiveUserCount() == 2);
    EXPECT(chars.IsUserActive(500));
    EXPECT(chars.IsUserActive(501));
    EXPECT(!chars.IsUserActive(9999));

    // --- W6-38c: HELPMESSAGE broadcast + persist --------------------
    {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, 3);
        wire::WritePOD<std::int64_t>(b, 1750000000);
        wire::WritePOD<std::int64_t>(b, 1760000000);
        wire::WriteString(b, "server maintenance at dawn");
        SendFramed(pc, ToUint16(MessageId::CT_HELPMESSAGE_REQ), b);
    }
    for (auto* s : {&p1, &p2})
    {
        auto [w, b] = ReadFramed(*s);
        EXPECT(w == ToUint16(MessageId::MW_HELPMESSAGE_REQ));
        wire::Reader r(b);
        std::uint8_t id = 0; std::int64_t st = 0, en = 0; std::string msg;
        r.Read(id); r.Read(st); r.Read(en); r.ReadString(msg);
        EXPECT(id == 3);
        EXPECT(st == 1750000000);
        EXPECT(en == 1760000000);
        EXPECT(msg == "server maintenance at dawn");
    }
    for (int i = 0; i < 1000 && ops_repo.HelpMessages().empty(); ++i)
        std::this_thread::sleep_for(10ms);
    {
        const auto hm = ops_repo.HelpMessages();
        EXPECT(hm.size() == 1);
        if (!hm.empty())
        {
            EXPECT(hm[0].id == 3);
            EXPECT(hm[0].start_unix == 1750000000);
            EXPECT(hm[0].end_unix == 1760000000);
            EXPECT(hm[0].message == "server maintenance at dawn");
        }
    }

    // --- W6-38d: QUITSERVICE is a stub ------------------------------
    SendFramed(pc, ToUint16(MessageId::SM_QUITSERVICE_REQ), {});
    std::this_thread::sleep_for(30ms);
    EXPECT(chars.Size() == 2);          // nothing torn down

    // --- W6-38e: DELSESSION map teardown ----------------------------
    // p1 (0x0442) goes away: char A (con on 0x42+0x43) must be
    // CloseChar()d — p2 sees MW_DELCHAR_REQ for A; char B (0x43 only)
    // survives; the TCURRENTUSER clear fires with (group=7, 0x42, 4);
    // p1's socket is force-closed by the server.
    SendFramed(p1, ToUint16(MessageId::SM_DELSESSION_REQ), {});
    {
        auto [w, b] = ReadFramed(p2);
        EXPECT(w == ToUint16(MessageId::MW_DELCHAR_REQ));
        wire::Reader r(b);
        std::uint32_t char_id = 0;
        r.Read(char_id);
        EXPECT(char_id == 100);
    }
    {   // p1 (the departing map itself) also holds a con for char A,
        // so CloseChar sends it a DELCHAR too — before the forced
        // socket close.
        auto [w, b] = ReadFramed(p1);
        EXPECT(w == ToUint16(MessageId::MW_DELCHAR_REQ));
        wire::Reader r(b);
        std::uint32_t char_id = 0;
        r.Read(char_id);
        EXPECT(char_id == 100);
    }
    for (int i = 0; i < 1000 && chars.Find(100); ++i)
        std::this_thread::sleep_for(10ms);
    EXPECT(chars.Find(100) == nullptr);
    EXPECT(chars.Find(200) != nullptr);
    EXPECT(chars.Size() == 1);
    for (int i = 0; i < 1000 && ops_repo.ClearCalls().empty(); ++i)
        std::this_thread::sleep_for(10ms);
    {
        const auto cc = ops_repo.ClearCalls();
        EXPECT(cc.size() == 1);
        if (!cc.empty())
        {
            EXPECT(cc[0].group_id == 7);
            EXPECT(cc[0].server_id == 0x42);
            EXPECT(cc[0].svr_type == 4);
        }
    }
    {   // …then the server force-closes p1.
        boost::system::error_code ec;
        tworldsvr::PacketHeader hdr{};
        boost::asio::read(p1, boost::asio::buffer(&hdr, sizeof(hdr)), ec);
        EXPECT(static_cast<bool>(ec));   // EOF / reset
    }

    // --- W6-38f: DELSESSION from a non-map sender --------------------
    // pc (ctrl socket, wID=0) sends DELSESSION: no sweep, no clear —
    // but its socket still gets closed (legacy runs COMP_CLOSE
    // unconditionally).
    SendFramed(pc, ToUint16(MessageId::SM_DELSESSION_REQ), {});
    std::this_thread::sleep_for(30ms);
    EXPECT(chars.Size() == 1);                    // B untouched
    EXPECT(ops_repo.ClearCalls().size() == 1);    // no second clear
    {
        boost::system::error_code ec;
        tworldsvr::PacketHeader hdr{};
        boost::asio::read(pc, boost::asio::buffer(&hdr, sizeof(hdr)), ec);
        EXPECT(static_cast<bool>(ec));
    }

    p2.close();
    io.stop();
    io_thread.join();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_service_plane_handlers\n");
    else
        std::printf("FAIL test_tworldsvr_asio_service_plane_handlers "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
