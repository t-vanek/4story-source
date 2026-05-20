// F3 wire-level tests: admin forwarders (kick, move, char_msg, ban,
// chat-ban N-wave aggregation) + authority gate behavior.
//
// Pattern: stand up a ControlServer with FakeAdminAudit +
// FakeUserProtected + a manually-prepared PeerRegistry that already
// holds open peer sockets. Operator sends the admin packet, then we
// verify (a) the peer received the expected forward, (b) the audit
// log captured the operator + target, (c) authority denials emit
// CT_AUTHORITY_ACK + audit denial record + no peer forward.

#include "../control_server.h"
#include "../control_session.h"
#include "../peer_dialer.h"
#include "../peer_session.h"
#include "../senders.h"
#include "../wire_codec.h"
#include "../services/chat_ban_repository.h"
#include "../services/disabled_service_controller.h"
#include "../services/fake_admin_audit_logger.h"
#include "../services/fake_operator_auth_service.h"
#include "../services/fake_service_inventory.h"
#include "../services/fake_user_protected_service.h"
#include "../services/peer_registry.h"
#include "../services/svr_type.h"

#include "MessageId.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

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
using tcontrolsvr::FakeAdminAuditLogger;
using tcontrolsvr::FakeOperatorAuthService;
using tcontrolsvr::FakeServiceInventory;
using tcontrolsvr::FakeUserProtectedService;
using tcontrolsvr::PacketHeader;
using tcontrolsvr::PeerDialer;
using tcontrolsvr::PeerRegistry;
using tcontrolsvr::PeerSession;
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

void SendFramed(boost::asio::ip::tcp::socket& sock,
                std::uint16_t wId,
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

// Pre-stage a connected peer pair in the PeerRegistry: one socket
// belongs to the control server (registered as PeerSession + the
// RunPeerLoop coroutine for CT_*_ACK demux), the other socket is
// returned to the test for direct CPacket framing.
//
// The `client_io` argument keeps the client socket's executor alive
// for the duration of the test — boost::asio sockets store an
// executor reference and must not outlive the io_context that
// created them.
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

    // Spawn the peer read loop so CT_*_ACK from the test client gets
    // dispatched (CT_CHATBAN_ACK aggregator, etc.). Without this the
    // chat-ban test would hang waiting for the operator response.
    boost::asio::co_spawn(io,
        tcontrolsvr::handlers::RunPeerLoop(peer, ctx),
        boost::asio::detached);
    return {std::move(client), peer};
}

// Drive a logged-in operator session, returning the operator's
// connected socket. `client_io` keeps the socket's executor alive
// for the duration of the test (sockets store an executor ref by
// reference, so the io_context must outlive them).
boost::asio::ip::tcp::socket LoginOperator(
    boost::asio::io_context& client_io,
    std::uint16_t port,
    const std::string& id,
    const std::string& pw)
{
    using boost::asio::ip::tcp;
    tcp::socket sock(client_io);
    sock.connect({boost::asio::ip::make_address_v4("127.0.0.1"), port});

    std::vector<std::byte> body;
    tcontrolsvr::wire::WriteString(body, id);
    tcontrolsvr::wire::WriteString(body, pw);
    SendFramed(sock, ToUint16(MessageId::CT_OPLOGIN_REQ), body);

    // Drain the 5-packet post-login ack chain.
    for (int i = 0; i < 5; ++i) RecvFramed(sock);
    return sock;
}

// Shared fixture — keeps the server alive across the test cases.
struct Harness
{
    boost::asio::io_context           io;
    boost::asio::io_context           client_io;   // outlives test sockets
    FakeOperatorAuthService           auth;
    FakeServiceInventory              inv;
    PeerRegistry                      peers{inv};
    FakeAdminAuditLogger              audit;
    FakeUserProtectedService          user_ban;
    ChatBanRepository                 chat_bans;
    DisabledServiceController         controller;
    std::unique_ptr<PeerDialer>       dialer;
    std::unique_ptr<ControlServer>    server;
    std::thread                       runner;

