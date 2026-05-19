// Regression for the CT_* peer-type gate (round 2 #1).
//
// Scenario:
//   * Server boots with `[server] control_server_ip = "10.0.0.99"`.
//   * A client connects from 127.0.0.1 (loopback) and fires
//     CT_EVENTUPDATE_REQ aimed at writing event_id=42 into the
//     registry.
//   * The dispatcher must drop the packet because the peer IP
//     doesn't match control_server_ip — registry stays empty.
//
// Mirror scenario with control_server_ip = "" verifies the gate
// is open by default (in-process tests + dev mode).

#include "../login_server.h"
#include "asio_session.h"
#include "MessageId.h"
#include "services/local_event_registry.h"

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

// CT_EVENTUPDATE_REQ wire body: BYTE bEventID, WORD wValue,
// EVENTINFO struct. We send the minimum that survives the
// parser-failure path so the entry lands as parsed=false but
// still gets Upsert'd into the registry.
std::vector<std::byte> MakeMinimalEventUpdate(std::uint8_t event_id,
                                              std::uint16_t value)
{
    std::vector<std::byte> body;
    body.push_back(std::byte{event_id});
    body.push_back(std::byte{static_cast<std::uint8_t>(value & 0xFF)});
    body.push_back(std::byte{static_cast<std::uint8_t>((value >> 8) & 0xFF)});
    // No EVENTINFO tail — ParseEventInfo returns false but the
    // handler still calls Upsert with parsed=false + empty opaque.
    return body;
}

// Run a single CT_EVENTUPDATE_REQ exchange against a LoginServer
// configured with the given control_server_ip. Returns the registry
// size after the round trip.
std::size_t RunCtEventUpdate(const std::string& control_ip)
{
    asio::io_context server_io;
    tloginsvr::services::LocalEventRegistry registry;

    tloginsvr::LoginServerConfig cfg{};
    cfg.port = 0;
    cfg.event_registry = &registry;
    cfg.control_server_ip = control_ip;
    cfg.test_handlers_enabled = false;

    tloginsvr::LoginServer server(server_io, cfg);
    const std::uint16_t port = server.Port();
    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Client side — peer at 127.0.0.1, which is NOT 10.0.0.99
    // (the gated case) but IS allowed when control_ip is empty.
    asio::io_context client_io;
    tcp::socket client_sock(client_io);
    client_sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));
    auto client = std::make_shared<tnetlib::AsioSession>(
        std::move(client_sock), tnetlib::PeerType::Server);

    auto send_coro = [client]() -> asio::awaitable<void> {
        const auto body = MakeMinimalEventUpdate(42, 1);
        co_await client->SendPacket(
            tnetlib::protocol::ToUint16(
                tnetlib::protocol::MessageId::CT_EVENTUPDATE_REQ),
            std::span<const std::byte>(body.data(), body.size()));
    };
    asio::co_spawn(client_io, send_coro(), asio::detached);
    std::thread client_thread([&client_io] { client_io.run(); });

    // The handler is fire-and-forget (no ack on EVENTUPDATE_REQ);
    // give the io_contexts a moment to process the packet.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const auto count = registry.Snapshot().size();

    server_io.stop();
    client_io.stop();
    if (server_thread.joinable()) server_thread.join();
    if (client_thread.joinable()) client_thread.join();
    return count;
}

void TestGateBlocksNonControlPeer()
{
    std::printf("[control_gate — peer mismatch drops CT_EVENTUPDATE_REQ]\n");
    const auto size = RunCtEventUpdate("10.0.0.99");
    Check(size == 0, "event registry empty (packet was dropped)");
}

void TestGateOpenAllowsLoopback()
{
    std::printf("[control_gate — empty config allows CT_EVENTUPDATE_REQ]\n");
    const auto size = RunCtEventUpdate("");
    Check(size == 1, "event registry holds one entry");
}

} // namespace

int main()
{
    TestGateBlocksNonControlPeer();
    TestGateOpenAllowsLoopback();

    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
