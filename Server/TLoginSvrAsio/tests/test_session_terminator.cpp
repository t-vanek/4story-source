// End-to-end test for the close-time cleanup chain:
//   1. Client logs in → registry entry stamped with user_id + session_key
//   2. Client maybe calls CS_START_REQ → registry's handoff_to_map flips
//   3. Client socket closes → HandleConnection's post-RunPackets path:
//        - lookup registry entry
//        - call SessionTerminator.Terminate(entry.user_id, entry.session_key, reason)
//        - unregister
//
// Two scenarios:
//   A. plain disconnect          → terminator called with Disconnect reason
//   B. login + START_REQ success → terminator called with MapHandoff reason

#include "../login_server.h"
#include "../services/fake_auth_service.h"
#include "../services/local_connection_registry.h"
#include "../services/fake_map_server_locator.h"
#include "../services/fake_session_terminator.h"
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

constexpr std::uint16_t kProtocolVersion = 0x2918;

std::vector<std::byte> MakeLoginReq(const std::string& user,
                                    const std::string& pass)
{
    std::vector<std::byte> out;
    auto bytes = [&](const void* src, std::size_t n) {
        const auto* p = reinterpret_cast<const std::byte*>(src);
        out.insert(out.end(), p, p + n);
    };
    auto str = [&](const std::string& s) {
        std::int32_t len = static_cast<std::int32_t>(s.size());
        bytes(&len, 4);
        bytes(s.data(), s.size());
    };
    std::uint16_t v = kProtocolVersion;
    bytes(&v, 2);
    str(""); str(pass); str(""); str(""); str(user);
    // Legacy checksum (CSHandler.cpp:185-202).
    constexpr std::int64_t kKey = 0x336c3aebf71a8b08LL;
    std::int64_t ck = static_cast<std::int64_t>(v) * 2 - 500;
    const std::int64_t idx = ck % 8, body = ck / 8;
    for (std::int64_t i = 0; i < idx; ++i) { ck ^= body; ck += kKey; }
    std::int64_t dlCheck = 0;
    bytes(&dlCheck, 8);
    bytes(&ck, 8);
    return out;
}

// Runs one client: connect, send LOGIN_REQ, optionally send START_REQ,
// wait for the ack, then close cleanly. Returns when done.
void RunClient(std::uint16_t port,
               const std::string& user,
               const std::string& pass,
               bool send_start_req)
{
    asio::io_context client_io;
    tcp::socket sock(client_io);
    sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(sock), tnetlib::PeerType::Server);

    std::atomic<int> acks_seen{0};
    const int expected_acks = send_start_req ? 2 : 1;

    asio::co_spawn(client_io,
        [sess, &acks_seen]() -> asio::awaitable<void> {
            co_await sess->RunPackets(
                [&acks_seen](const tnetlib::DecodedPacket&) {
                    acks_seen.fetch_add(1);
                });
        },
        asio::detached);

    asio::co_spawn(client_io,
        [sess, user, pass, send_start_req]() -> asio::awaitable<void> {
            const auto login = MakeLoginReq(user, pass);
            co_await sess->SendPacket(
                tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_LOGIN_REQ),
                std::span<const std::byte>(login.data(), login.size()));

            if (send_start_req)
            {
                // Body: bGroupID=1, bChannel=0, dwCharID=42
                std::vector<std::byte> start(6);
                start[0] = std::byte{1};
                start[1] = std::byte{0};
                std::int32_t cid = 42;
                std::memcpy(start.data() + 2, &cid, 4);
                co_await sess->SendPacket(
                    tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_START_REQ),
                    std::span<const std::byte>(start.data(), start.size()));
            }
        },
        asio::detached);

    std::thread t([&client_io] { client_io.run(); });
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (acks_seen.load() < expected_acks &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    sess->Close();
    client_io.stop();
    if (t.joinable()) t.join();
}

