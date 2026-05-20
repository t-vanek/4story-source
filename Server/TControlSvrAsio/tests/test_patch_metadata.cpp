// F5 unit test: patch metadata + castle handlers.
//
// Patch flow: CT_UPDATEPATCH_REQ → IPatchMetadataService.UpdatePatch
// called once per row. CT_PREVERSIONTABLE_REQ → operator gets the
// current TPREVERSION snapshot. CT_PREVERSIONUPDATE_REQ → three-pass
// batch (BetaToVersion, DeletePreVersion, UpdatePrePatch) + reply
// with the refreshed table.
//
// Castle flow tested indirectly through dispatch — the full peer-
// loop integration is covered by test_admin_forwarders and
// test_peer_monitor; here we verify CT_UPDATEPATCH ⇒ SP calls and
// CT_PREVERSIONTABLE ⇒ wire reply byte shape.

#include "../control_server.h"
#include "../peer_dialer.h"
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
using tcontrolsvr::DisabledServiceController;
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
using tcontrolsvr::SpdlogAlerter;
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

    Harness()
    {
        auth.AddOperator("ctl", "pw", 2);   // MANAGER_CONTROL
        auth.AddOperator("low", "pw", 5);   // MANAGER_GMLEVEL1
        inv.AddGroup({1, "World1"});
        inv.AddMachine({1, "host-a", 0, {"127.0.0.1"}, {"127.0.0.1"}, ""});
        inv.AddType({svr_type::kPatchSvr, 0, "PatchSvr"});
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

void TestUpdatePatchFiresSP()
{
    Harness h;
    auto op = h.Login("ctl", "pw");

    std::vector<std::byte> body;
    tcontrolsvr::wire::WritePOD<std::uint16_t>(body, 2);   // count
    tcontrolsvr::wire::WriteString(body, std::string("client/data"));
    tcontrolsvr::wire::WriteString(body, std::string("patch1.bin"));
    tcontrolsvr::wire::WritePOD<std::uint32_t>(body, 4096);
    tcontrolsvr::wire::WriteString(body, std::string("client/assets"));
    tcontrolsvr::wire::WriteString(body, std::string("ui.zip"));
    tcontrolsvr::wire::WritePOD<std::uint32_t>(body, 8192);
    SendFramed(op, ToUint16(MessageId::CT_UPDATEPATCH_REQ), body);

    // Give the io thread a chance to drain the dispatch.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const auto& calls = h.patch_meta.Calls();
    EXPECT(calls.size() == 2);
    EXPECT(calls[0].kind == "update_patch");
    EXPECT(calls[0].name == "patch1.bin");
    EXPECT(calls[0].size == 4096);
    EXPECT(calls[1].name == "ui.zip");
}

void TestPreVersionTableSnapshotReply()
{
    Harness h;
    h.patch_meta.Seed({101, "p", "n1", 100});
    h.patch_meta.Seed({102, "p", "n2", 200});

    auto op = h.Login("ctl", "pw");
    SendFramed(op, ToUint16(MessageId::CT_PREVERSIONTABLE_REQ), {});

    auto ack = RecvFramed(op);
    EXPECT(ack.wId == ToUint16(MessageId::CT_PREVERSIONTABLE_ACK));
    std::uint32_t count = 0;
    std::memcpy(&count, ack.body.data(), 4);
    EXPECT(count == 2);
}

void TestPreVersionUpdateThreePhase()
{
    Harness h;
    h.patch_meta.Seed({200, "p", "drop_me", 0});

    auto op = h.Login("ctl", "pw");
    std::vector<std::byte> body;
    // Phase 1: promote one beta (200 → released).
    tcontrolsvr::wire::WritePOD<std::uint16_t>(body, 1);
    tcontrolsvr::wire::WritePOD<std::uint32_t>(body, 200);
    // Phase 2: delete zero betas.
    tcontrolsvr::wire::WritePOD<std::uint16_t>(body, 0);
    // Phase 3: insert one new pre.
    tcontrolsvr::wire::WritePOD<std::uint16_t>(body, 1);
    tcontrolsvr::wire::WriteString(body, std::string("p/new"));
    tcontrolsvr::wire::WriteString(body, std::string("nightly.bin"));
    tcontrolsvr::wire::WritePOD<std::uint32_t>(body, 500);

    SendFramed(op, ToUint16(MessageId::CT_PREVERSIONUPDATE_REQ), body);
    auto ack = RecvFramed(op);
    EXPECT(ack.wId == ToUint16(MessageId::CT_PREVERSIONTABLE_ACK));

    // Verify SP call ordering.
    const auto& calls = h.patch_meta.Calls();
    EXPECT(calls.size() >= 2);
    EXPECT(calls[0].kind == "beta_to_version");
    EXPECT(calls[0].beta == 200);
    bool saw_insert = false;
    for (const auto& c : calls)
        if (c.kind == "update_pre_patch" && c.name == "nightly.bin")
            saw_insert = true;
    EXPECT(saw_insert);
}

void TestUpdatePatchDeniedForLowAuthority()
{
    Harness h;
    auto op = h.Login("low", "pw");
    std::vector<std::byte> body;
    tcontrolsvr::wire::WritePOD<std::uint16_t>(body, 1);
    tcontrolsvr::wire::WriteString(body, std::string("p"));
    tcontrolsvr::wire::WriteString(body, std::string("n"));
    tcontrolsvr::wire::WritePOD<std::uint32_t>(body, 1);
    SendFramed(op, ToUint16(MessageId::CT_UPDATEPATCH_REQ), body);

    auto ack = RecvFramed(op);
    EXPECT(ack.wId == ToUint16(MessageId::CT_AUTHORITY_ACK));
    EXPECT(h.patch_meta.Calls().empty());
}

} // namespace

int main()
{
    TestUpdatePatchFiresSP();
    TestPreVersionTableSnapshotReply();
    TestPreVersionUpdateThreePhase();
    TestUpdatePatchDeniedForLowAuthority();
    if (g_fails) { std::fprintf(stderr, "%d failure(s)\n", g_fails); return 1; }
    std::printf("ok\n");
    return 0;
}
