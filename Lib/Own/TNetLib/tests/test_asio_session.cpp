// Standalone test for AsioSession + AsioListener. Spins up an echo
// server on an ephemeral port, connects a client, sends a few bytes,
// asserts the same bytes come back.
//
// PCH-free; depends only on tnetlib::AsioSession + tnetlib::AsioListener
// + Boost.Asio. Run via ctest.

#include "../TNetLib/asio_session.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <vector>

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

// Per-session echo: read whatever bytes come in, write them back. Held
// alive for the duration of the read loop via shared_from_this.
asio::awaitable<void> RunEchoSession(std::shared_ptr<tnetlib::AsioSession> sess)
{
    co_await sess->Run([sess](std::span<const std::byte> bytes) {
        // Echo: spawn a write back to the client. async_write is
        // sequenced via the session's own socket strand (single thread
        // in this test, so no cross-strand concerns).
        std::vector<std::byte> copy(bytes.begin(), bytes.end());
        asio::co_spawn(
            sess->Socket().get_executor(),
            [sess, copy = std::move(copy)]() -> asio::awaitable<void> {
                co_await sess->Send(std::span<const std::byte>(copy.data(), copy.size()));
            },
            asio::detached);
    });
}

void TestEchoRoundtrip()
{
    std::printf("[echo roundtrip on ephemeral port]\n");

    asio::io_context io;
    tnetlib::AsioListener listener(io.get_executor(), 0);
    const std::uint16_t port = listener.Port();
    Check(port != 0, "listener bound to ephemeral port");

    // Spawn the accept loop.
    asio::co_spawn(io,
        listener.Run([&io](tcp::socket socket) {
            auto sess = std::make_shared<tnetlib::AsioSession>(
                std::move(socket), tnetlib::PeerType::Server);
            asio::co_spawn(io, RunEchoSession(sess), asio::detached);
        }),
        asio::detached);

    // Run the io_context on a background thread so we can drive the
    // client synchronously from the main thread.
    std::thread io_thread([&io] { io.run(); });

    // Give the listener a moment to start.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Client side: classic sync socket. The server is the modern path
    // under test; the client just needs to round-trip bytes.
    {
        asio::io_context client_io;
        tcp::socket client(client_io);
        client.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));
        Check(client.is_open(), "client connected");

        const std::string msg = "hello from the modern world";
        const std::size_t sent = asio::write(client,
            asio::buffer(msg.data(), msg.size()));
        Check(sent == msg.size(), "client wrote all bytes");

        std::string recv_buf(msg.size(), '\0');
        const std::size_t got = asio::read(client,
            asio::buffer(recv_buf.data(), recv_buf.size()));
        Check(got == msg.size(), "client read same byte count back");
        Check(recv_buf == msg, "echoed bytes match original");

        client.shutdown(tcp::socket::shutdown_both);
        client.close();
    }

    // Stop the io_context.
    io.stop();
    if (io_thread.joinable())
        io_thread.join();
}

