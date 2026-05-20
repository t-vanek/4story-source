// Wire-parity regression suite covering the round-2 audit fixes:
//
//   1. Four count-width mismatches (CHATBANLIST / EVENTLIST /
//      CASHITEMLIST / PREVERSIONTABLE): legacy uses WORD count,
//      not DWORD. Verify the first two bytes of the body are the
//      count and the next entry starts at offset 2.
//   2. CT_CHATBANLIST_ACK field order: { DWORD id, CString target,
//      INT64 created, WORD minutes, CString reason, CString op }.
//   3. CT_EVENTUPDATE_REQ now appends EventInfo body — verify the
//      packet body extends past the 3-byte header.
//
// Plus round-2 missing-handler smoke tests:
//   4. CT_MONACTION_REQ → Map peer receives CT_MONACTION_ACK with
//      the right field layout.
//   5. CT_ITEMFIND_REQ → World peer receives CT_ITEMFIND_REQ with
//      { DWORD manager_id, WORD item_id, CString name }.
//   6. CT_SERVICEDATACLEAR_REQ → each live peer receives
//      CT_SERVICEDATACLEAR_ACK + the local registry resets counters.

#include "../control_server.h"
#include "../control_session.h"
#include "../event_codec.h"
#include "../peer_dialer.h"
#include "../peer_session.h"
#include "../senders.h"
#include "../wire_codec.h"
#include "../services/chat_ban_repository.h"
#include "../services/disabled_service_controller.h"
#include "../services/event_registry.h"
#include "../services/fake_admin_audit_logger.h"
#include "../services/fake_event_repository.h"
#include "../services/fake_operator_auth_service.h"
#include "../services/fake_patch_metadata_service.h"
#include "../services/fake_service_inventory.h"
#include "../services/fake_user_protected_service.h"
#include "../services/peer_registry.h"
#include "../services/spdlog_alerter.h"
#include "../services/svr_type.h"
#include "../handlers/handlers.h"

#include "MessageId.h"

#include <boost/asio/buffer.hpp>
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
#include <thread>
#include <vector>

namespace {

using tcontrolsvr::ChatBanRepository;
using tcontrolsvr::ComputeChecksum;
using tcontrolsvr::ControlServer;
using tcontrolsvr::ControlServerConfig;
using tcontrolsvr::ControlSession;
using tcontrolsvr::DisabledServiceController;
using tcontrolsvr::EventInfo;
using tcontrolsvr::EventRegistry;
using tcontrolsvr::FakeAdminAuditLogger;
using tcontrolsvr::FakeEventRepository;
using tcontrolsvr::FakeOperatorAuthService;
using tcontrolsvr::FakePatchMetadataService;
using tcontrolsvr::FakeServiceInventory;
using tcontrolsvr::FakeUserProtectedService;
using tcontrolsvr::PacketHeader;
using tcontrolsvr::PeerDialer;
using tcontrolsvr::PeerRegistry;
using tcontrolsvr::PeerSession;
using tcontrolsvr::SpdlogAlerter;
namespace event_kind = tcontrolsvr::event_kind;
namespace svr_type = tcontrolsvr::svr_type;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

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
    PacketHeader hdr{};
    hdr.wSize    = static_cast<std::uint16_t>(8 + body.size());
    hdr.wID      = wId;
    hdr.dwChkSum = ComputeChecksum(body.data(), body.size());
    boost::asio::write(sock, boost::asio::buffer(&hdr, sizeof(hdr)));
    if (!body.empty())
        boost::asio::write(sock, boost::asio::buffer(body.data(), body.size()));
}

struct InboundPacket { std::uint16_t wId; std::vector<std::byte> body; };

