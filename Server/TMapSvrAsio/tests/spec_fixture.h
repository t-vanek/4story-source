#pragma once

// Shared infrastructure for characterization spec tests.
//
// Each `tests/test_<handler>_spec.cpp` follows the same shape:
//   1. Spin up MapServer on an ephemeral port.
//   2. Open a loopback client socket wrapped in AsioSession.
//   3. Send one (or a few) wire-faithful packet(s).
//   4. Collect any ACKs that come back.
//   5. Assert against the legacy-documented expectation.
//
// This header factors out the boilerplate so each spec file is just
// the per-branch assertions + setup. The harness is intentionally
// barebones — no test framework dep, just `Check` / `Pending` counters
// printed to stdout, return code = (failed != 0).

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
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace tmapsvr::spec {

// =====================================================================
// Counters + result printers. The test main() reads g_failed for the
// process exit code; passed/pending are informational.
// =====================================================================

inline int g_passed  = 0;
inline int g_failed  = 0;
inline int g_pending = 0;

inline void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS     %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL     %s\n", label); }
}

inline void Pending(const char* label, const char* legacy_ref)
{
    ++g_pending;
    std::printf("  PENDING  %s   (%s)\n", label, legacy_ref);
}

inline int ResultExitCode()
{
    std::printf("\nResults: %d passed, %d failed, %d pending\n",
        g_passed, g_failed, g_pending);
    return g_failed == 0 ? 0 : 1;
}

// =====================================================================
// ServerFixture — RAII MapServer on an ephemeral port, driven by an
// internal io_context thread. Destructor stops everything cleanly.
// =====================================================================

struct ServerFixture
{
    boost::asio::io_context                       io;
    FakeMapSessionValidator                       validator;
    std::unique_ptr<MapServer>                    server;
    std::thread                                   thread;

    // `accept_all`        — fake validator's allow-all switch.
    // `versions`          — wire-version gate; empty → reject all.
    // `on_quit_request`   — wired into HandlerContext; null = no-op.
    explicit ServerFixture(bool accept_all,
                           std::vector<std::uint16_t> versions = { 0x2918 },
                           std::function<void()> on_quit_request = {})
    {
        validator.SetAcceptAll(accept_all);

        MapServerConfig cfg{};
        cfg.port              = 0;  // ephemeral
        cfg.validator         = &validator;
        cfg.accepted_versions = std::move(versions);
        cfg.pre_auth_timeout  = std::chrono::seconds(10);
        cfg.on_quit_request   = std::move(on_quit_request);

        server = std::make_unique<MapServer>(io, cfg);

        boost::asio::co_spawn(io, server->Run(), boost::asio::detached);
        thread = std::thread([this] { io.run(); });
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ~ServerFixture()
    {
        io.stop();
        if (thread.joinable()) thread.join();
    }

    std::uint16_t Port() const { return server->Port(); }
};

// =====================================================================
// AckCapture — collected per-test wire response.
//   `wId` is the message id of each received packet (in order).
//   `bodies` is the raw body (caller decodes per-handler shape).
//   `socket_closed_by_peer` is true if the server closed the connection
//   during the wait window.
// =====================================================================

struct AckCapture
{
    std::vector<std::uint16_t>              wIds;
    std::vector<std::vector<std::byte>>     bodies;
    bool                                    socket_closed_by_peer = false;
};

// Connect a loopback client, send one or more packets through the
// supplied builder, collect any inbound traffic for `wait_for`.
// Builder receives the AsioSession and is expected to issue async
// `co_await sess->SendPacket(...)` calls in any order. Use a builder
// instead of a vector of bodies so tests can interleave multiple
// packet IDs or wait between sends.
inline AckCapture
RunClient(std::uint16_t server_port,
          std::function<boost::asio::awaitable<void>(
              std::shared_ptr<tnetlib::AsioSession>)> builder,
          std::chrono::milliseconds wait_for = std::chrono::milliseconds(300))
{
    namespace asio = boost::asio;
    using boost::asio::ip::tcp;

    AckCapture out;
    asio::io_context client_io;
    tcp::socket sock(client_io);
    boost::system::error_code ec;
    sock.connect(
        tcp::endpoint(asio::ip::make_address_v4("127.0.0.1"), server_port),
        ec);
    if (ec) return out;

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(sock), tnetlib::PeerType::Server);

    std::atomic<bool> closed{ false };

    asio::co_spawn(client_io,
        [sess, &out, &closed]() -> asio::awaitable<void> {
            try {
                co_await sess->RunPackets(
                    [&out](const tnetlib::DecodedPacket& pkt) {
                        out.wIds.push_back(pkt.wId);
                        out.bodies.emplace_back(
                            pkt.body.begin(), pkt.body.end());
                    });
            } catch (...) {}
            closed.store(true);
        },
        asio::detached);

    asio::co_spawn(client_io, builder(sess), asio::detached);

    std::thread t([&client_io] { client_io.run(); });
    const auto deadline = std::chrono::steady_clock::now() + wait_for;
    while (!closed.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Capture peer-close BEFORE the self-Close — otherwise our own
    // Close() flips `closed` via the receive coroutine's catch block
    // and the "server held the socket open" assertion becomes
    // impossible to express.
    out.socket_closed_by_peer = closed.load();

    sess->Close();
    client_io.stop();
    if (t.joinable()) t.join();
    return out;
}

// Convenience for the dominant case: send a single packet with the
// given id + body, capture the response.
inline AckCapture
RunSinglePacket(std::uint16_t server_port,
                std::uint16_t wId,
                const std::vector<std::byte>& body,
                std::chrono::milliseconds wait_for = std::chrono::milliseconds(300))
{
    return RunClient(server_port,
        [wId, body](std::shared_ptr<tnetlib::AsioSession> sess)
            -> boost::asio::awaitable<void>
        {
            co_await sess->SendPacket(wId,
                std::span<const std::byte>(body.data(), body.size()));
        },
        wait_for);
}

} // namespace tmapsvr::spec
