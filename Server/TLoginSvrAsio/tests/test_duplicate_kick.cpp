// End-to-end test for the duplicate-session enforcement path.
// Spins up the login server with both IAuthService (in-memory) and
// IConnectionRegistry (in-memory). Two clients log in as the same
// user; the second login should kick the first (TCP socket closed,
// RunPackets loop on that client side exits with EOF).
//
// The first client also receives its LR_SUCCESS ack BEFORE getting
// kicked — the kick is post-success. The second client gets its
// own LR_SUCCESS and remains live.

#include "../login_server.h"
#include "../services/fake_auth_service.h"
#include "../services/local_connection_registry.h"
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
#include <future>
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

std::vector<std::byte> MakeLoginReq(std::uint16_t version,
                                    const std::string& user_id,
                                    const std::string& password)
{
    std::vector<std::byte> out;
    auto append_bytes = [&](const void* src, std::size_t n) {
        const auto* p = reinterpret_cast<const std::byte*>(src);
        out.insert(out.end(), p, p + n);
    };
    auto append_string = [&](const std::string& s) {
        std::int32_t len = static_cast<std::int32_t>(s.size());
        append_bytes(&len, 4);
        append_bytes(s.data(), s.size());
    };
    append_bytes(&version, 2);
    append_string("");
    append_string(password);
    append_string("");
    append_string("");
    append_string(user_id);
    // Legacy checksum (CSHandler.cpp:185-202).
    constexpr std::int64_t kKey = 0x336c3aebf71a8b08LL;
    std::int64_t ck = static_cast<std::int64_t>(version) * 2 - 500;
    const std::int64_t idx = ck % 8, body = ck / 8;
    for (std::int64_t i = 0; i < idx; ++i) { ck ^= body; ck += kKey; }
    std::int64_t dlCheck = 0;
    append_bytes(&dlCheck, 8);
    append_bytes(&ck, 8);
    return out;
}

// Connect, send a LOGIN_REQ for the given user/pass, wait until the
// CS_LOGIN_ACK comes back, then sit on the socket waiting for it to
// close. Returns:
//   { ack_seen, ack_result_byte, socket_closed_within_deadline }
struct ClientOutcome
{
    bool          ack_seen = false;
    std::uint8_t  ack_result = 0xFF;
    bool          got_eof = false;
};

ClientOutcome RunOneClient(std::uint16_t port,
                           const std::string& user,
                           const std::string& pass,
                           std::chrono::milliseconds eof_wait)
{
    asio::io_context client_io;
    tcp::socket sock(client_io);
    sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(sock), tnetlib::PeerType::Server);

    std::atomic<bool> ack_seen{false};
    std::atomic<std::uint8_t> result{0xFF};
    std::atomic<bool> recv_loop_done{false};

    asio::co_spawn(client_io,
        [sess, &ack_seen, &result, &recv_loop_done]() -> asio::awaitable<void> {
            co_await sess->RunPackets(
                [&ack_seen, &result](const tnetlib::DecodedPacket& pkt) {
                    if (pkt.wId == tnetlib::protocol::ToUint16(
                            tnetlib::protocol::MessageId::CS_LOGIN_ACK))
                    {
                        if (!pkt.body.empty())
                            result.store(static_cast<std::uint8_t>(pkt.body[0]));
                        ack_seen.store(true);
                    }
                });
            // RunPackets returns when the socket closes (EOF) or
            // hits an error. Either way the loop is done.
            recv_loop_done.store(true);
        },
        asio::detached);

    asio::co_spawn(client_io,
        [sess, user, pass]() -> asio::awaitable<void> {
            const auto body = MakeLoginReq(kProtocolVersion, user, pass);
            co_await sess->SendPacket(
                tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_LOGIN_REQ),
                std::span<const std::byte>(body.data(), body.size()));
        },
        asio::detached);

    std::thread t([&client_io] { client_io.run(); });

    // Wait for ack first.
    const auto ack_deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!ack_seen.load() &&
           std::chrono::steady_clock::now() < ack_deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Then wait up to eof_wait for the recv loop to exit (server kicks us).
    const auto eof_deadline =
        std::chrono::steady_clock::now() + eof_wait;
    while (!recv_loop_done.load() &&
           std::chrono::steady_clock::now() < eof_deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Snapshot BEFORE we tear down the socket — otherwise our own
    // Close() would set recv_loop_done = true and the caller could
    // never tell whether the server kicked us or we self-closed.
    const bool eof_observed = recv_loop_done.load();

    sess->Close();
    client_io.stop();
    if (t.joinable()) t.join();

    return ClientOutcome{
        .ack_seen = ack_seen.load(),
        .ack_result = result.load(),
        .got_eof = eof_observed,
    };
}

void TestDuplicateKick()
{
    std::printf("[duplicate-kick: second login kicks first session]\n");

    auto auth = std::make_unique<tloginsvr::services::FakeAuthService>();
    auth->AddUser("alice", "hunter2", 1001);

    auto registry =
        std::make_unique<tloginsvr::services::LocalConnectionRegistry>();

    asio::io_context server_io;
    tloginsvr::LoginServerConfig cfg{};
    cfg.port = 0;
    cfg.auth_service = auth.get();
    cfg.connection_registry = registry.get();
    tloginsvr::LoginServer server(server_io, cfg);
    const std::uint16_t port = server.Port();
    Check(port != 0, "server bound");

    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Client 1 logs in. Sit on the socket; expect to get kicked when
    // client 2 logs in for the same user.
    auto c1_future = std::async(std::launch::async, [port] {
        return RunOneClient(port, "alice", "hunter2",
                            std::chrono::seconds(2));
    });

    // Give client 1 time to complete its login round-trip and end up
    // sitting in the registry.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    Check(registry->Count() == 1, "after client 1 login: registry has 1 entry");

    // Client 2 logs in as the same user. Should get its own ack and
    // stay alive.
    const auto c2 = RunOneClient(port, "alice", "hunter2",
                                 std::chrono::milliseconds(100));
    Check(c2.ack_seen, "client 2 received its CS_LOGIN_ACK");
    Check(c2.ack_result == 0, "client 2 ack is LR_SUCCESS (0)");
    // Client 2's RunPackets loop hasn't ended within 100ms — that
    // confirms client 2 is still live (server didn't kick it).
    Check(!c2.got_eof, "client 2 NOT kicked (its socket stays open)");

    const auto c1 = c1_future.get();
    Check(c1.ack_seen, "client 1 received its CS_LOGIN_ACK before being kicked");
    Check(c1.ack_result == 0, "client 1 initial ack was LR_SUCCESS (0)");
    Check(c1.got_eof, "client 1 socket closed (kicked by server)");

    // After both clients are done (test cleanup closed them), registry
    // should have at most 1 entry (client 2's entry from before its
    // local Close). The HandleConnection coroutine's Unregister will
    // run when the server-side session closes — that's async, so we
    // can't assert exact count immediately. But the duplicate-kick
    // path itself (registry count was 1 between login 1 and login 2,
    // and is at most 1 after login 2) is what we care about.

    server_io.stop();
    if (server_thread.joinable()) server_thread.join();
}

} // namespace

int main()
{
    std::printf("=== tloginsvr_asio duplicate-kick test ===\n");
    try
    {
        TestDuplicateKick();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
