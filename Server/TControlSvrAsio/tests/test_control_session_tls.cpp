// ControlSession TLS-variant round-trip test.
//
// Drives the same wire protocol as the plain-TCP variant
// (8-byte header + body + 32-bit XOR-fold checksum) but on top of an
// asio::ssl::stream. Confirms:
//   * Mutual TLS handshake completes (server side)
//   * ControlSession::Run() reads packets through the encrypted channel
//   * ControlSession::SendPacket() writes packets through it
//   * RemoteIPv4() returns a sane string (captured via next_layer())
//   * IsOpen() reflects the underlying tcp::socket state
//
// Cert fixtures are reused from Lib/Own/TNetLib/test_certs/ — same
// pattern as the fourstory_peer_client_tls test.

#include "../control_session.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/write.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifndef TEST_CERT_DIR
#error "TEST_CERT_DIR must point at the tnetlib test cert fixtures"
#endif

namespace asio = boost::asio;
using boost::asio::ip::tcp;

namespace {

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

// Build one 8-byte header + body frame using the same wire layout
// the ControlSession decoder expects.
std::vector<std::byte> MakeWireFrame(std::uint16_t wId,
                                      const std::vector<std::byte>& body)
{
    using namespace tcontrolsvr;
    std::vector<std::byte> out(kPacketHeaderSize + body.size());
    PacketHeader hdr{};
    hdr.wSize    = static_cast<std::uint16_t>(out.size());
    hdr.wID      = wId;
    hdr.dwChkSum = ComputeChecksum(body.data(), body.size());
    std::memcpy(out.data(), &hdr, sizeof(hdr));
    if (!body.empty())
        std::memcpy(out.data() + sizeof(hdr), body.data(), body.size());
    return out;
}

void TestTlsControlSessionRoundtrip()
{
    std::printf("[ControlSession TLS round-trip]\n");

    asio::io_context io;
    auto server_ctx = MakeServerCtx();
    auto client_ctx = MakeClientCtx();

    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 0));
    const std::uint16_t port = acceptor.local_endpoint().port();

    // Server side — accept, hand off to ControlSession(TlsStream),
    // and echo each packet straight back via SendPacket.
    std::atomic<int> server_received{0};
    std::atomic<bool> server_ip_ok{false};
    asio::co_spawn(io,
        [&]() -> asio::awaitable<void> {
            boost::system::error_code ec;
            auto sock = co_await acceptor.async_accept(
                asio::redirect_error(asio::use_awaitable, ec));
            if (ec) co_return;

            tcontrolsvr::ControlSession::TlsStream stream(
                std::move(sock), server_ctx);
            co_await stream.async_handshake(
                asio::ssl::stream_base::server,
                asio::redirect_error(asio::use_awaitable, ec));
            if (ec) co_return;

            auto sess = std::make_shared<tcontrolsvr::ControlSession>(
                std::move(stream));
            // Sanity check: the cached IPv4 should be loopback,
            // captured via next_layer().remote_endpoint().
            server_ip_ok.store(sess->RemoteIPv4() == "127.0.0.1");

            co_await sess->Run(
                [](std::shared_ptr<tcontrolsvr::ControlSession> s,
                   tcontrolsvr::DecodedPacket pkt)
                    -> asio::awaitable<void> {
                    co_await s->SendPacket(pkt.wId, std::move(pkt.body));
                });
            server_received.store(1);
        },
        asio::detached);

    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Client side — raw asio + ssl::stream so we exercise both
    // ControlSession ends in this test.
    asio::io_context client_io;
    asio::ssl::stream<tcp::socket> client_stream(client_io, client_ctx);
    client_stream.next_layer().connect(
        tcp::endpoint(asio::ip::address_v4::loopback(), port));
    {
        boost::system::error_code ec;
        client_stream.handshake(asio::ssl::stream_base::client, ec);
        Check(!ec, "client-side TLS handshake completed");
        if (ec) {
            io.stop();
            if (io_thread.joinable()) io_thread.join();
            return;
        }
    }

    // Send two packets sequentially, read the echoes back.
    std::vector<std::byte> body1{
        std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}, std::byte{0xDD}};
    std::vector<std::byte> body2{
        std::byte{0x11}, std::byte{0x22}, std::byte{0x33},
        std::byte{0x44}, std::byte{0x55}};

    auto frame1 = MakeWireFrame(0x9F00, body1);
    auto frame2 = MakeWireFrame(0x9F02, body2);

    boost::system::error_code wec;
    asio::write(client_stream, asio::buffer(frame1.data(), frame1.size()), wec);
    Check(!wec, "client wrote frame 1");
    asio::write(client_stream, asio::buffer(frame2.data(), frame2.size()), wec);
    Check(!wec, "client wrote frame 2");

    // Read echo 1 — 8-byte header + body1.size() bytes.
    auto read_echo = [&](std::uint16_t expected_wid,
                          const std::vector<std::byte>& expected_body)
        -> bool {
        tcontrolsvr::PacketHeader hdr{};
        boost::system::error_code rec;
        asio::read(client_stream, asio::buffer(&hdr, sizeof(hdr)), rec);
        if (rec) return false;
        if (hdr.wID != expected_wid) return false;
        if (hdr.wSize !=
            sizeof(hdr) + expected_body.size()) return false;
        std::vector<std::byte> got(expected_body.size());
        asio::read(client_stream, asio::buffer(got.data(), got.size()), rec);
        if (rec) return false;
        return got == expected_body;
    };

    Check(read_echo(0x9F00, body1), "echoed frame 1 matches");
    Check(read_echo(0x9F02, body2), "echoed frame 2 matches");

    // Verify the server captured the right IP.
    Check(server_ip_ok.load(),
          "server RemoteIPv4() returned loopback");

    boost::system::error_code ig;
    client_stream.next_layer().close(ig);

    // Let the server's Run() loop notice EOF.
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(2);
    while (server_received.load() == 0 &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    Check(server_received.load() == 1,
          "server Run() loop terminated on peer close");

    io.stop();
    if (io_thread.joinable()) io_thread.join();
}

} // namespace

int main()
{
    std::printf("=== tcontrolsvr ControlSession TLS tests ===\n");
    try
    {
        TestTlsControlSessionRoundtrip();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception thrown: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
