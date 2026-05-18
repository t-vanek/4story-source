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
    // Default config (no RC4) so the test peer can be a plain
    // PeerType::Server too — matches the modernized-peer migration
    // mode. The real-client mode (RC4 enabled) is exercised by the
    // TestPacketRoundtripWithRC4 KAT in test_tnetlib_asio.
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

// Full lobby flow — login + grouplist + channellist + charlist round
// trip. All four handlers are stubs returning empty lists, but the
// test proves the dispatcher routes each request to the right handler
// and the per-session sequence-number counter stays consistent across
// multiple inbound/outbound packets.
void TestLobbyFlow()
{
    std::printf("[tloginsvr_asio full lobby flow — login + 3 list reqs]\n");

    asio::io_context server_io;
    tloginsvr::LoginServer server(server_io, 0);
    const std::uint16_t port = server.Port();
    Check(port != 0, "server bound");

    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    asio::io_context client_io;
    tcp::socket client_sock(client_io);
    client_sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));
    Check(client_sock.is_open(), "client connected");

    auto client_sess = std::make_shared<tnetlib::AsioSession>(
        std::move(client_sock), tnetlib::PeerType::Server);

    std::atomic<int> ack_count{0};
    std::vector<std::uint16_t> received_ids;

    asio::co_spawn(client_io,
        [client_sess, &ack_count, &received_ids]() -> asio::awaitable<void> {
            co_await client_sess->RunPackets(
                [&ack_count, &received_ids](const tnetlib::DecodedPacket& pkt) {
                    received_ids.push_back(pkt.wId);
                    ack_count.fetch_add(1);
                });
        },
        asio::detached);

    asio::co_spawn(client_io,
        [client_sess]() -> asio::awaitable<void> {
            using namespace tnetlib::protocol;
            // 1. login (wVersion only)
            std::uint16_t v = 0x2918;
            std::byte login_body[2];
            std::memcpy(login_body, &v, 2);
            co_await client_sess->SendPacket(
                ToUint16(MessageId::CS_LOGIN_REQ),
                std::span<const std::byte>(login_body, 2));
            // 2. grouplist (empty body)
            co_await client_sess->SendPacket(
                ToUint16(MessageId::CS_GROUPLIST_REQ),
                std::span<const std::byte>{});
            // 3. channellist (BYTE bGroupID = 1)
            std::byte ch_body[1] = { std::byte{1} };
            co_await client_sess->SendPacket(
                ToUint16(MessageId::CS_CHANNELLIST_REQ),
                std::span<const std::byte>(ch_body, 1));
            // 4. charlist (BYTE bGroupID = 1)
            std::byte cl_body[1] = { std::byte{1} };
            co_await client_sess->SendPacket(
                ToUint16(MessageId::CS_CHARLIST_REQ),
                std::span<const std::byte>(cl_body, 1));
        },
        asio::detached);

    std::thread client_thread([&client_io] { client_io.run(); });
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (ack_count.load() < 4 &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    Check(ack_count.load() == 4, "all 4 acks received");
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    if (received_ids.size() == 4)
    {
        // Order is preserved by the per-session TCP stream + the
        // server's per-packet co_spawn dispatch (each handler
        // serializes its own SendPacket through the session's
        // implicit strand).
        Check(received_ids[0] == ToUint16(MessageId::CS_LOGIN_ACK),
              "ack 0 is CS_LOGIN_ACK");
        Check(received_ids[1] == ToUint16(MessageId::CS_GROUPLIST_ACK),
              "ack 1 is CS_GROUPLIST_ACK");
        Check(received_ids[2] == ToUint16(MessageId::CS_CHANNELLIST_ACK),
              "ack 2 is CS_CHANNELLIST_ACK");
        Check(received_ids[3] == ToUint16(MessageId::CS_CHARLIST_ACK),
              "ack 3 is CS_CHARLIST_ACK");
    }

    client_sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    server_io.stop();
    if (server_thread.joinable()) server_thread.join();
}

// Exercise the remaining stub handlers that have an ack: CREATECHAR,
// DELCHAR, START, VETERAN. AGREEMENT and HOTSEND have no ack on the
// legacy wire; TERMINATE closes the session and is therefore tested
// last in its own KAT.
void TestStubHandlerAcks()
{
    std::printf("[tloginsvr_asio stub-handler acks — create/del/start/veteran]\n");

    asio::io_context server_io;
    tloginsvr::LoginServer server(server_io, 0);
    const std::uint16_t port = server.Port();
    Check(port != 0, "server bound");
    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    asio::io_context client_io;
    tcp::socket client_sock(client_io);
    client_sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));
    auto client_sess = std::make_shared<tnetlib::AsioSession>(
        std::move(client_sock), tnetlib::PeerType::Server);

    std::atomic<int> ack_count{0};
    std::vector<std::pair<std::uint16_t, std::uint8_t>> ack_info; // wId + first byte (= result)

    asio::co_spawn(client_io,
        [client_sess, &ack_count, &ack_info]() -> asio::awaitable<void> {
            co_await client_sess->RunPackets(
                [&ack_count, &ack_info](const tnetlib::DecodedPacket& pkt) {
                    const std::uint8_t first =
                        pkt.body.empty() ? 0xFF
                                         : static_cast<std::uint8_t>(pkt.body[0]);
                    ack_info.push_back({pkt.wId, first});
                    ack_count.fetch_add(1);
                });
        },
        asio::detached);

    asio::co_spawn(client_io,
        [client_sess]() -> asio::awaitable<void> {
            using namespace tnetlib::protocol;
            // Minimal bodies — handlers don't validate beyond the
            // first few bytes in stub mode.
            std::byte dummy[16] = {};
            co_await client_sess->SendPacket(
                ToUint16(MessageId::CS_CREATECHAR_REQ),
                std::span<const std::byte>(dummy, 16));
            co_await client_sess->SendPacket(
                ToUint16(MessageId::CS_DELCHAR_REQ),
                std::span<const std::byte>(dummy, 9));
            co_await client_sess->SendPacket(
                ToUint16(MessageId::CS_START_REQ),
                std::span<const std::byte>(dummy, 6));
            co_await client_sess->SendPacket(
                ToUint16(MessageId::CS_VETERAN_REQ),
                std::span<const std::byte>{});
        },
        asio::detached);

    std::thread client_thread([&client_io] { client_io.run(); });
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (ack_count.load() < 4 &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    Check(ack_count.load() == 4, "all 4 stub-handler acks received");
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToUint16;
    if (ack_info.size() == 4)
    {
        Check(ack_info[0].first == ToUint16(MessageId::CS_CREATECHAR_ACK)
              && ack_info[0].second == 7,
              "CREATECHAR_ACK with CR_INTERNAL (7)");
        Check(ack_info[1].first == ToUint16(MessageId::CS_DELCHAR_ACK)
              && ack_info[1].second == 3,
              "DELCHAR_ACK with DR_INTERNAL (3)");
        Check(ack_info[2].first == ToUint16(MessageId::CS_START_ACK)
              && ack_info[2].second == 1,
              "START_ACK with SR_NOSERVER (1)");
        Check(ack_info[3].first == ToUint16(MessageId::CS_VETERAN_ACK)
              && ack_info[3].second == 0,
              "VETERAN_ACK with bOption=0 (no bonus)");
    }

    client_sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    server_io.stop();
    if (server_thread.joinable()) server_thread.join();
}

int main()
{
    std::printf("=== tloginsvr_asio handshake test ===\n");
    try
    {
        TestLoginHandshake();
        TestLobbyFlow();
        TestStubHandlerAcks();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
