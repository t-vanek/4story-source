// F2 wire-level test: peer side of the SERVICEMONITOR flow.
//
// Scenario:
//   1. Stand up a ControlServer with a FakeServiceInventory carrying
//      one machine + one service.
//   2. Operator logs in.
//   3. Stand up a loopback "peer" server (a plain TCP listener that
//      speaks the 8-byte CPacket frame) on the configured port.
//   4. Operator sends CT_NEWCONNECT_REQ targeting that service.
//   5. ControlServer dials, sends CT_CTRLSVR_REQ.
//   6. The peer answers with CT_SERVICEMONITOR_REQ.
//   7. The peer reads CT_SERVICEMONITOR_ACK (RTT echo) back from
//      control_svr.
//   8. The operator reads CT_SERVICEDATA_ACK (fan-out).
//
// Pure unit-style: no DB, no external services, all loopback.

#include "../control_server.h"
#include "../control_session.h"
#include "../peer_dialer.h"
#include "../senders.h"
#include "../wire_codec.h"
#include "../services/disabled_service_controller.h"
#include "../services/fake_operator_auth_service.h"
#include "../services/fake_service_inventory.h"
#include "../services/peer_registry.h"

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
#include <cstring>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

using tcontrolsvr::ControlServer;
using tcontrolsvr::ControlServerConfig;
using tcontrolsvr::ControlSession;
using tcontrolsvr::DisabledServiceController;
using tcontrolsvr::FakeOperatorAuthService;
using tcontrolsvr::FakeServiceInventory;
using tcontrolsvr::PeerDialer;
using tcontrolsvr::PeerRegistry;
using tcontrolsvr::PacketHeader;
using tcontrolsvr::ComputeChecksum;
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