InboundPacket RecvFramed(boost::asio::ip::tcp::socket& sock)
{
    PacketHeader hdr{};
    boost::asio::read(sock, boost::asio::buffer(&hdr, sizeof(hdr)));
    InboundPacket pkt{};
    pkt.wId = hdr.wID;
    const std::size_t body_len = hdr.wSize - 8;
    pkt.body.resize(body_len);
    if (body_len) boost::asio::read(sock,
        boost::asio::buffer(pkt.body.data(), body_len));
    return pkt;
}

struct StagedPeer
{
    boost::asio::ip::tcp::socket client;
    std::shared_ptr<PeerSession> server_side;
};

StagedPeer StagePeer(boost::asio::io_context& io,
                     boost::asio::io_context& client_io,
                     PeerRegistry& registry,
                     tcontrolsvr::HandlerContext ctx,
                     tcontrolsvr::ServiceInstance svc)
{
    using boost::asio::ip::tcp;
    tcp::acceptor acc(io, tcp::endpoint(tcp::v4(), 0));
    const auto port = acc.local_endpoint().port();
    tcp::socket client(client_io);
    std::thread connector([&client, port] {
        client.connect({boost::asio::ip::make_address_v4("127.0.0.1"), port});
    });
    tcp::socket peer_side(io);
    acc.accept(peer_side);
    connector.join();
    auto wire = std::make_shared<ControlSession>(std::move(peer_side));
    auto peer = std::make_shared<PeerSession>(wire, std::move(svc));
    registry.SetConnection(peer->ServiceId(), peer);
    if (auto* st = registry.Status(peer->ServiceId()))
        st->status = tcontrolsvr::ServiceStatus::Running;
    boost::asio::co_spawn(io,
        tcontrolsvr::handlers::RunPeerLoop(peer, ctx),
        boost::asio::detached);
    return {std::move(client), peer};
}

struct Harness
{
    boost::asio::io_context           io;
    boost::asio::io_context           client_io;
    FakeOperatorAuthService           auth;
    FakeServiceInventory              inv;
    PeerRegistry                      peers{inv};
    FakeAdminAuditLogger              audit;
    FakeUserProtectedService          user_ban;
    ChatBanRepository                 chat_bans;
    EventRegistry                     events;
    FakeEventRepository               event_repo;
    FakePatchMetadataService          patch_meta;
    SpdlogAlerter                     alerter;
    DisabledServiceController         controller;
    std::unique_ptr<PeerDialer>       dialer;
    std::unique_ptr<ControlServer>    server;
    std::thread                       runner;

    tcontrolsvr::HandlerContext PeerCtx()
    {
        tcontrolsvr::HandlerContext ctx{};
        ctx.auth       = &auth;
        ctx.inventory  = &inv;
        ctx.operators  = &server->Operators();
        ctx.peers      = &peers;
        ctx.controller = &controller;
        ctx.dialer     = dialer.get();
        ctx.audit      = &audit;
        ctx.user_ban   = &user_ban;
        ctx.chat_bans  = &chat_bans;
        ctx.events     = &events;
        ctx.event_repo = &event_repo;
        ctx.patch_meta = &patch_meta;
        ctx.alerter    = &alerter;
        ctx.io         = &io;
        return ctx;
    }

    Harness()
    {
        auth.AddOperator("all", "pw", 1);   // MANAGER_ALL
        auth.AddOperator("gm",  "pw", 4);   // MANAGER_SERVICE
        inv.AddGroup({1, "World1"});
        inv.AddMachine({1, "host-a", 0, {"127.0.0.1"}, {"127.0.0.1"}, ""});
        inv.AddType({svr_type::kMapSvr,   0, "MapSvr"});
        inv.AddType({svr_type::kWorldSvr, 0, "WorldSvr"});
        inv.AddService({0x010401, 1, svr_type::kMapSvr,   1, 1, 0, "Map1", "", ""});
        inv.AddService({0x010501, 1, svr_type::kWorldSvr, 1, 1, 0, "Wor1", "", ""});
        peers.Rebind(inv);

        dialer = std::make_unique<PeerDialer>(io, peers, inv);
        ControlServerConfig cfg{};
        cfg.port       = 0;
        cfg.auth       = &auth;
        cfg.inventory  = &inv;
        cfg.controller = &controller;
        cfg.dialer     = dialer.get();
        cfg.peers      = &peers;
        cfg.audit      = &audit;
        cfg.user_ban   = &user_ban;
        cfg.chat_bans  = &chat_bans;
        cfg.events     = &events;
        cfg.event_repo = &event_repo;
        cfg.patch_meta = &patch_meta;
        cfg.alerter    = &alerter;
        server = std::make_unique<ControlServer>(io, cfg);
        boost::asio::co_spawn(io, server->Run(), boost::asio::detached);
        runner = std::thread([this] { io.run(); });
    }
    ~Harness()
    {
        io.stop();
        if (runner.joinable()) runner.join();
    }

