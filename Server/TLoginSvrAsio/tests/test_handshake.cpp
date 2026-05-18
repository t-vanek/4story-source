// End-to-end smoke test for TLoginSvrAsio. Spins up the server on
// an ephemeral port, opens a client AsioSession against it, sends
// a packet-codec-framed CS_LOGIN_REQ with the legacy protocol
// version, asserts the CS_LOGIN_ACK round-trips with bResult ==
// LR_SUCCESS (0).
//
// PCH-free. Doesn't link against legacy CSession / CPacket — only
// the modernized tnetlib surface.

#include "../login_server.h"
#include "asio_session.h"
#include "MessageId.h"
#include "packet_codec.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
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

constexpr std::uint16_t kProtocolVersion = 0x2918;

// Build the wVersion-only CS_LOGIN_REQ body. The server stub only
// reads the first two bytes; we don't need to populate the rest.
std::vector<std::byte> MakeLoginReqBody()
{
    std::vector<std::byte> body(2);
    std::memcpy(body.data(), &kProtocolVersion, 2);
    return body;
}

void TestLoginHandshake()
{
    std::printf("[tloginsvr_asio handshake — version-match]\n");

    asio::io_context server_io;
    tloginsvr::LoginServer server(server_io, 0); // ephemeral
    const std::uint16_t port = server.Port();
    Check(port != 0, "server bound to ephemeral port");

    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Client side — also an AsioSession (PeerType::Server so it
    // matches the server's Phase-3 XOR-only crypto choice).
    asio::io_context client_io;
    tcp::socket client_sock(client_io);
    client_sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));
    Check(client_sock.is_open(), "client connected");

    auto client_sess = std::make_shared<tnetlib::AsioSession>(
        std::move(client_sock), tnetlib::PeerType::Server);

    std::atomic<bool> got_ack{false};
    std::uint8_t      ack_result = 0xFF;

    auto recv_coro = [client_sess, &got_ack, &ack_result]()
        -> asio::awaitable<void>
    {
        co_await client_sess->RunPackets(
            [&got_ack, &ack_result](const tnetlib::DecodedPacket& pkt) {
                if (pkt.wId == tnetlib::protocol::ToUint16(
                        tnetlib::protocol::MessageId::CS_LOGIN_ACK))
                {
                    if (!pkt.body.empty())
                        ack_result = static_cast<std::uint8_t>(pkt.body[0]);
                    got_ack = true;
                }
            });
    };
    asio::co_spawn(client_io, recv_coro(), asio::detached);

    auto send_coro = [client_sess]() -> asio::awaitable<void>
    {
        const auto body = MakeLoginReqBody();
        co_await client_sess->SendPacket(
            tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_LOGIN_REQ),
            std::span<const std::byte>(body.data(), body.size()));
    };
    asio::co_spawn(client_io, send_coro(), asio::detached);

    std::thread client_thread([&client_io] { client_io.run(); });

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!got_ack.load() &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    Check(got_ack.load(), "CS_LOGIN_ACK received within 2s");
    Check(ack_result == 0, "ack bResult == LR_SUCCESS (0)");

    client_sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    server_io.stop();
    if (server_thread.joinable()) server_thread.join();
}

} // namespace

int main()
{
    std::printf("=== tloginsvr_asio handshake test ===\n");
    try
    {
        TestLoginHandshake();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
