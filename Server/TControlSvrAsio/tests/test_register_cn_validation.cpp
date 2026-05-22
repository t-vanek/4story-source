// CT_PEER_REGISTER_REQ — Common Name validation against TPEER_AUTH.
//
// When a peer connects over TLS, the cert's CN is captured into
// ControlSession::PeerCommonName() at construction. The register
// handler compares it against the operator-configured peer_name in
// PeerSecurityGate's trust map (loaded from TPEER_AUTH). A mismatch
// is treated as identity fraud and the registration is rejected with
// reason_code = kRejectCnMismatch (3).
//
// This test pairs the project's test fixtures (CN=tnetlib-test-client)
// with InjectTrustForTest so the handler chain runs without a DB.
//
// Cases:
//   1. CN matches expected peer_name → REGISTER_ACK accepted=1
//   2. CN differs from expected peer_name → REGISTER_ACK accepted=0,
//      reason_code=3 (kRejectCnMismatch)
//   3. No trust entry for the identity → handler skips the CN check,
//      registration proceeds (operator hasn't constrained the slot).
//
// Out of scope:
//   * Heartbeat / deregister round-trip (covered by test_peer_client)
//   * Plain-TCP path with peer_tls_enabled = false (covered by the
//     hybrid test)

#include "../control_server.h"
#include "../control_session.h"
#include "../senders.h"
#include "../wire_codec.h"
#include "../services/fake_operator_auth_service.h"
#include "../services/fake_service_inventory.h"
#include "../services/peer_registry.h"

#include "fourstory/security/peer_security_gate.h"

#include "MessageId.h"

#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/write.hpp>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifndef TEST_CERT_DIR
#error "TEST_CERT_DIR must point at the tnetlib test cert fixtures"
#endif