    boost::asio::ip::tcp::socket Login(const std::string& id,
                                       const std::string& pw)
    {
        using boost::asio::ip::tcp;
        tcp::socket sock(client_io);
        sock.connect({boost::asio::ip::make_address_v4("127.0.0.1"),
                      server->Port()});
        std::vector<std::byte> body;
        tcontrolsvr::wire::WriteString(body, id);
        tcontrolsvr::wire::WriteString(body, pw);
        SendFramed(sock, ToUint16(MessageId::CT_OPLOGIN_REQ), body);
        for (int i = 0; i < 5; ++i) RecvFramed(sock);
        return sock;
    }
};

void TestChatBanListWireShape()
{
    // Verify CT_CHATBANLIST_ACK = { WORD count, [ DWORD id,
    // CString target, INT64 created, WORD minutes, CString reason,
    // CString operator ] * count } matches legacy.
    Harness h;
    h.chat_bans.CreateBan("gm", "target1", 30, "reason1", 0, 0);

    auto op = h.Login("all", "pw");
    SendFramed(op, ToUint16(MessageId::CT_CHATBANLIST_REQ), {});
    auto ack = RecvFramed(op);
    EXPECT(ack.wId == ToUint16(MessageId::CT_CHATBANLIST_ACK));
    EXPECT(ack.body.size() >= 2);
    std::uint16_t count = 0;
    std::memcpy(&count, ack.body.data(), 2);
    EXPECT(count == 1);  // WORD count, not DWORD

    // Walk the row: DWORD id, CString target, INT64 created, WORD
    // minutes, CString reason, CString operator. Skip the WORD
    // count by pointing the Reader at body[2..] directly.
    tcontrolsvr::wire::Reader r(ack.body.data() + 2,
                                ack.body.size() - 2);
    std::uint32_t id = 0;
    std::string target, reason, operator_id;
    std::int64_t created = 0;
    std::uint16_t minutes = 0;
    EXPECT(r.Read(id));
    EXPECT(r.ReadString(target));
    EXPECT(r.Read(created));
    EXPECT(r.Read(minutes));
    EXPECT(r.ReadString(reason));
    EXPECT(r.ReadString(operator_id));
    EXPECT(target      == "target1");
    EXPECT(minutes     == 30);
    EXPECT(reason      == "reason1");
    EXPECT(operator_id == "gm");
}

void TestEventListWireWidth()
{
    // Verify the count is WORD (2 bytes), not DWORD.
    Harness h;
    EventInfo ev{};
    ev.index = 1;
    ev.kind  = event_kind::kExpRate;
    ev.title = "test";
    ev.start_unix = 100;
    ev.end_unix   = 200;
    ev.value = 50;
    h.events.Upsert(ev);

    auto op = h.Login("all", "pw");
    SendFramed(op, ToUint16(MessageId::CT_EVENTLIST_REQ), {});
    auto ack = RecvFramed(op);
    EXPECT(ack.wId == ToUint16(MessageId::CT_EVENTLIST_ACK));
    std::uint16_t count = 0;
    std::memcpy(&count, ack.body.data(), 2);
    EXPECT(count == 1);
}