void TestPlainDisconnectFiresTerminator()
{
    std::printf("[plain disconnect → SessionTerminator called with Disconnect]\n");

    auto auth = std::make_unique<tloginsvr::services::FakeAuthService>();
    auth->AddUser("alice", "pw", /*db_id=*/1001);
    auto registry = std::make_unique<tloginsvr::services::LocalConnectionRegistry>();
    auto term = std::make_unique<tloginsvr::services::FakeSessionTerminator>();

    asio::io_context server_io;
    tloginsvr::LoginServerConfig cfg{};
    cfg.port = 0;
    cfg.auth_service = auth.get();
    cfg.connection_registry = registry.get();
    cfg.session_terminator = term.get();
    tloginsvr::LoginServer server(server_io, cfg);
    const std::uint16_t port = server.Port();

    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    RunClient(port, "alice", "pw", /*send_start_req=*/false);

    // Server-side close is async — give it a moment.
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (term->Count() == 0 &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const auto history = term->History();
    Check(history.size() == 1, "exactly one terminator call");
    if (history.size() == 1)
    {
        Check(history[0].user_id == 1001, "user_id = 1001");
        Check(history[0].session_key == 1, "session_key = 1 (first session)");
        Check(history[0].reason == tloginsvr::services::TerminationReason::Disconnect,
              "reason = Disconnect");
    }

    server_io.stop();
    if (server_thread.joinable()) server_thread.join();
}

void TestStartReqFlipsToMapHandoff()
{
    std::printf("[LOGIN + START_REQ + close → terminator called with MapHandoff]\n");

    auto auth = std::make_unique<tloginsvr::services::FakeAuthService>();
    auth->AddUser("bob", "pw", /*db_id=*/2002);
    auto registry = std::make_unique<tloginsvr::services::LocalConnectionRegistry>();
    auto term = std::make_unique<tloginsvr::services::FakeSessionTerminator>();
    auto locator = std::make_unique<tloginsvr::services::FakeMapServerLocator>();
    locator->AddMapServer(1, tloginsvr::services::MapEndpoint{
        .ipv4 = {127, 0, 0, 1}, .port = 5815, .server_id = 1});

    asio::io_context server_io;
    tloginsvr::LoginServerConfig cfg{};
    cfg.port = 0;
    cfg.auth_service = auth.get();
    cfg.connection_registry = registry.get();
    cfg.session_terminator = term.get();
    cfg.map_server_locator = locator.get();
    tloginsvr::LoginServer server(server_io, cfg);
    const std::uint16_t port = server.Port();

    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    RunClient(port, "bob", "pw", /*send_start_req=*/true);

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (term->Count() == 0 &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    const auto history = term->History();
    Check(history.size() == 1, "exactly one terminator call");
    if (history.size() == 1)
    {
        Check(history[0].user_id == 2002, "user_id = 2002");
        Check(history[0].reason == tloginsvr::services::TerminationReason::MapHandoff,
              "reason = MapHandoff (CS_START_REQ marked it)");
    }

    server_io.stop();
    if (server_thread.joinable()) server_thread.join();
}

void TestUnauthenticatedSessionDoesNotFireTerminator()
{
    std::printf("[connect + disconnect without login → no terminator call]\n");

    auto registry = std::make_unique<tloginsvr::services::LocalConnectionRegistry>();
    auto term = std::make_unique<tloginsvr::services::FakeSessionTerminator>();

    asio::io_context server_io;
    tloginsvr::LoginServerConfig cfg{};
    cfg.port = 0;
    cfg.connection_registry = registry.get();
    cfg.session_terminator = term.get();
    tloginsvr::LoginServer server(server_io, cfg);
    const std::uint16_t port = server.Port();

    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Connect + immediately close without any packets.
    {
        asio::io_context client_io;
        tcp::socket sock(client_io);
        sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));
        sock.shutdown(tcp::socket::shutdown_both);
        sock.close();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    Check(term->Count() == 0,
          "terminator not called for never-authenticated session");

    server_io.stop();
    if (server_thread.joinable()) server_thread.join();
}

} // namespace

int main()
{
    std::printf("=== tloginsvr_asio session-terminator test ===\n");
    try
    {
        TestPlainDisconnectFiresTerminator();
        TestStartReqFlipsToMapHandoff();
        TestUnauthenticatedSessionDoesNotFireTerminator();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
