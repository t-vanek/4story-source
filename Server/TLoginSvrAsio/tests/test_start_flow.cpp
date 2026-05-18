// End-to-end test for CS_START_REQ wired through IMapServerLocator.
// Two scenarios:
//   1. Group has a registered Map endpoint → SR_SUCCESS + correct
//      IPv4 octets + port + server_id on the wire.
//   2. Group has nothing registered → SR_NOSERVER, body zeroed.

#include "../login_server.h"
#include "../services/fake_map_server_locator.h"
#include "asio_session.h"
#include "MessageId.h"

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

// One CS_START_REQ → CS_START_ACK round trip. Returns the 8-byte
// ack body (zero-padded if short).
struct StartAck
{
    bool                       ack_seen = false;
    std::array<std::byte, 8>   body{};
};

StartAck SendStartAndCapture(std::uint16_t port,
                             std::uint8_t group_id,
                             std::uint8_t channel,
                             std::int32_t char_id)
{
    asio::io_context client_io;
    tcp::socket sock(client_io);
    sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(sock), tnetlib::PeerType::Server);

    std::atomic<bool> ack_seen{false};
    std::array<std::byte, 8> capture{};

    asio::co_spawn(client_io,
        [sess, &ack_seen, &capture]() -> asio::awaitable<void> {
            co_await sess->RunPackets(
                [&ack_seen, &capture](const tnetlib::DecodedPacket& pkt) {
                    if (pkt.wId == tnetlib::protocol::ToUint16(
                            tnetlib::protocol::MessageId::CS_START_ACK))
                    {
                        const auto n = std::min<std::size_t>(8, pkt.body.size());
                        std::memcpy(capture.data(), pkt.body.data(), n);
                        ack_seen.store(true);
                    }
                });
        },
        asio::detached);

    asio::co_spawn(client_io,
        [sess, group_id, channel, char_id]() -> asio::awaitable<void> {
            std::vector<std::byte> body(6);
            body[0] = static_cast<std::byte>(group_id);
            body[1] = static_cast<std::byte>(channel);
            std::memcpy(body.data() + 2, &char_id, 4);
            co_await sess->SendPacket(
                tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_START_REQ),
                std::span<const std::byte>(body.data(), body.size()));
        },
        asio::detached);

    std::thread t([&client_io] { client_io.run(); });
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!ack_seen.load() &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    sess->Close();
    client_io.stop();
    if (t.joinable()) t.join();
    return StartAck{ ack_seen.load(), capture };
}

void TestStartFlow()
{
    std::printf("[CS_START_REQ via IMapServerLocator — hit + miss]\n");

    auto locator =
        std::make_unique<tloginsvr::services::FakeMapServerLocator>();
    // Group 1 maps to 192.168.42.7:5815 server_id=2.
    locator->AddMapServer(1, tloginsvr::services::MapEndpoint{
        .ipv4 = {192, 168, 42, 7},
        .port = 5815,
        .server_id = 2,
    });

    asio::io_context server_io;
    tloginsvr::LoginServerConfig cfg{};
    cfg.port = 0;
    cfg.map_server_locator = locator.get();
    tloginsvr::LoginServer server(server_io, cfg);
    const std::uint16_t port = server.Port();
    Check(port != 0, "server bound");

    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 1. Hit — group 1 is registered.
    const auto hit = SendStartAndCapture(port, /*group=*/1, /*ch=*/0, /*char=*/42);
    Check(hit.ack_seen, "group 1: ack received");
    Check(static_cast<std::uint8_t>(hit.body[0]) == 0,
          "group 1: bResult == SR_SUCCESS (0)");
    Check(static_cast<std::uint8_t>(hit.body[1]) == 192 &&
          static_cast<std::uint8_t>(hit.body[2]) == 168 &&
          static_cast<std::uint8_t>(hit.body[3]) == 42 &&
          static_cast<std::uint8_t>(hit.body[4]) == 7,
          "group 1: IPv4 octets match 192.168.42.7");
    std::uint16_t port_on_wire = 0;
    std::memcpy(&port_on_wire, &hit.body[5], 2);
    Check(port_on_wire == 5815, "group 1: wPort matches 5815");
    Check(static_cast<std::uint8_t>(hit.body[7]) == 2,
          "group 1: bServerID matches 2");

    // 2. Miss — group 99 isn't registered.
    const auto miss = SendStartAndCapture(port, /*group=*/99, /*ch=*/0, /*char=*/1);
    Check(miss.ack_seen, "group 99: ack received");
    Check(static_cast<std::uint8_t>(miss.body[0]) == 1,
          "group 99: bResult == SR_NOSERVER (1)");
    // Body tail should be zero — defensive check, the wire payload
    // is meaningless on SR_NOSERVER but the legacy client may still
    // inspect it.
    Check(static_cast<std::uint8_t>(miss.body[1]) == 0 &&
          static_cast<std::uint8_t>(miss.body[2]) == 0 &&
          static_cast<std::uint8_t>(miss.body[3]) == 0 &&
          static_cast<std::uint8_t>(miss.body[4]) == 0,
          "group 99: IPv4 octets zeroed");

    server_io.stop();
    if (server_thread.joinable()) server_thread.join();
}

} // namespace

int main()
{
    std::printf("=== tloginsvr_asio start-flow test ===\n");
    try
    {
        TestStartFlow();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