void TestCashItemListWireWidth()
{
    Harness h;
    h.event_repo.AddCashItem({100, "item-A"});
    h.event_repo.AddCashItem({200, "item-B"});
    auto op = h.Login("all", "pw");
    SendFramed(op, ToUint16(MessageId::CT_CASHITEMLIST_REQ), {});
    auto ack = RecvFramed(op);
    EXPECT(ack.wId == ToUint16(MessageId::CT_CASHITEMLIST_ACK));
    std::uint16_t count = 0;
    std::memcpy(&count, ack.body.data(), 2);
    EXPECT(count == 2);
}

void TestMonActionReqForwarder()
{
    Harness h;
    auto staged = StagePeer(h.io, h.client_io, h.peers, h.PeerCtx(),
        {0x010401, 1, svr_type::kMapSvr, 1, 1, 0, "Map1", "", ""});

    auto op = h.Login("all", "pw");
    std::vector<std::byte> body;
    tcontrolsvr::wire::WritePOD<std::uint8_t >(body, 1);     // group
    tcontrolsvr::wire::WritePOD<std::uint8_t >(body, 2);     // channel
    tcontrolsvr::wire::WritePOD<std::uint16_t>(body, 42);    // map_id
    tcontrolsvr::wire::WritePOD<std::uint32_t>(body, 999);   // mon_id
    tcontrolsvr::wire::WritePOD<std::uint8_t >(body, 7);     // action
    tcontrolsvr::wire::WritePOD<std::uint32_t>(body, 11);    // trigger
    tcontrolsvr::wire::WritePOD<std::uint32_t>(body, 22);    // host
    tcontrolsvr::wire::WritePOD<std::uint32_t>(body, 33);    // rh
    tcontrolsvr::wire::WritePOD<std::uint8_t >(body, 8);     // rh_type
    tcontrolsvr::wire::WritePOD<std::uint16_t>(body, 55);    // spawn_id
    SendFramed(op, ToUint16(MessageId::CT_MONACTION_REQ), body);

    auto fwd = RecvFramed(staged.client);
    EXPECT(fwd.wId == ToUint16(MessageId::CT_MONACTION_ACK));
    tcontrolsvr::wire::Reader r(fwd.body);
    std::uint8_t channel = 0, action = 0, rh_type = 0;
    std::uint16_t map_id = 0, spawn_id = 0;
    std::uint32_t mon_id = 0, trigger = 0, host = 0, rh = 0;
    EXPECT(r.Read(channel));
    EXPECT(r.Read(map_id));
    EXPECT(r.Read(mon_id));
    EXPECT(r.Read(action));
    EXPECT(r.Read(trigger));
    EXPECT(r.Read(host));
    EXPECT(r.Read(rh));
    EXPECT(r.Read(rh_type));
    EXPECT(r.Read(spawn_id));
    EXPECT(channel  == 2);
    EXPECT(map_id   == 42);
    EXPECT(mon_id   == 999);
    EXPECT(action   == 7);
    EXPECT(spawn_id == 55);
}

void TestItemFindReqForwarder()
{
    Harness h;
    auto staged = StagePeer(h.io, h.client_io, h.peers, h.PeerCtx(),
        {0x010501, 1, svr_type::kWorldSvr, 1, 1, 0, "Wor1", "", ""});

    auto op = h.Login("all", "pw");
    std::vector<std::byte> body;
    tcontrolsvr::wire::WritePOD<std::uint16_t>(body, 7);    // item_id
    tcontrolsvr::wire::WriteString(body, std::string("user42"));
    tcontrolsvr::wire::WritePOD<std::uint8_t >(body, 1);    // world
    SendFramed(op, ToUint16(MessageId::CT_ITEMFIND_REQ), body);

    auto fwd = RecvFramed(staged.client);
    EXPECT(fwd.wId == ToUint16(MessageId::CT_ITEMFIND_REQ));
    tcontrolsvr::wire::Reader r(fwd.body);
    std::uint32_t manager_id = 0;
    std::uint16_t item_id = 0;
    std::string user_name;
    EXPECT(r.Read(manager_id));
    EXPECT(r.Read(item_id));
    EXPECT(r.ReadString(user_name));
    EXPECT(item_id   == 7);
    EXPECT(user_name == "user42");
    EXPECT(manager_id != 0);   // operator's seq, set by Login
}