// Two AsioSessions talking to each other via the packet codec. One side
// listens, accepts, and echoes inbound packets back (same wId, same
// body). The other side connects, sends three packets with distinct
// payloads, then verifies all three arrive decoded with matching wIds
// and bodies.
void TestPacketRoundtrip()
{
    std::printf("[full packet round-trip via codec]\n");

    asio::io_context io;
    tnetlib::AsioListener listener(io.get_executor(), 0);
    const std::uint16_t port = listener.Port();
    Check(port != 0, "listener bound to ephemeral port");

    // Server side — accept and echo packets at the codec level.
    asio::co_spawn(io,
        listener.Run([&io](tcp::socket socket) {
            auto sess = std::make_shared<tnetlib::AsioSession>(
                std::move(socket), tnetlib::PeerType::Server);
            asio::co_spawn(io,
                [sess]() -> asio::awaitable<void> {
                    co_await sess->RunPackets(
                        [sess](const tnetlib::DecodedPacket& pkt) {
                            // Copy body before scheduling the echo —
                            // pkt.body is only valid for the duration
                            // of this callback.
                            std::vector<std::byte> body_copy(
                                pkt.body.begin(), pkt.body.end());
                            const std::uint16_t wId = pkt.wId;
                            asio::co_spawn(
                                sess->Socket().get_executor(),
                                [sess, wId, body_copy = std::move(body_copy)]()
                                    -> asio::awaitable<void> {
                                    co_await sess->SendPacket(wId,
                                        std::span<const std::byte>(
                                            body_copy.data(), body_copy.size()));
                                },
                                asio::detached);
                        });
                },
                asio::detached);
        }),
        asio::detached);

    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Client side — also an AsioSession (so we exercise both directions
    // of the codec). Send three packets, expect three back.
    asio::io_context client_io;
    auto client_sock = tcp::socket(client_io);
    client_sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));
    Check(client_sock.is_open(), "client connected");

    auto client_sess = std::make_shared<tnetlib::AsioSession>(
        std::move(client_sock), tnetlib::PeerType::Server);

    // Collect echoed packets here.
    std::vector<std::pair<std::uint16_t, std::vector<std::byte>>> received;
    std::atomic<int> received_count{0};

    auto recv_coro = [client_sess, &received, &received_count]()
        -> asio::awaitable<void>
    {
        co_await client_sess->RunPackets(
            [&received, &received_count](const tnetlib::DecodedPacket& pkt) {
                received.push_back({
                    pkt.wId,
                    std::vector<std::byte>(pkt.body.begin(), pkt.body.end())});
                received_count.fetch_add(1);
            });
    };
    asio::co_spawn(client_io, recv_coro(), asio::detached);

    // Send three packets.
    auto send_coro = [client_sess]() -> asio::awaitable<void>
    {
        const char* p1 = "hello";
        const char* p2 = "modern";
        const char* p3 = "world!!";
        co_await client_sess->SendPacket(0x1001,
            std::span<const std::byte>(
                reinterpret_cast<const std::byte*>(p1), 5));
        co_await client_sess->SendPacket(0x1002,
            std::span<const std::byte>(
                reinterpret_cast<const std::byte*>(p2), 6));
        co_await client_sess->SendPacket(0x1003,
            std::span<const std::byte>(
                reinterpret_cast<const std::byte*>(p3), 7));
    };
    asio::co_spawn(client_io, send_coro(), asio::detached);

    // Drive the client io_context on a dedicated thread; wait for echoes.
    std::thread client_thread([&client_io] { client_io.run(); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (received_count.load() < 3 && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    Check(received.size() == 3, "received all three echoed packets");
    if (received.size() == 3)
    {
        Check(received[0].first == 0x1001, "packet 0 wId matches");
        Check(received[0].second.size() == 5 &&
              std::memcmp(received[0].second.data(), "hello", 5) == 0,
              "packet 0 body matches");
        Check(received[1].first == 0x1002, "packet 1 wId matches");
        Check(received[1].second.size() == 6 &&
              std::memcmp(received[1].second.data(), "modern", 6) == 0,
              "packet 1 body matches");
        Check(received[2].first == 0x1003, "packet 2 wId matches");
        Check(received[2].second.size() == 7 &&
              std::memcmp(received[2].second.data(), "world!!", 7) == 0,
              "packet 2 body matches");
    }

    // Clean shutdown.
    client_sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    io.stop();
    if (io_thread.joinable()) io_thread.join();
}

// Same packet round-trip as above, but with the RC4 layer enabled on
// both ends. Validates the full wire-format pipeline that a real
// legacy client → server connection runs through:
//   client SEND:  body + checksum → XOR body → XOR header → RC4 entire frame (wSize preserved)
//   server RECV:  read → RC4 entire frame (wSize restored) → XOR header → seq check → XOR body
//
// "Client" side here is acting as a fake legacy client (RC4 outbound only),
// "server" side is the modernized AsioSession in server-of-legacy-client
// mode (RC4 inbound only). One round-trip; on the return path neither
// side does RC4 (matches legacy convention: server-to-client is XOR-only).
void TestPacketRoundtripWithRC4()
{
    std::printf("[full packet round-trip via RC4 + codec]\n");

    // Use a non-trivial secret key with embedded high bytes so any byte-
    // order or signedness regression in the MD5/RC4 chain surfaces.
    const std::vector<std::byte> secret = [] {
        std::vector<std::byte> v;
        const char* s = "rc4-secret-\x92test\x94";
        for (const char* p = s; *p; ++p) v.push_back(static_cast<std::byte>(*p));
        return v;
    }();

    asio::io_context io;
    tnetlib::AsioListener listener(io.get_executor(), 0);
    const std::uint16_t port = listener.Port();
    Check(port != 0, "listener bound");

    // Server-side echo.
    asio::co_spawn(io,
        listener.Run([&io, &secret](tcp::socket socket) {
            auto sess = std::make_shared<tnetlib::AsioSession>(
                std::move(socket), tnetlib::PeerType::Client);
            // Server side: RC4 inbound only. Outbound back to the "client"
            // here is plain to match legacy m_bUseCrypt=FALSE on outbound.
            // (The fake client below mirrors that: outbound RC4, inbound plain.)
            sess->EnableInboundRC4(secret);
            asio::co_spawn(io,
                [sess]() -> asio::awaitable<void> {
                    co_await sess->RunPackets(
                        [sess](const tnetlib::DecodedPacket& pkt) {
                            std::vector<std::byte> body_copy(
                                pkt.body.begin(), pkt.body.end());
                            const std::uint16_t wId = pkt.wId;
                            asio::co_spawn(
                                sess->Socket().get_executor(),
                                [sess, wId, body_copy = std::move(body_copy)]()
                                    -> asio::awaitable<void> {
                                    co_await sess->SendPacket(wId,
                                        std::span<const std::byte>(
                                            body_copy.data(), body_copy.size()));
                                },
                                asio::detached);
                        });
                },
                asio::detached);
        }),
        asio::detached);

    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    asio::io_context client_io;
    tcp::socket client_sock(client_io);
    client_sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));
    Check(client_sock.is_open(), "fake-client connected");

    auto client_sess = std::make_shared<tnetlib::AsioSession>(
        std::move(client_sock), tnetlib::PeerType::Server);
    client_sess->EnableOutboundRC4(secret);

    std::vector<std::pair<std::uint16_t, std::vector<std::byte>>> received;
    std::atomic<int> received_count{0};

    asio::co_spawn(client_io,
        [client_sess, &received, &received_count]() -> asio::awaitable<void> {
            co_await client_sess->RunPackets(
                [&received, &received_count](const tnetlib::DecodedPacket& pkt) {
                    received.push_back({
                        pkt.wId,
                        std::vector<std::byte>(pkt.body.begin(), pkt.body.end())});
                    received_count.fetch_add(1);
                });
        },
        asio::detached);

    asio::co_spawn(client_io,
        [client_sess]() -> asio::awaitable<void> {
            const char* p1 = "rc4-encrypted";
            const char* p2 = "wire works";
            co_await client_sess->SendPacket(0x2001,
                std::span<const std::byte>(
                    reinterpret_cast<const std::byte*>(p1), 13));
            co_await client_sess->SendPacket(0x2002,
                std::span<const std::byte>(
                    reinterpret_cast<const std::byte*>(p2), 10));
        },
        asio::detached);

    std::thread client_thread([&client_io] { client_io.run(); });
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (received_count.load() < 2 &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    Check(received.size() == 2, "received both echoed packets through RC4 layer");
    if (received.size() == 2)
    {
        Check(received[0].first == 0x2001 &&
              received[0].second.size() == 13 &&
              std::memcmp(received[0].second.data(), "rc4-encrypted", 13) == 0,
              "packet 0 wId + body match after RC4 round-trip");
        Check(received[1].first == 0x2002 &&
              received[1].second.size() == 10 &&
              std::memcmp(received[1].second.data(), "wire works", 10) == 0,
              "packet 1 wId + body match after RC4 round-trip");
    }

    client_sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    io.stop();
    if (io_thread.joinable()) io_thread.join();
}

} // namespace

int main()
{
    std::printf("=== tnetlib AsioSession tests ===\n");
    try
    {
        TestEchoRoundtrip();
        TestPacketRoundtrip();
        TestPacketRoundtripWithRC4();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception thrown: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