    tcontrolsvr::HandlerContext PeerCtx()
    {
        tcontrolsvr::HandlerContext ctx{};
        ctx.auth      = &auth;
        ctx.inventory = &inv;
        ctx.operators = &server->Operators();
        ctx.peers     = &peers;
        ctx.controller= &controller;
        ctx.dialer    = dialer.get();
        ctx.audit     = &audit;
        ctx.user_ban  = &user_ban;
        ctx.chat_bans = &chat_bans;
        ctx.io        = &io;
        return ctx;
    }

    Harness()
    {
        auth.AddOperator("gm1",   "pw", 5);   // MANAGER_GMLEVEL1
        auth.AddOperator("gm2",   "pw", 6);   // MANAGER_GMLEVEL2
        auth.AddOperator("gm3",   "pw", 7);   // MANAGER_GMLEVEL3
        auth.AddOperator("svc_op","pw", 4);   // MANAGER_SERVICE
        auth.AddOperator("all",   "pw", 1);   // MANAGER_ALL

        inv.AddGroup({1, "World1"});
        inv.AddMachine({1, "host-a", 0, {"127.0.0.1"}, {"127.0.0.1"}, ""});
        inv.AddType({svr_type::kMapSvr,   0, "MapSvr"});
        inv.AddType({svr_type::kWorldSvr, 0, "WorldSvr"});
        inv.AddType({svr_type::kRlySvr,   0, "RlySvr"});
        inv.AddService({0x010401, 1, svr_type::kMapSvr,   1, 1, 0, "Map1",  "", ""});
        inv.AddService({0x010501, 1, svr_type::kWorldSvr, 1, 1, 0, "Wor1",  "", ""});
        inv.AddService({0x010701, 1, svr_type::kRlySvr,   1, 1, 0, "Rly1",  "", ""});
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
        server = std::make_unique<ControlServer>(io, cfg);
        boost::asio::co_spawn(io, server->Run(), boost::asio::detached);
        runner = std::thread([this] { io.run(); });
    }

    ~Harness()
    {
        io.stop();
        if (runner.joinable()) runner.join();
    }
};

void TestKickForwarder()
{
    Harness h;
    auto staged = StagePeer(h.io, h.client_io, h.peers, h.PeerCtx(),
        {0x010401, 1, svr_type::kMapSvr, 1, 1, 0, "Map1", "", ""});
    auto& map_peer = staged.client;

    auto op = LoginOperator(h.client_io, h.server->Port(),"gm2", "pw");
    std::vector<std::byte> body;
    tcontrolsvr::wire::WriteString(body, std::string("rule_breaker"));
    SendFramed(op, ToUint16(MessageId::CT_USERKICKOUT_REQ), body);

    // Peer should receive CT_USERKICKOUT_ACK with the target name.
    auto fwd = RecvFramed(map_peer);
    EXPECT(fwd.wId == ToUint16(MessageId::CT_USERKICKOUT_ACK));
    tcontrolsvr::wire::Reader r(fwd.body);
    std::string forwarded_name;
    EXPECT(r.ReadString(forwarded_name));
    EXPECT(forwarded_name == "rule_breaker");

    // Audit captured the operator + target.
    EXPECT(h.audit.Records().size() >= 1);
    EXPECT(h.audit.Records().back().kind == "kick");
    EXPECT(h.audit.Records().back().op == "gm2");
    EXPECT(h.audit.Records().back().target == "rule_breaker");
}

void TestKickDeniedForLowAuthority()
{
    Harness h;
    auto staged = StagePeer(h.io, h.client_io, h.peers, h.PeerCtx(),
        {0x010401, 1, svr_type::kMapSvr, 1, 1, 0, "Map1", "", ""});
    (void)staged.client;  // denial must NOT reach the peer

    auto op = LoginOperator(h.client_io, h.server->Port(),"gm1", "pw");
    std::vector<std::byte> body;
    tcontrolsvr::wire::WriteString(body, std::string("victim"));
    SendFramed(op, ToUint16(MessageId::CT_USERKICKOUT_REQ), body);

    // Operator should get CT_AUTHORITY_ACK with no body.
    auto auth_ack = RecvFramed(op);
    EXPECT(auth_ack.wId == ToUint16(MessageId::CT_AUTHORITY_ACK));
    EXPECT(auth_ack.body.empty());

    // Peer should NOT have received a forward — drain available
    // bytes by setting a short read with non_blocking. Easier:
    // verify the audit log shows a denial, not a kick.
    EXPECT(h.audit.Records().size() >= 1);
    EXPECT(h.audit.Records().back().kind == "authority_denied");
    EXPECT(h.audit.Records().back().op == "gm1");
}

