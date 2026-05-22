// Hybrid first-byte detection — verifies the ControlServer accept
// loop routes TLS and plain TCP clients correctly on the SAME
// listener when an ssl_ctx is configured.
//
// Scenario:
//   1. ControlServer started with [security].peer_tls_enabled = true
//      (ssl_ctx non-null), FakeOperatorAuthService + FakeServiceInventory
//      wired so CT_OPLOGIN_REQ goes end-to-end through the handler chain.
//   2. Plain TCP client opens a connection and runs the legacy
//      CT_OPLOGIN_REQ flow. Expected: server peeks the first byte,
//      sees it's not 0x16, takes the plain code path, the operator
//      login succeeds (backward compat with TController.exe).
//   3. TLS client (mutual TLS via the project test certs) opens a
//      connection. Expected: server peeks 0x16, runs the TLS handshake,
//      then the same CT_OPLOGIN_REQ flow succeeds.
//
// Asserts that both clients receive a valid CT_OPLOGIN_ACK with the
// expected authority byte, proving the hybrid path lands at the same
// dispatcher regardless of transport.

#include "../control_server.h"
#include "../control_session.h"
#include "../senders.h"
#include "../wire_codec.h"
#include "../services/fake_operator_auth_service.h"
#include "../services/fake_service_inventory.h"

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
using tcontrolsvr::PacketHeader;
using tcontrolsvr::ComputeChecksum;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

int g_fails = 0;
int g_passed = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_fails;  std::printf("  FAIL  %s\n", label); }
}

std::string CertPath(const char* file)
{
    return std::string(TEST_CERT_DIR) + "/" + file;
}

std::vector<std::byte> BuildOpLoginBody(const std::string& id,
                                        const std::string& pw)
{
    std::vector<std::byte> b;
    tcontrolsvr::wire::WriteString(b, id);
    tcontrolsvr::wire::WriteString(b, pw);
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

void TestHybridAcceptTlsAndPlainOnSameListener()
{
    std::printf("[ControlServer hybrid TLS + plain on same listener]\n");

    asio::io_context io;
    auto server_ssl_ctx = MakeServerCtx();

    FakeOperatorAuthService auth;
    auth.AddOperator("gm_alpha", "secret", 3);
    FakeServiceInventory inv;

    ControlServerConfig svr_cfg{};
    svr_cfg.port       = 0;
    svr_cfg.auth       = &auth;
    svr_cfg.inventory  = &inv;
    svr_cfg.ssl_ctx    = &server_ssl_ctx;   // <-- hybrid mode

    ControlServer server(io, svr_cfg);
    asio::co_spawn(io, server.Run(), asio::detached);
    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // ── Plain TCP client (legacy operator path) ─────────────────────
    {
        asio::io_context client_io;
        tcp::socket sock(client_io);
        sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(),
                                    server.Port()));
        Check(sock.is_open(), "plain client connected");

        WriteFramed(sock, ToUint16(MessageId::CT_OPLOGIN_REQ),
                    BuildOpLoginBody("gm_alpha", "secret"));
        auto ack = ReadFramed(sock);
        Check(ack.wId == ToUint16(MessageId::CT_OPLOGIN_ACK),
              "plain client received CT_OPLOGIN_ACK");
        Check(ack.body.size() >= 2 &&
              static_cast<std::uint8_t>(ack.body[0]) == 0,
              "plain client login succeeded (bRet=0)");
        Check(ack.body.size() >= 2 &&
              static_cast<std::uint8_t>(ack.body[1]) == 3,
              "plain client got authority=3");

        sock.close();
    }

    // ── TLS client (modern peer / Phase A path) ─────────────────────
    {
        asio::io_context client_io;
        auto client_ssl_ctx = MakeClientCtx();
        asio::ssl::stream<tcp::socket> stream(client_io, client_ssl_ctx);
        stream.next_layer().connect(
            tcp::endpoint(asio::ip::address_v4::loopback(), server.Port()));
        Check(stream.next_layer().is_open(), "TLS client connected");

        boost::system::error_code hs_ec;
        stream.handshake(asio::ssl::stream_base::client, hs_ec);
        Check(!hs_ec, "TLS client handshake succeeded");

        WriteFramed(stream, ToUint16(MessageId::CT_OPLOGIN_REQ),
                    BuildOpLoginBody("gm_alpha", "secret"));
        auto ack = ReadFramed(stream);
        Check(ack.wId == ToUint16(MessageId::CT_OPLOGIN_ACK),
              "TLS client received CT_OPLOGIN_ACK");
        Check(ack.body.size() >= 2 &&
              static_cast<std::uint8_t>(ack.body[0]) == 0,
              "TLS client login succeeded (bRet=0)");
        Check(ack.body.size() >= 2 &&
              static_cast<std::uint8_t>(ack.body[1]) == 3,
              "TLS client got authority=3");

        boost::system::error_code ig;
        stream.next_layer().close(ig);
    }

    io.stop();
    if (io_thread.joinable()) io_thread.join();
}

} // namespace

int main()
{
    std::printf("=== ControlServer hybrid TLS/plain tests ===\n");
    try
    {
        TestHybridAcceptTlsAndPlainOnSameListener();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception thrown: %s\n", ex.what());
        ++g_fails;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_fails);
    return g_fails == 0 ? 0 : 1;
}
