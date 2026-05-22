// PeerDialer — outbound TLS CN/SAN enforcement against the security
// gate. Mirror of the inbound test_register_cn_validation, but on
// the dial side (PR #49 added the transport but only logged the CN;
// the code review flagged this as a real gap closed in this PR).
//
// Scenarios:
//   1. Gate has an opinion for (group, server) AND the cert CN
//      matches → Dial returns a populated PeerDialResult.
//   2. Gate has an opinion AND the cert CN doesn't match → Dial
//      returns res.session == nullptr with a "peer identity
//      mismatch" message in failure_reason.
//   3. Gate has no opinion → Dial succeeds (preserves PR #49
//      behaviour for un-constrained slots).

#include "../peer_dialer.h"
#include "../control_session.h"
#include "../services/fake_service_inventory.h"
#include "../services/peer_registry.h"

#include "fourstory/security/peer_security_gate.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>

#ifndef TEST_CERT_DIR
#error "TEST_CERT_DIR must point at the tnetlib test cert fixtures"
#endif

namespace {

namespace asio = boost::asio;
using boost::asio::ip::tcp;
using tcontrolsvr::ControlSession;
using tcontrolsvr::FakeServiceInventory;
using tcontrolsvr::PeerDialer;
using tcontrolsvr::PeerRegistry;

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

// One dial-and-return run. Spins up a TLS listener (presenting
// server.crt with CN="tnetlib-test-server") on an ephemeral port,
// configures the dialer with the given gate, and runs Dial() against
// a synthetic ServiceInstance pointing at the listener.
struct DialOutcome
{
    bool        session_published;
    std::string failure_reason;
};

DialOutcome RunDial(asio::ssl::context& server_ctx,
                     fourstory::security::PeerSecurityGate* gate,
                     std::uint8_t group_id,
                     std::uint8_t server_id,
                     std::uint8_t type_id)
{
    asio::io_context io;

    // TLS-capable listener that accepts one connection and parks.
    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 0));
    const std::uint16_t port = acceptor.local_endpoint().port();
    asio::co_spawn(io,
        [&]() -> asio::awaitable<void> {
            boost::system::error_code ec;
            auto sock = co_await acceptor.async_accept(
                asio::redirect_error(asio::use_awaitable, ec));
            if (ec) co_return;
            asio::ssl::stream<tcp::socket> stream(std::move(sock),
                                                   server_ctx);
            co_await stream.async_handshake(
                asio::ssl::stream_base::server,
                asio::redirect_error(asio::use_awaitable, ec));
            // Park — let the client drive the dial flow. We don't
            // read or write anything; the test only cares about the
            // dialer's post-handshake decision.
            (void)ec;
        },
        asio::detached);

    // Inventory contains exactly one machine + one service mapped
    // at the loopback listener.
    FakeServiceInventory inv;
    tcontrolsvr::Machine machine{};
    machine.id            = 1;
    machine.private_addrs = {"127.0.0.1"};
    inv.AddMachine(machine);
    tcontrolsvr::ServiceInstance svc{};
    svc.service_id = (static_cast<std::uint32_t>(group_id) << 16) |
                     (static_cast<std::uint32_t>(type_id)  <<  8) |
                      static_cast<std::uint32_t>(server_id);
    svc.group_id   = group_id;
    svc.type_id    = type_id;
    svc.server_id  = server_id;
    svc.machine_id = 1;
    svc.port       = port;
    svc.name       = "test-target";
    inv.AddService(svc);

    PeerRegistry peers(inv);
    PeerDialer dialer(io, peers, inv, std::chrono::seconds(2));
    auto client_ctx = MakeClientCtx();
    dialer.SetTlsContext(&client_ctx);
    dialer.SetSecurityGate(gate);

    DialOutcome out{};
    asio::co_spawn(io,
        [&]() -> asio::awaitable<void> {
            auto r = co_await dialer.Dial(svc);
            out.session_published = (r.session != nullptr);
            out.failure_reason    = std::move(r.failure_reason);
        },
        asio::detached);

    io.run();   // drains everything (listener parked, dial done)
    return out;
}

void TestDialAcceptsMatchingIdentity()
{
    std::printf("[dial accepts when gate's expected name matches cert]\n");
    fourstory::security::SecurityConfig cfg;
    fourstory::security::PeerSecurityGate gate(cfg, nullptr);
    // Server cert CN is "tnetlib-test-server". Map an arbitrary
    // identity tuple (g=1, s=2, t=3) to that name in the trust map.
    gate.InjectTrustForTest(1, 2, 3, "tnetlib-test-server");

    auto server_ctx = MakeServerCtx();
    auto out = RunDial(server_ctx, &gate, 1, 2, 3);
    Check(out.session_published, "dialer published the PeerSession on match");
    Check(out.failure_reason.empty(), "no failure_reason on match");
}

void TestDialRejectsMismatchedIdentity()
{
    std::printf("[dial rejects when gate's expected name doesn't match]\n");
    fourstory::security::SecurityConfig cfg;
    fourstory::security::PeerSecurityGate gate(cfg, nullptr);
    // Expected != actual cert CN; cert presents tnetlib-test-server.
    gate.InjectTrustForTest(1, 2, 3, "some-other-peer");

    auto server_ctx = MakeServerCtx();
    auto out = RunDial(server_ctx, &gate, 1, 2, 3);
    Check(!out.session_published,
          "dialer did NOT publish PeerSession on identity mismatch");
    Check(out.failure_reason.find("identity mismatch") != std::string::npos,
          "failure_reason names the identity-mismatch path");
    Check(out.failure_reason.find("tnetlib-test-server") != std::string::npos,
          "failure_reason reports captured CN");
    Check(out.failure_reason.find("some-other-peer") != std::string::npos,
          "failure_reason reports expected peer name");
}

void TestDialAcceptsWhenGateHasNoOpinion()
{
    std::printf("[dial accepts when gate has no trust entry]\n");
    fourstory::security::SecurityConfig cfg;
    fourstory::security::PeerSecurityGate gate(cfg, nullptr);
    // No InjectTrustForTest — gate's trust map is empty for this
    // identity. Pre-PR behaviour (log-only CN sanity) should hold.

    auto server_ctx = MakeServerCtx();
    auto out = RunDial(server_ctx, &gate, 1, 2, 3);
    Check(out.session_published,
          "dialer published PeerSession when gate has no opinion");
}

} // namespace

int main()
{
    std::printf("=== PeerDialer outbound CN/SAN enforcement ===\n");
    try
    {
        TestDialAcceptsMatchingIdentity();
        TestDialRejectsMismatchedIdentity();
        TestDialAcceptsWhenGateHasNoOpinion();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception thrown: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
