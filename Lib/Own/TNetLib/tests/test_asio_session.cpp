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

} // namespace

int main()
{
    std::printf("=== tnetlib AsioSession tests ===\n");
    try
    {
        TestEchoRoundtrip();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception thrown: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