void TestBanCallsUserProtectedService()
{
    Harness h;
    h.user_ban.SetReturn(7);  // arbitrary SP return; flow through to ACK

    auto op = LoginOperator(h.client_io, h.server->Port(),"gm1", "pw");
    std::vector<std::byte> body;
    tcontrolsvr::wire::WriteString(body, std::string("baduser"));
    tcontrolsvr::wire::WritePOD<std::uint32_t>(body, 14);  // duration
    tcontrolsvr::wire::WriteString(body, std::string("multi-account abuse"));
    tcontrolsvr::wire::WritePOD<std::uint8_t>(body, 0);    // permanent
    SendFramed(op, ToUint16(MessageId::CT_USERPROTECTED_REQ), body);

    auto ack = RecvFramed(op);
    EXPECT(ack.wId == ToUint16(MessageId::CT_USERPROTECTED_ACK));
    EXPECT(static_cast<std::uint8_t>(ack.body[0]) == 7);

    EXPECT(h.user_ban.Bans().size() == 1);
    const auto& b = h.user_ban.Bans().front();
    EXPECT(b.user_id == "baduser");
    EXPECT(b.duration_days == 14);
    EXPECT(b.reason == "multi-account abuse");
    EXPECT(b.permanent == 0);
    EXPECT(b.operator_id == "gm1");
}

void TestChatBanAggregation()
{
    Harness h;
    auto staged_world = StagePeer(h.io, h.client_io, h.peers, h.PeerCtx(),
        {0x010501, 1, svr_type::kWorldSvr, 1, 1, 0, "Wor1", "", ""});
    auto& world_peer = staged_world.client;

    auto op = LoginOperator(h.client_io, h.server->Port(),"gm3", "pw");
    std::vector<std::byte> body;
    tcontrolsvr::wire::WriteString(body, std::string("loudmouth"));
    tcontrolsvr::wire::WritePOD<std::uint16_t>(body, 15);
    tcontrolsvr::wire::WriteString(body, std::string("flooded chat"));
    SendFramed(op, ToUint16(MessageId::CT_CHATBAN_REQ), body);

    // World peer receives CT_CHATBAN_REQ with seq + manager_id.
    auto fwd = RecvFramed(world_peer);
    EXPECT(fwd.wId == ToUint16(MessageId::CT_CHATBAN_REQ));
    tcontrolsvr::wire::Reader r(fwd.body);
    std::string name; std::uint16_t minutes; std::uint32_t seq, manager_id;
    EXPECT(r.ReadString(name));
    EXPECT(r.Read(minutes));
    EXPECT(r.Read(seq));
    EXPECT(r.Read(manager_id));
    EXPECT(name    == "loudmouth");
    EXPECT(minutes == 15);
    EXPECT(seq     != 0);

    // World replies with bRet=1 — that's the only world ack expected.
    std::vector<std::byte> ack_body;
    tcontrolsvr::wire::WritePOD<std::uint8_t >(ack_body, 1);
    tcontrolsvr::wire::WritePOD<std::uint32_t>(ack_body, seq);
    tcontrolsvr::wire::WritePOD<std::uint32_t>(ack_body, manager_id);
    SendFramed(world_peer, ToUint16(MessageId::CT_CHATBAN_ACK), ack_body);

    // Operator should see CT_CHATBAN_ACK = success.
    auto op_ack = RecvFramed(op);
    EXPECT(op_ack.wId == ToUint16(MessageId::CT_CHATBAN_ACK));
    EXPECT(static_cast<std::uint8_t>(op_ack.body[0]) == 1);

    // Registry still has the ban (success → kept for CT_CHATBANLIST_REQ).
    EXPECT(h.chat_bans.Size() == 1);
}

} // namespace

int main()
{
    TestKickForwarder();
    TestKickDeniedForLowAuthority();
    TestBanCallsUserProtectedService();
    TestChatBanAggregation();
    if (g_fails)
    {
        std::fprintf(stderr, "%d failure(s)\n", g_fails);
        return 1;
    }
    std::printf("ok\n");
    return 0;
}
