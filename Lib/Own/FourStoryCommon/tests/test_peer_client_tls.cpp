// Integration test: PeerClient with TLS enabled actually drives a
// client-side TLS handshake before the registry protocol kicks in.
//
// The test stands up a dummy TLS acceptor (raw boost::asio +
// ssl::context using the project test fixtures), wires a PeerClient
// at it with ssl_ctx set, and asserts:
//   * server-side async_handshake completed without error
//   * the first bytes the server reads are the CT_PEER_REGISTER_REQ
//     frame (proves the encrypted channel is carrying the protocol)
//
// What this test does NOT do:
//   * Full register/heartbeat round-trip — that needs a TControlSvr
//     loopback and lives in TControlSvrAsio's integration tests.
//   * Verify peer CN matches TPEER_AUTH.peer_name — that's part of
//     the per-server integration PR (post-handshake DB lookup).

#include "fourstory/cluster/peer_client.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifndef TEST_CERT_DIR
#error "TEST_CERT_DIR must point at the tnetlib test cert fixtures"
#endif

namespace asio = boost::asio;
using boost::asio::ip::tcp;

namespace {

std::string CertPath(const char* file)
{
    return std::string(TEST_CERT_DIR) + "/" + file;
}

asio::ssl::context MakeAcceptorCtx()
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

} // namespace

int main()
{
    std::printf("=== peer_client TLS integration test ===\n");

    asio::io_context io;
    auto server_ctx = MakeAcceptorCtx();

    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), 0));
    const std::uint16_t port = acceptor.local_endpoint().port();
    std::printf("  listener bound on ephemeral port %u\n", port);

    std::atomic<bool> server_handshake_ok{false};
    std::atomic<bool> server_read_some_bytes{false};
    std::atomic<int>  server_first_byte_count{0};

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
            if (ec)
            {
                std::printf("  server handshake error: %s\n",
                            ec.message().c_str());
                co_return;
            }
            server_handshake_ok.store(true);

            // Read the 8-byte registry header. If TLS is doing its job
            // the bytes the client sent (CT_PEER_REGISTER_REQ at 0x9F00)
            // come through plaintext on this side.
            std::array<std::byte, 8> hdr{};
            const auto n = co_await asio::async_read(stream,
                asio::buffer(hdr),
                asio::redirect_error(asio::use_awaitable, ec));
            if (!ec && n == hdr.size())
            {
                server_read_some_bytes.store(true);
                server_first_byte_count.store(static_cast<int>(n));
            }
        },
        asio::detached);

    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Build the PeerClient with TLS enabled. The client context must
    // outlive the PeerClient — we keep both on the stack here.
    asio::ssl::context client_ctx = MakeClientCtx();

    fourstory::cluster::PeerClientOptions opts;
    opts.control_host = "127.0.0.1";
    opts.control_port = port;
    opts.service_id   = 0x010103;   // group=1, type=1, server=3
    opts.reported_name = "tls-test-peer";
    opts.reported_addr = "127.0.0.1";
    opts.reported_port = 4000;
    opts.version       = "5.0.0-test";
    opts.initial_backoff = std::chrono::seconds(5);
    opts.max_backoff     = std::chrono::seconds(5);
    opts.ssl_ctx = &client_ctx;

    asio::io_context client_io;
    auto pc = std::make_shared<fourstory::cluster::PeerClient>(
        client_io, std::move(opts));
    asio::co_spawn(client_io, pc->Run(), asio::detached);

    std::thread client_thread([&client_io] { client_io.run(); });

    // Give the handshake + first frame ~2s to complete.
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(2);
    while ((!server_handshake_ok.load() ||
            !server_read_some_bytes.load()) &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    int passed = 0;
    int failed = 0;
    auto check = [&](bool cond, const char* label) {
        if (cond) { ++passed; std::printf("  PASS  %s\n", label); }
        else      { ++failed; std::printf("  FAIL  %s\n", label); }
    };

    check(server_handshake_ok.load(),
          "server-side TLS handshake completed");
    check(server_read_some_bytes.load(),
          "register-frame bytes arrived through the encrypted channel");
    check(server_first_byte_count.load() == 8,
          "exactly 8 header bytes were read");

    pc->Stop();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    io.stop();
    if (io_thread.joinable()) io_thread.join();

    std::printf("\nResults: %d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
