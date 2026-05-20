// Regression for P-6 (TPATCH_AUDIT): the patch server's
// stale-client sweep. Exercises PatchServer::SweepStaleClients
// directly with real loopback sockets so the sweep sees IsOpen() ==
// true and acts on the actual age + server-peer-flag predicates.
//
// Why loopback instead of mocked sockets: the sweep uses
// PatchSession::IsOpen() / Close() which both touch the underlying
// tcp::socket. A default-constructed socket reports !IsOpen() so the
// sweep would skip it — to test the real predicate we need a socket
// the OS considers open.

#include "patch_server.h"
#include "patch_session.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>

namespace asio = boost::asio;
using namespace std::chrono_literals;

namespace {

int g_passed = 0;
int g_failed = 0;
void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

// Connect a loopback client to `acc.local_endpoint()` and accept on
// `acc`. Returns the server-side socket (open, peer connected).
asio::ip::tcp::socket
LoopbackAccept(asio::io_context& io, asio::ip::tcp::acceptor& acc)
{
    asio::ip::tcp::socket client(io);
    client.connect(acc.local_endpoint());
    asio::ip::tcp::socket server(io);
    acc.accept(server);
    // We don't need the client side past accept — leak it as a
    // detached open socket so the server side stays peered. We
    // shutdown+close at scope end via the destructor; an in-progress
    // sweep close on the server side has nothing to flush.
    (void)client.native_handle();
    return server;
}

void TestSweepClosesOnlyStaleNonServerSessions()
{
    std::printf("[stale_sweep — only stale non-server peers closed]\n");

    asio::io_context io;
    // Helper acceptor solely to manufacture peered sockets.
    asio::ip::tcp::acceptor helper_acc(io,
        asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));

    tpatchsvr::PatchServerConfig cfg{};
    cfg.port = 0;  // ephemeral; PatchServer ctor binds its own listener
    tpatchsvr::PatchServer server(io, cfg);

    // Build three sessions:
    //  1. stale + client     — should be closed by the sweep
    //  2. stale + server     — exempt (MarkAsServerPeer)
    //  3. fresh + client     — too young, must survive a 50ms cap
    auto s_stale_client  = std::make_shared<tpatchsvr::PatchSession>(
        LoopbackAccept(io, helper_acc));
    auto s_stale_server  = std::make_shared<tpatchsvr::PatchSession>(
        LoopbackAccept(io, helper_acc));
    s_stale_server->MarkAsServerPeer();

    server.Register(s_stale_client);
    server.Register(s_stale_server);

    // Let those two age past the cap.
    std::this_thread::sleep_for(80ms);

    // Now a fresh session — `connected_at` is "now".
    auto s_fresh = std::make_shared<tpatchsvr::PatchSession>(
        LoopbackAccept(io, helper_acc));
    server.Register(s_fresh);

    const auto closed = server.SweepStaleClients(50ms);

    Check(closed == 1,
        "SweepStaleClients(50ms) returns 1 (only stale_client matches)");
    Check(!s_stale_client->IsOpen(),
        "stale + non-server peer is closed");
    Check(s_stale_server->IsOpen(),
        "stale + server peer survives (exempt)");
    Check(s_fresh->IsOpen(),
        "fresh client (age < cap) survives");
}

void TestSweepOnEmptyRegistryReturnsZero()
{
    std::printf("[stale_sweep — empty registry returns 0]\n");

    asio::io_context io;
    tpatchsvr::PatchServerConfig cfg{};
    cfg.port = 0;
    tpatchsvr::PatchServer server(io, cfg);

    Check(server.SweepStaleClients(1ms) == 0,
        "empty registry → 0 closed");
}

void TestSweepIdempotentOnAlreadyClosedSessions()
{
    std::printf("[stale_sweep — idempotent on already-closed sessions]\n");

    asio::io_context io;
    asio::ip::tcp::acceptor helper_acc(io,
        asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));

    tpatchsvr::PatchServerConfig cfg{};
    cfg.port = 0;
    tpatchsvr::PatchServer server(io, cfg);

    auto sess = std::make_shared<tpatchsvr::PatchSession>(
        LoopbackAccept(io, helper_acc));
    server.Register(sess);
    std::this_thread::sleep_for(80ms);

    // First sweep closes it.
    Check(server.SweepStaleClients(50ms) == 1,
        "first sweep closes the only stale session");
    Check(!sess->IsOpen(), "session is closed");

    // Second sweep: the session is still registered but !IsOpen,
    // so the predicate skips it. Return value should be 0.
    Check(server.SweepStaleClients(50ms) == 0,
        "second sweep skips already-closed sessions");
}

void TestUnregisterRemovesFromRegistry()
{
    std::printf("[stale_sweep — Unregister wipes the entry]\n");

    asio::io_context io;
    asio::ip::tcp::acceptor helper_acc(io,
        asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 0));

    tpatchsvr::PatchServerConfig cfg{};
    cfg.port = 0;
    tpatchsvr::PatchServer server(io, cfg);

    auto sess = std::make_shared<tpatchsvr::PatchSession>(
        LoopbackAccept(io, helper_acc));
    server.Register(sess);
    server.Unregister(sess.get());

    std::this_thread::sleep_for(80ms);
    Check(server.SweepStaleClients(50ms) == 0,
        "unregistered session is invisible to sweep");
    Check(sess->IsOpen(),
        "Unregister does NOT close the underlying socket");
}

} // namespace

int main()
{
    std::printf("=== tpatchsvr_asio stale-client sweep test ===\n");

    try
    {
        TestSweepClosesOnlyStaleNonServerSessions();
        TestSweepOnEmptyRegistryReturnsZero();
        TestSweepIdempotentOnAlreadyClosedSessions();
        TestUnregisterRemovesFromRegistry();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }

    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
