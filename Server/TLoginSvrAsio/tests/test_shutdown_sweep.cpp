// Regression for round-2 #4: shutdown bulk-logout. Exercises the
// IConnectionRegistry::Snapshot + ISessionTerminator::Terminate
// chain that main.cpp's graceful_shutdown lambda drives on SIGINT/
// SIGTERM/SM_QUITSERVICE_REQ. Without this sweep every live session
// leaves a stale TCURRENTUSER row until the next boot's
// ClearStaleSessions kicks in.

#include "services/local_connection_registry.h"
#include "services/fake_session_terminator.h"
#include "asio_session.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <algorithm>
#include <cstdio>
#include <memory>
#include <vector>

namespace asio = boost::asio;

namespace {

int g_passed = 0;
int g_failed = 0;
void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

// Build a "session" we can register: a real AsioSession over an
// unbound TCP socket. The shutdown sweep doesn't touch the socket,
// only the registry entry + terminator. PeerType::Server skips the
// RC4 handshake bookkeeping.
std::shared_ptr<tnetlib::AsioSession> MakeFakeSession(asio::io_context& io)
{
    asio::ip::tcp::socket sock(io);
    return std::make_shared<tnetlib::AsioSession>(
        std::move(sock), tnetlib::PeerType::Server);
}

void TestShutdownSweepHitsEveryLiveSession()
{
    std::printf("[shutdown_sweep — every live session gets Terminate]\n");

    asio::io_context io;
    tloginsvr::services::LocalConnectionRegistry registry;
    tloginsvr::services::FakeSessionTerminator   terminator;

    // Seed three "live" sessions with distinct user_id + session_key.
    auto s1 = MakeFakeSession(io);
    auto s2 = MakeFakeSession(io);
    auto s3 = MakeFakeSession(io);
    registry.Register({.user_id = 101, .session_key = 1001, .agreed = true}, s1);
    registry.Register({.user_id = 202, .session_key = 1002, .agreed = true}, s2);
    registry.Register({.user_id = 303, .session_key = 1003, .agreed = true}, s3);
    Check(registry.Count() == 3, "three sessions registered");

    // This is the body of graceful_shutdown in main.cpp — Snapshot +
    // Terminate(Disconnect) for each. Keeping the loop inline so the
    // test asserts exactly the shape main.cpp's lambda performs.
    const auto live = registry.Snapshot();
    Check(live.size() == 3, "snapshot returns all three");
    for (const auto& it : live)
    {
        terminator.Terminate(it.entry.user_id, it.entry.session_key,
            tloginsvr::services::TerminationReason::Disconnect);
    }

    const auto history = terminator.History();
    Check(history.size() == 3, "three Terminate calls recorded");

    // Order isn't guaranteed (registry walks an unordered_map). Sort
    // by user_id and compare against the expected sequence.
    std::vector<std::int32_t> uids;
    uids.reserve(history.size());
    for (const auto& r : history) uids.push_back(r.user_id);
    std::sort(uids.begin(), uids.end());
    Check(uids == std::vector<std::int32_t>{101, 202, 303},
        "every registered uid received Terminate");

    for (const auto& r : history)
    {
        Check(r.reason == tloginsvr::services::TerminationReason::Disconnect,
            "reason == Disconnect");
    }
}

void TestShutdownSweepOnEmptyRegistryIsHarmless()
{
    std::printf("[shutdown_sweep — empty registry is a no-op]\n");
    tloginsvr::services::LocalConnectionRegistry registry;
    tloginsvr::services::FakeSessionTerminator   terminator;

    const auto live = registry.Snapshot();
    Check(live.empty(), "empty snapshot");
    for (const auto& it : live)
    {
        terminator.Terminate(it.entry.user_id, it.entry.session_key,
            tloginsvr::services::TerminationReason::Disconnect);
    }
    Check(terminator.Count() == 0, "no Terminate calls made");
}

} // namespace

int main()
{
    TestShutdownSweepHitsEveryLiveSession();
    TestShutdownSweepOnEmptyRegistryIsHarmless();

    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