void TestPeerMonitorFlow()
{
    // Stand up a fake peer "MapSvr" on a loopback port. Plain TCP —
    // we'll exchange CPacket frames with control_svr through this
    // socket once it dials in.
    boost::asio::io_context peer_io;
    boost::asio::ip::tcp::acceptor peer_acceptor(peer_io,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), 0));
    const std::uint16_t peer_port = peer_acceptor.local_endpoint().port();
    boost::asio::ip::tcp::socket peer_sock(peer_io);

    // Accept on a thread so it runs concurrently with the control
    // dialer.
    std::thread peer_listener([&] {
        boost::system::error_code ec;
        peer_acceptor.accept(peer_sock, ec);
    });

    // Set up the control server with the fake inventory pointing at
    // our peer listener.
    boost::asio::io_context io;

    FakeOperatorAuthService auth;
    auth.AddOperator("gm", "pw", 4);  // MANAGER_SERVICE

    FakeServiceInventory inv;
    inv.AddGroup({1, "World1"});
    inv.AddMachine({1, "host-a", 0,
                    /*public=*/{"127.0.0.1"},
                    /*private=*/{"127.0.0.1"}, ""});
    inv.AddType({2, 0, "MapSvr"});
    constexpr std::uint8_t kGroupId = 1, kTypeId = 2, kServerId = 1;
    const std::uint32_t kServiceId =
        (std::uint32_t(kGroupId) << 16) |
        (std::uint32_t(kTypeId)  <<  8) |
         std::uint32_t(kServerId);
    inv.AddService({kServiceId, kGroupId, kTypeId, kServerId,
                    /*machine_id=*/1, peer_port, "MapSvr1", "", ""});

    PeerRegistry peers(inv);
    DisabledServiceController controller;
    PeerDialer dialer(io, peers, inv);

    ControlServerConfig cfg{};
    cfg.port       = 0;
    cfg.auth       = &auth;
    cfg.inventory  = &inv;
    cfg.controller = &controller;
    cfg.dialer     = &dialer;
    cfg.peers      = &peers;
    cfg.auto_start = 0;
    ControlServer server(io, cfg);
    boost::asio::co_spawn(io, server.Run(), boost::asio::detached);

    std::thread t([&io] { io.run(); });

    // Operator side — log in.
    boost::asio::io_context op_io;
    boost::asio::ip::tcp::socket op_sock(op_io);
    op_sock.connect({boost::asio::ip::make_address_v4("127.0.0.1"),
                     server.Port()});

    std::vector<std::byte> login_body;
    tcontrolsvr::wire::WriteString(login_body, std::string("gm"));
    tcontrolsvr::wire::WriteString(login_body, std::string("pw"));
    SendFramed(op_sock, ToUint16(MessageId::CT_OPLOGIN_REQ), login_body);

    // Drain the 5-packet post-login chain.
    for (int i = 0; i < 5; ++i) RecvFramed(op_sock);

    // Ask control to dial the MapSvr1 peer.
    std::vector<std::byte> newconnect_body;
    tcontrolsvr::wire::WritePOD<std::uint32_t>(newconnect_body, kServiceId);
    SendFramed(op_sock, ToUint16(MessageId::CT_NEWCONNECT_REQ),
               newconnect_body);

    // Wait for the peer acceptor to take the inbound connection.
    peer_listener.join();
    peer_io.run_one();  // drain acceptor completion

    // Peer should receive CT_CTRLSVR_REQ next.
    auto ctrl_req = RecvFramed(peer_sock);
    EXPECT(ctrl_req.wId == ToUint16(MessageId::CT_CTRLSVR_REQ));
    EXPECT(ctrl_req.body.empty());

    // Peer pushes a CT_SERVICEMONITOR_REQ — tick=42, session=10,
    // user=5, active=3.
    {
        std::vector<std::byte> mon;
        tcontrolsvr::wire::WritePOD<std::uint32_t>(mon, 42);
        tcontrolsvr::wire::WritePOD<std::uint32_t>(mon, 10);
        tcontrolsvr::wire::WritePOD<std::uint32_t>(mon, 5);
        tcontrolsvr::wire::WritePOD<std::uint32_t>(mon, 3);
        SendFramed(peer_sock,
            ToUint16(MessageId::CT_SERVICEMONITOR_REQ), mon);
    }

    // Peer should read back CT_SERVICEMONITOR_ACK = tick (RTT echo).
    auto mon_ack = RecvFramed(peer_sock);
    EXPECT(mon_ack.wId == ToUint16(MessageId::CT_SERVICEMONITOR_ACK));
    EXPECT(mon_ack.body.size() == 4);
    std::uint32_t echoed_tick = 0;
    std::memcpy(&echoed_tick, mon_ack.body.data(), 4);
    EXPECT(echoed_tick == 42);

    // Operator should receive CT_SERVICEDATA_ACK fan-out.
    auto data_ack = RecvFramed(op_sock);
    EXPECT(data_ack.wId == ToUint16(MessageId::CT_SERVICEDATA_ACK));
    // Layout: dwID, dwSession, dwCurUser, dwMaxUser, dwPing,
    // INT64 nPickTime, dwStopCount, INT64 nLatestStop, dwActiveUser
    EXPECT(data_ack.body.size() == 4 * 7 + 8 * 2);  // 7*DWORD + 2*INT64
    std::uint32_t recv_id = 0, recv_session = 0, recv_user = 0;
    std::memcpy(&recv_id,      data_ack.body.data() +  0, 4);
    std::memcpy(&recv_session, data_ack.body.data() +  4, 4);
    std::memcpy(&recv_user,    data_ack.body.data() +  8, 4);
    EXPECT(recv_id      == kServiceId);
    EXPECT(recv_session == 10);
    EXPECT(recv_user    == 5);

    // CT_SERVICESTAT_REQ should now show MapSvr1 as Running.
    SendFramed(op_sock, ToUint16(MessageId::CT_SERVICESTAT_REQ), {});
    auto stat = RecvFramed(op_sock);
    EXPECT(stat.wId == ToUint16(MessageId::CT_SERVICESTAT_ACK));
    std::uint32_t count = 0;
    std::memcpy(&count, stat.body.data(), 4);
    EXPECT(count == 1);

    op_sock.close();
    peer_sock.close();

    io.stop();
    t.join();
}

} // namespace

int main()
{
    TestPeerMonitorFlow();
    if (g_fails)
    {
        std::fprintf(stderr, "%d failure(s)\n", g_fails);
        return 1;
    }
    std::printf("ok\n");
    return 0;
}