namespace {

namespace asio = boost::asio;
using boost::asio::ip::tcp;
using tcontrolsvr::ControlServer;
using tcontrolsvr::ControlServerConfig;
using tcontrolsvr::FakeOperatorAuthService;
using tcontrolsvr::FakeServiceInventory;
using tcontrolsvr::PeerRegistry;
using tcontrolsvr::PacketHeader;
using tcontrolsvr::ComputeChecksum;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

int g_passed = 0;
int g_failed = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

std::string CertPath(const char* file)
{
    return std::string(TEST_CERT_DIR) + "/" + file;
}

asio::ssl::context MakeServerCtx()
{
    asio::ssl::context ctx(asio::ssl::context::tls_server);
    ctx.set_options(
        asio::ssl::context::default_workarounds |
        asio::ssl::context::no_sslv2 |
        asio::ssl::context::no_sslv3 |
        asio::ssl::context::single_dh_use);
    ctx.use_certificate_file(CertPath("server.crt"),
                              asio::ssl::context::pem);
    ctx.use_private_key_file(CertPath("server.key"),
                              asio::ssl::context::pem);
    ctx.load_verify_file(CertPath("ca.crt"));
    ctx.set_verify_mode(asio::ssl::verify_peer |
                        asio::ssl::verify_fail_if_no_peer_cert);
    return ctx;
}

asio::ssl::context MakeClientCtx()
{
    asio::ssl::context ctx(asio::ssl::context::tls_client);
    ctx.set_options(
        asio::ssl::context::default_workarounds |
        asio::ssl::context::no_sslv2 |
        asio::ssl::context::no_sslv3);
    ctx.use_certificate_file(CertPath("client.crt"),
                              asio::ssl::context::pem);
    ctx.use_private_key_file(CertPath("client.key"),
                              asio::ssl::context::pem);
    ctx.load_verify_file(CertPath("ca.crt"));
    ctx.set_verify_mode(asio::ssl::verify_peer);
    return ctx;
}

// Encode a CT_PEER_REGISTER_REQ body. Same layout as PeerClient's
// outbound; the helper here is intentionally hand-coded so the test
// doesn't accidentally couple to PeerClient's framing.
std::vector<std::byte>
BuildRegisterBody(std::uint32_t service_id,
                  const std::string& name,
                  const std::string& addr,
                  std::uint16_t port,
                  const std::string& version,
                  std::uint32_t pid,
                  std::uint64_t start_unix)
{
    std::vector<std::byte> b;
    tcontrolsvr::wire::WritePOD<std::uint32_t>(b, service_id);
    tcontrolsvr::wire::WriteString(b, name);
    tcontrolsvr::wire::WriteString(b, addr);
    tcontrolsvr::wire::WritePOD<std::uint16_t>(b, port);
    tcontrolsvr::wire::WriteString(b, version);
    tcontrolsvr::wire::WritePOD<std::uint32_t>(b, pid);
    tcontrolsvr::wire::WritePOD<std::uint64_t>(b, start_unix);
    return b;
}

template <class Stream>
void WriteFramed(Stream& s, std::uint16_t wId,
                  const std::vector<std::byte>& body)
{
    PacketHeader hdr{};
    hdr.wSize    = static_cast<std::uint16_t>(8 + body.size());
    hdr.wID      = wId;
    hdr.dwChkSum = ComputeChecksum(body.data(), body.size());
    asio::write(s, asio::buffer(&hdr, sizeof(hdr)));
    if (!body.empty())
        asio::write(s, asio::buffer(body.data(), body.size()));
}

struct Pkt {
    std::uint16_t wId = 0;
    std::vector<std::byte> body;
};

template <class Stream>
Pkt ReadFramed(Stream& s)
{
    PacketHeader hdr{};
    asio::read(s, asio::buffer(&hdr, sizeof(hdr)));
    Pkt p;
    p.wId = hdr.wID;
    const std::size_t body_len = hdr.wSize - sizeof(hdr);
    p.body.resize(body_len);
    if (body_len) asio::read(s, asio::buffer(p.body.data(), body_len));
    return p;
}

// Extract the (accepted, reason_code) tuple from a REGISTER_ACK body.
// Wire shape mirrors handlers_registry.cpp:
//   BYTE accepted, DWORD reason_code, QWORD lease_epoch, DWORD heartbeat_sec
struct RegisterAck { std::uint8_t accepted; std::uint32_t reason; };
RegisterAck ParseRegisterAck(const std::vector<std::byte>& body)
{
    RegisterAck a{};
    if (body.size() < 1 + 4) return a;
    a.accepted = static_cast<std::uint8_t>(body[0]);
    std::memcpy(&a.reason, body.data() + 1, 4);
    return a;
}

// One TLS register attempt against a freshly-built ControlServer with
// the given gate. Returns the parsed ack so the caller can assert on
// accepted/reason. Identity bytes are picked to match the inventory
// row + the gate's injected trust entry.
RegisterAck DriveTlsRegister(asio::ssl::context& server_ctx,
                              fourstory::security::PeerSecurityGate* gate,
                              std::uint32_t service_id)
{
    asio::io_context io;

    FakeOperatorAuthService auth;
    FakeServiceInventory inv;
    tcontrolsvr::ServiceInstance svc{};
    svc.service_id = service_id;
    svc.group_id   = static_cast<std::uint8_t>((service_id >> 16) & 0xFF);
    svc.type_id    = static_cast<std::uint8_t>((service_id >>  8) & 0xFF);
    svc.server_id  = static_cast<std::uint8_t>(service_id & 0xFF);
    inv.AddService(svc);
    PeerRegistry peers(inv);

    ControlServerConfig svr_cfg{};
    svr_cfg.port      = 0;
    svr_cfg.auth      = &auth;
    svr_cfg.inventory = &inv;
    svr_cfg.peers     = &peers;
    svr_cfg.security  = gate;
    svr_cfg.ssl_ctx   = &server_ctx;
    ControlServer server(io, svr_cfg);
    asio::co_spawn(io, server.Run(), asio::detached);
    std::thread t([&io] { io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    asio::io_context client_io;
    auto client_ssl_ctx = MakeClientCtx();
    asio::ssl::stream<tcp::socket> stream(client_io, client_ssl_ctx);
    stream.next_layer().connect(
        tcp::endpoint(asio::ip::address_v4::loopback(), server.Port()));
    stream.handshake(asio::ssl::stream_base::client);

    auto body = BuildRegisterBody(service_id, "test-peer",
                                   "127.0.0.1", 4000, "5.0.0-test",
                                   /*pid=*/1234, /*start_unix=*/1700000000);
    WriteFramed(stream, ToUint16(MessageId::CT_PEER_REGISTER_REQ), body);
    auto ack_pkt = ReadFramed(stream);
    auto ack = ParseRegisterAck(ack_pkt.body);

    boost::system::error_code ig;
    stream.next_layer().close(ig);
    io.stop();
    t.join();
    return ack;
}

void TestCnMatchAccepts()
{
    std::printf("[CN matches expected peer_name → register accepted]\n");
    fourstory::security::SecurityConfig cfg;
    fourstory::security::PeerSecurityGate gate(cfg, nullptr);

    // Injected trust entry: identity (group=1, server=2, type=3) maps
    // to peer_name "tnetlib-test-client" — exactly the CN baked into
    // the test cert fixtures. Service_id below encodes the same triple.
    gate.InjectTrustForTest(/*group=*/1, /*server=*/2, /*type=*/3,
                             "tnetlib-test-client");

    auto server_ctx = MakeServerCtx();
    const std::uint32_t service_id = (1u << 16) | (3u << 8) | 2u;
    auto ack = DriveTlsRegister(server_ctx, &gate, service_id);
    Check(ack.accepted == 1, "REGISTER_ACK accepted=1 on CN match");
    Check(ack.reason == 0, "REGISTER_ACK reason=0 on CN match");
}

void TestCnMismatchRejects()
{
    std::printf("[CN differs from expected peer_name → register rejected]\n");
    fourstory::security::SecurityConfig cfg;
    fourstory::security::PeerSecurityGate gate(cfg, nullptr);

    // Trust entry claims the slot belongs to a different peer name;
    // the cert's CN ("tnetlib-test-client") will not match.
    gate.InjectTrustForTest(/*group=*/1, /*server=*/2, /*type=*/3,
                             "completely-different-peer");

    auto server_ctx = MakeServerCtx();
    const std::uint32_t service_id = (1u << 16) | (3u << 8) | 2u;
    auto ack = DriveTlsRegister(server_ctx, &gate, service_id);
    Check(ack.accepted == 0, "REGISTER_ACK accepted=0 on CN mismatch");
    // reason 3 = kRejectCnMismatch
    Check(ack.reason == 3, "REGISTER_ACK reason=3 (kRejectCnMismatch)");
}

void TestNoTrustEntrySkipsCheck()
{
    std::printf("[no trust entry → CN check skipped, register accepted]\n");
    fourstory::security::SecurityConfig cfg;
    fourstory::security::PeerSecurityGate gate(cfg, nullptr);
    // No InjectTrustForTest call — gate's trust map is empty.

    auto server_ctx = MakeServerCtx();
    const std::uint32_t service_id = (1u << 16) | (3u << 8) | 2u;
    auto ack = DriveTlsRegister(server_ctx, &gate, service_id);
    Check(ack.accepted == 1,
          "REGISTER_ACK accepted=1 when gate has no opinion");
}

} // namespace

int main()
{
    std::printf("=== CT_PEER_REGISTER_REQ CN validation ===\n");
    try
    {
        TestCnMatchAccepts();
        TestCnMismatchRejects();
        TestNoTrustEntrySkipsCheck();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception thrown: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
