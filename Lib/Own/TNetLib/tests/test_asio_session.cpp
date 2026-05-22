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

#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
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

// Verifies that the ErrorLogger callback is invoked when RunPackets
// rejects a malformed packet, instead of silently terminating the loop
// (previous behaviour). Sends a 24-byte garbage frame with a valid
// wSize so framing accepts it; the decrypted header will then have a
// dwNumber that doesn't match the expected sequence (=1), tripping the
// "sequence mismatch" branch. The custom sink captures the message so
// the test can confirm both that it fires and what it says.
std::mutex g_log_mtx;
std::vector<std::string> g_log_captured;

void TestErrorLoggerOnMalformedPacket()
{
    std::printf("[error logger fires on protocol error]\n");

    g_log_captured.clear();
    tnetlib::AsioSession::SetErrorLogger([](std::string_view msg) {
        std::lock_guard<std::mutex> lk(g_log_mtx);
        g_log_captured.emplace_back(msg);
    });

    asio::io_context io;
    tnetlib::AsioListener listener(io.get_executor(), 0);
    const std::uint16_t port = listener.Port();
    Check(port != 0, "listener bound to ephemeral port");

    asio::co_spawn(io,
        listener.Run([&io](tcp::socket socket) {
            auto sess = std::make_shared<tnetlib::AsioSession>(
                std::move(socket), tnetlib::PeerType::Client);
            asio::co_spawn(io,
                [sess]() -> asio::awaitable<void> {
                    co_await sess->RunPackets(
                        [](const tnetlib::DecodedPacket&){});
                },
                asio::detached);
        }),
        asio::detached);

    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    asio::io_context client_io;
    tcp::socket client(client_io);
    client.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));
    Check(client.is_open(), "client connected");

    // 16-byte header + 8-byte body = 24 total. wSize is plaintext on the
    // wire and must be in [16, kMaxPacketSize); set it correctly so the
    // framer reads the body. Everything else is zero — XOR decrypt with
    // the per-sequence key will yield a dwNumber != 1, hitting the
    // sequence-mismatch path.
    std::array<std::uint8_t, 24> frame{};
    frame[0] = 24; // wSize low byte
    frame[1] = 0;  // wSize high byte
    asio::write(client, asio::buffer(frame));

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline)
    {
        {
            std::lock_guard<std::mutex> lk(g_log_mtx);
            if (!g_log_captured.empty()) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    {
        std::lock_guard<std::mutex> lk(g_log_mtx);
        Check(!g_log_captured.empty(), "logger captured at least one message");
        if (!g_log_captured.empty())
        {
            const auto& msg = g_log_captured.front();
            const bool starts = msg.rfind("recv:", 0) == 0;
            Check(starts, "captured message is a 'recv:' diagnostic");
        }
    }

    // Restore default sink so later tests don't pollute the capture
    // vector (and so a follow-up suite invocation starts fresh).
    tnetlib::AsioSession::SetErrorLogger(nullptr);

    boost::system::error_code ec;
    client.shutdown(tcp::socket::shutdown_both, ec);
    client.close(ec);
    io.stop();
    if (io_thread.joinable()) io_thread.join();
}

// Verifies that SendPacket refuses oversized payloads instead of
// emitting a malformed wire frame. With the logger sink active we also
// see the diagnostic that explains why the send was dropped.
void TestSendPacketRejectsOversizedBody()
{
    std::printf("[SendPacket rejects oversized body]\n");

    g_log_captured.clear();
    tnetlib::AsioSession::SetErrorLogger([](std::string_view msg) {
        std::lock_guard<std::mutex> lk(g_log_mtx);
        g_log_captured.emplace_back(msg);
    });

    asio::io_context io;
    tnetlib::AsioListener listener(io.get_executor(), 0);
    const std::uint16_t port = listener.Port();

    asio::co_spawn(io,
        listener.Run([](tcp::socket socket) {
            // Accept and drop — we only care about the SendPacket
            // outcome on the client side.
            (void)socket;
        }),
        asio::detached);

    std::thread io_thread([&io] { io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    asio::io_context client_io;
    tcp::socket client_sock(client_io);
    client_sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));
    auto client_sess = std::make_shared<tnetlib::AsioSession>(
        std::move(client_sock), tnetlib::PeerType::Server);

    // Body just over the codec limit. kMaxPacketSize is 0xFFFF; subtract
    // the 16-byte header to get the largest accepted body, then add one.
    std::vector<std::byte> too_big(
        static_cast<std::size_t>(0xFFFF) - 16, std::byte{0});
    asio::co_spawn(client_io,
        [client_sess, &too_big]() -> asio::awaitable<void> {
            co_await client_sess->SendPacket(0x9999,
                std::span<const std::byte>(too_big.data(), too_big.size()));
        },
        asio::detached);

    std::thread client_thread([&client_io] { client_io.run(); });
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < deadline)
    {
        {
            std::lock_guard<std::mutex> lk(g_log_mtx);
            if (!g_log_captured.empty()) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    {
        std::lock_guard<std::mutex> lk(g_log_mtx);
        Check(!g_log_captured.empty(),
              "logger captured oversize-body diagnostic");
        if (!g_log_captured.empty())
        {
            const bool is_send = g_log_captured.front().rfind("send:", 0) == 0;
            Check(is_send, "captured message is a 'send:' diagnostic");
        }
    }

    tnetlib::AsioSession::SetErrorLogger(nullptr);
    client_sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    io.stop();
    if (io_thread.joinable()) io_thread.join();
}

// Stress test for the Phase-3 send queue. Spawns N concurrent producer
// coroutines on the client, each issuing M SendPackets back-to-back
// without explicit serialization between them. The drain coroutine
// inside AsioSession is responsible for keeping sequence numbers and
// socket writes ordered. Server-side: the per-session read loop
// receives the packets and counts them; any sequence drift or
// checksum mismatch would terminate RunPackets and short the count.
//
// Asserts: all N*M packets land, and the encoded wId of each one falls
// into the expected per-producer range so we know none were corrupted.
void TestConcurrentSendPacketSerializes()
{
    std::printf("[concurrent SendPacket serializes via internal queue]\n");

    constexpr int kProducers     = 8;
    constexpr int kPacketsPerOne = 50;
    constexpr int kTotal         = kProducers * kPacketsPerOne;

    asio::io_context io;
    tnetlib::AsioListener listener(io.get_executor(), 0);
    const std::uint16_t port = listener.Port();
    Check(port != 0, "listener bound");

    std::atomic<int> server_received{0};
    std::vector<std::uint16_t> server_wids;
    std::mutex server_wids_mtx;

    asio::co_spawn(io,
        listener.Run([&](tcp::socket socket) {
            auto sess = std::make_shared<tnetlib::AsioSession>(
                std::move(socket), tnetlib::PeerType::Server);
            asio::co_spawn(io,
                [sess, &server_received, &server_wids, &server_wids_mtx]()
                    -> asio::awaitable<void> {
                    co_await sess->RunPackets(
                        [&](const tnetlib::DecodedPacket& pkt) {
                            {
                                std::lock_guard<std::mutex> lk(
                                    server_wids_mtx);
                                server_wids.push_back(pkt.wId);
                            }
                            server_received.fetch_add(1);
                        });
                },
                asio::detached);
        }),
        asio::detached);

    // Run io_context on multiple threads to amplify any latent races
    // between concurrent producers and the drain. Two suffices to
    // detect ordering bugs without burning CPU.
    std::thread io_thread1([&io] { io.run(); });
    std::thread io_thread2([&io] { io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    asio::io_context client_io;
    tcp::socket client_sock(client_io);
    client_sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));
    auto client_sess = std::make_shared<tnetlib::AsioSession>(
        std::move(client_sock), tnetlib::PeerType::Server);

    // RunPackets has to start so the client-side drain spawns. We
    // don't care about inbound packets here, just kick it.
    asio::co_spawn(client_io,
        [client_sess]() -> asio::awaitable<void> {
            co_await client_sess->RunPackets(
                [](const tnetlib::DecodedPacket&){});
        },
        asio::detached);

    // Spawn kProducers parallel sender coroutines. Each one tags its
    // packets with a wId in [pid*1000, pid*1000 + kPacketsPerOne) so
    // we can spot interleaving on the receive side.
    for (int pid = 0; pid < kProducers; ++pid)
    {
        asio::co_spawn(client_io,
            [client_sess, pid]() -> asio::awaitable<void> {
                std::vector<std::byte> payload(8, std::byte{0xAB});
                for (int i = 0; i < kPacketsPerOne; ++i)
                {
                    const std::uint16_t wId =
                        static_cast<std::uint16_t>(pid * 1000 + i);
                    co_await client_sess->SendPacket(wId,
                        std::span<const std::byte>(payload.data(),
                                                    payload.size()));
                }
            },
            asio::detached);
    }

    std::thread client_thread([&client_io] { client_io.run(); });

    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(5);
    while (server_received.load() < kTotal &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    Check(server_received.load() == kTotal,
          "server received every packet (no sequence/checksum failure)");

    // Verify each producer's packets all arrived (wIds present, no
    // gaps). Allows arbitrary interleaving between producers.
    {
        std::lock_guard<std::mutex> lk(server_wids_mtx);
        std::vector<int> per_producer(kProducers, 0);
        bool all_in_range = true;
        for (auto w : server_wids)
        {
            const int pid = w / 1000;
            const int idx = w % 1000;
            if (pid < 0 || pid >= kProducers ||
                idx < 0 || idx >= kPacketsPerOne)
            {
                all_in_range = false;
                break;
            }
            per_producer[pid]++;
        }
        Check(all_in_range, "every wId falls in an expected producer range");
        bool counts_ok = true;
        for (int c : per_producer)
            if (c != kPacketsPerOne) counts_ok = false;
        Check(counts_ok, "each producer's packets all arrived");
    }

    client_sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    io.stop();
    if (io_thread1.joinable()) io_thread1.join();
    if (io_thread2.joinable()) io_thread2.join();
}

} // namespace

int main()
{
    std::printf("=== tnetlib AsioSession tests ===\n");
    // Mute the default stderr sink for the rest of the suite — happy-path
    // tests don't expect any diagnostics, and the dedicated logger tests
    // install their own sink anyway. Individual tests restore the default
    // (nullptr) on the way out.
    tnetlib::AsioSession::SetErrorLogger(nullptr);
    try
    {
        TestEchoRoundtrip();
        TestPacketRoundtrip();
        TestPacketRoundtripWithRC4();
        TestErrorLoggerOnMalformedPacket();
        TestSendPacketRejectsOversizedBody();
        TestConcurrentSendPacketSerializes();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception thrown: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
