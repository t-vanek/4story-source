// Characterization test for SM_QUITSERVICE_REQ.
//
// Source of truth: Server/TMapSvr/SSHandler.cpp:9-23
//
// Wire shape: empty body. SM_QUITSERVICE_REQ is the internal cluster
// signal that a TControlSvr admin sent to shut down a single Map
// shard. The legacy handler logs the event, transitions the Windows
// service to STOP_PENDING (only if running as a service), then posts
// WM_QUIT to the main thread — the same effect as SIGTERM in a POSIX
// service.
//
// Branches in legacy OnSM_QUITSERVICE_REQ:
//
//   §1  SSHandler.cpp:14  always — log "SM_QUITSERVICE_REQ detected !!"
//       → modern: ACTIVE (spdlog::info equivalent)
//
//   §2  SSHandler.cpp:15-16  m_bService == TRUE
//       → SetServiceStatus(SERVICE_STOP_PENDING). Windows-only;
//         the Linux modern build runs as a plain daemon under
//         systemd, which gets the stop signal directly. We log
//         the equivalent state transition.
//       → modern: ACTIVE (best-effort log, no SCM call)
//
//   §3  SSHandler.cpp:17  always — PostThreadMessage WM_QUIT
//       → modern: ACTIVE — invoke `on_quit_request` callback if
//         configured. main.cpp wires the callback to `io.stop()`.

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
// §1+§3  SM_QUITSERVICE_REQ → log + on_quit_request fires
// =====================================================================
void TestQuitServiceTriggersCallback()
{
    std::printf("[§1+§3 SM_QUITSERVICE_REQ → on_quit_request fires  "
                "(SSHandler.cpp:14, :17)]\n");

    asio::io_context io;
    tmapsvr::FakeMapSessionValidator validator;
    validator.SetAcceptAll(true);

    std::atomic<int> quit_calls{ 0 };

    tmapsvr::MapServerConfig cfg{};
    cfg.port              = 0;
    cfg.validator         = &validator;
    cfg.accepted_versions = { 0x2918 };
    cfg.pre_auth_timeout  = std::chrono::seconds(10);
    cfg.on_quit_request   = [&quit_calls] { quit_calls.fetch_add(1); };

    tmapsvr::MapServer server(io, cfg);
    const auto port = server.Port();

    asio::co_spawn(io, server.Run(), asio::detached);
    std::thread srv_thread([&io] { io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    asio::io_context client_io;
    tcp::socket sock(client_io);
    sock.connect(
        tcp::endpoint(asio::ip::make_address_v4("127.0.0.1"), port));

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(sock), tnetlib::PeerType::Server);

    asio::co_spawn(client_io,
        [sess]() -> asio::awaitable<void> {
            try {
                co_await sess->RunPackets(
                    [](const tnetlib::DecodedPacket&) {});
            } catch (...) {}
        },
        asio::detached);

    asio::co_spawn(client_io,
        [sess]() -> asio::awaitable<void> {
            std::vector<std::byte> empty_body;
            co_await sess->SendPacket(
                ToUint16(MessageId::SM_QUITSERVICE_REQ),
                std::span<const std::byte>(empty_body.data(),
                                            empty_body.size()));
        },
        asio::detached);

    std::thread client_thread([&client_io] { client_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    Check(quit_calls.load() == 1,
        "on_quit_request invoked exactly once");

    sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    io.stop();
    if (srv_thread.joinable()) srv_thread.join();
}

// =====================================================================
// §1+§3  with no callback wired — must not crash
// =====================================================================
void TestQuitServiceWithoutCallback()
{
    std::printf("[§1+§3 SM_QUITSERVICE_REQ with no callback → silent log]\n");

    asio::io_context io;
    tmapsvr::FakeMapSessionValidator validator;
    validator.SetAcceptAll(true);

    tmapsvr::MapServerConfig cfg{};
    cfg.port              = 0;
    cfg.validator         = &validator;
    cfg.accepted_versions = { 0x2918 };
    cfg.pre_auth_timeout  = std::chrono::seconds(10);
    // on_quit_request intentionally left nullptr.

    tmapsvr::MapServer server(io, cfg);
    const auto port = server.Port();

    asio::co_spawn(io, server.Run(), asio::detached);
    std::thread srv_thread([&io] { io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    asio::io_context client_io;
    tcp::socket sock(client_io);
    sock.connect(
        tcp::endpoint(asio::ip::make_address_v4("127.0.0.1"), port));

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(sock), tnetlib::PeerType::Server);

    asio::co_spawn(client_io,
        [sess]() -> asio::awaitable<void> {
            try {
                co_await sess->RunPackets(
                    [](const tnetlib::DecodedPacket&) {});
            } catch (...) {}
        },
        asio::detached);

    asio::co_spawn(client_io,
        [sess]() -> asio::awaitable<void> {
            std::vector<std::byte> empty_body;
            co_await sess->SendPacket(
                ToUint16(MessageId::SM_QUITSERVICE_REQ),
                std::span<const std::byte>(empty_body.data(),
                                            empty_body.size()));
        },
        asio::detached);

    std::thread client_thread([&client_io] { client_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    Check(true, "no crash with empty on_quit_request");

    sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    io.stop();
    if (srv_thread.joinable()) srv_thread.join();
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnSM_QUITSERVICE_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/SSHandler.cpp:9-23\n\n");
    try
    {
        TestQuitServiceTriggersCallback();
        TestQuitServiceWithoutCallback();
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