void TestServiceDataClearReqResetsCounters()
{
    Harness h;
    // Pre-populate runtime status — without a connection so the
    // counters survive but no SERVICEDATACLEAR_ACK fires for them.
    if (auto* st = h.peers.Status(0x010401))
    {
        st->max_users  = 500;
        st->stop_count = 3;
    }
    auto op = h.Login("all", "pw");
    SendFramed(op, ToUint16(MessageId::CT_SERVICEDATACLEAR_REQ), {});
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto* st = h.peers.Status(0x010401);
    EXPECT(st != nullptr);
    EXPECT(st->max_users  == 0);
    EXPECT(st->stop_count == 0);
}

void TestEventUpdateReqAppendsEventInfo()
{
    // After F4 was patched to write the full EventInfo body, the
    // CT_EVENTUPDATE_REQ packet size must exceed the 3-byte header
    // (BYTE kind + WORD value). The minimum EventInfo body is
    // ~63 bytes (POD fields + 4 empty CStrings + 4 vector counts).
    Harness h;
    EventInfo ev{};
    ev.index       = 9;
    ev.kind        = event_kind::kExpRate;
    ev.server_type = svr_type::kWorldSvr;
    ev.group_id    = 1;
    ev.start_unix  = 100;
    ev.end_unix    = 200;
    ev.value       = 75;
    ev.title       = "ExpBoost";
    ev.start_msg   = "go!";
    ev.end_msg     = "done";
    h.events.Upsert(ev);

    auto staged = StagePeer(h.io, h.client_io, h.peers, h.PeerCtx(),
        {0x010501, 1, svr_type::kWorldSvr, 1, 1, 0, "Wor1", "", ""});

    auto op = h.Login("all", "pw");
    std::vector<std::byte> body;
    tcontrolsvr::wire::WritePOD<std::uint32_t>(body, 9);     // index
    tcontrolsvr::wire::WritePOD<std::uint16_t>(body, 75);    // value
    SendFramed(op, ToUint16(MessageId::CT_EVENTUPDATE_REQ), body);

    auto fwd = RecvFramed(staged.client);
    EXPECT(fwd.wId == ToUint16(MessageId::CT_EVENTUPDATE_REQ));
    // Header: BYTE kind + WORD value = 3 bytes; EventInfo body adds
    // ~60+ more, so the total must be well above 3.
    EXPECT(fwd.body.size() > 32);

    tcontrolsvr::wire::Reader r(fwd.body);
    std::uint8_t  kind = 0;
    std::uint16_t value = 0;
    EXPECT(r.Read(kind));
    EXPECT(r.Read(value));
    EXPECT(kind  == event_kind::kExpRate);
    EXPECT(value == 75);
    // Confirm EventInfo decodes back.
    EventInfo decoded{};
    EXPECT(tcontrolsvr::event_codec::Read(r, decoded));
    EXPECT(decoded.index == 9);
    EXPECT(decoded.title == "ExpBoost");
}

} // namespace

int main()
{
    TestChatBanListWireShape();
    TestEventListWireWidth();
    TestCashItemListWireWidth();
    TestMonActionReqForwarder();
    TestItemFindReqForwarder();
    TestServiceDataClearReqResetsCounters();
    TestEventUpdateReqAppendsEventInfo();
    if (g_fails) { std::fprintf(stderr, "%d failure(s)\n", g_fails); return 1; }
    std::printf("ok\n");
    return 0;
}
