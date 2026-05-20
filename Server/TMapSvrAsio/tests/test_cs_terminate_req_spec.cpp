// Characterization test for CS_TERMINATE_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:14876-14918
//
// Wire shape: empty body. The legacy handler historically held a
// "TLogoutAll" magic-key check that allowed an authenticated admin to
// shut down the server cluster from the wire. The shipped source has
// the body stripped (lines 14883-14917 are blank); all that remains is
// the IP log + drop.
//
// Branches in legacy OnCS_TERMINATE_REQ:
//
//   §1  CSHandler.cpp:14878-14882  always
//       → log "Backdoor attack from IP address: %s" + return EC_NOERROR
//       → modern: ACTIVE
//
// No CS_TERMINATE_ACK is sent. The packet is purely a server-side
// telemetry hook for spotting attackers probing the legacy magic.
// Returning EC_NOERROR (vs EC_SESSION_INVALIDCHAR) means the session
// stays alive — legacy doesn't close on this packet.

#include "handlers.h"
#include "map_server.h"
#include "services/fake_session_validator.h"
#include "wire_codec.h"

#include "asio_session.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

namespace asio = boost::asio;
using boost::asio::ip::tcp;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

int g_passed  = 0;
int g_failed  = 0;
int g_pending = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS     %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL     %s\n", label); }
}

// =====================================================================
// §1  always → log + EC_NOERROR (no ACK, session survives)
// =====================================================================
void TestBackdoorLogged()
{
    std::printf("[§1 backdoor log + drop  (CSHandler.cpp:14876-14918)]\n");

    asio::io_context io;
    tmapsvr::FakeMapSessionValidator validator;
    validator.SetAcceptAll(true);

    tmapsvr::MapServerConfig cfg{};
    cfg.port              = 0;
    cfg.validator         = &validator;
    cfg.accepted_versions = { 0x2918 };
    // CS_TERMINATE_REQ arrives before any CONNECT, so the pre-auth
    // watchdog would close us out if we wait too long for the
    // server-side log to flush. Bump the timeout for this test.
    cfg.pre_auth_timeout  = std::chrono::seconds(10);

    tmapsvr::MapServer server(io, cfg);
    const auto port = server.Port();

    asio::co_spawn(io, server.Run(), asio::detached);
    std::thread srv_thread([&io] { io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Send CS_TERMINATE_REQ with an empty body. Expect NO ack back and
    // the session to stay open for at least 200ms (proves legacy's
    // EC_NOERROR semantic — handler doesn't close).
    asio::io_context client_io;
    tcp::socket sock(client_io);
    sock.connect(
        tcp::endpoint(asio::ip::make_address_v4("127.0.0.1"), port));

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(sock), tnetlib::PeerType::Server);

    std::atomic<int>  received_count{ 0 };
    std::atomic<bool> closed{ false };

    asio::co_spawn(client_io,
        [sess, &received_count, &closed]() -> asio::awaitable<void> {
            try {
                co_await sess->RunPackets(
                    [&received_count](const tnetlib::DecodedPacket&) {
                        received_count.fetch_add(1);
                    });
            } catch (...) {}
            closed.store(true);
        },
        asio::detached);

    asio::co_spawn(client_io,
        [sess]() -> asio::awaitable<void> {
            std::vector<std::byte> empty_body;  // CS_TERMINATE_REQ has no payload
            co_await sess->SendPacket(
                ToUint16(MessageId::CS_TERMINATE_REQ),
                std::span<const std::byte>(empty_body.data(), empty_body.size()));
        },
        asio::detached);

    std::thread client_thread([&client_io] { client_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    Check(received_count.load() == 0,
        "no CS_TERMINATE_ACK sent (legacy returns EC_NOERROR without ack)");
    Check(!closed.load(),
        "session stays open after CS_TERMINATE_REQ (no server-side close)");

    sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    io.stop();
    if (srv_thread.joinable()) srv_thread.join();
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_TERMINATE_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:14876-14918\n\n");
    try
    {
        TestBackdoorLogged();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed, %d pending\n",
        g_passed, g_failed, g_pending);
    return g_failed == 0 ? 0 : 1;
}
