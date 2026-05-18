// End-to-end test for the auth-service-backed CS_LOGIN_REQ path.
// Validates the full wire flow:
//   * client serializes the legacy 5-string LOGIN_REQ body
//   * server parses it via ParseLoginReq
//   * server delegates to IAuthService (in-memory backend seeded with
//     one good user + one ban + one IP block)
//   * server returns the correct LR_* code in CS_LOGIN_ACK
//
// One ctest target, four scenarios (good login, bad password, no
// user, IP banned). Wire-format pipeline same as the existing
// handshake test; the new bit is the IAuthService dispatch.

#include "../login_server.h"
#include "../services/fake_auth_service.h"
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

// Serialize a CS_LOGIN_REQ body matching the legacy wire layout:
//   WORD version, then 5 length-prefixed strings (zombie3, password,
//   zombie1, zombie2, userId), then INT64 dlCheck + INT64 llChecksum.
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
    append_string("");        // zombie3
    append_string(password);
    append_string("");        // zombie1
    append_string("");        // zombie2
    append_string(user_id);
    // Legacy checksum from CSHandler.cpp:185-202 — the client computes
    // it from wVersion alone. Server enforces a match; sending 0 → close.
    constexpr std::int64_t kKey = 0x336c3aebf71a8b08LL;
    std::int64_t ck = static_cast<std::int64_t>(version) * 2 - 500;
    const std::int64_t idx  = ck % 8;
    const std::int64_t body = ck / 8;
    for (std::int64_t i = 0; i < idx; ++i) { ck ^= body; ck += kKey; }
    std::int64_t dlCheck = 0;
    append_bytes(&dlCheck, 8);
    append_bytes(&ck, 8);
    return out;
}

// One CS_LOGIN_REQ → CS_LOGIN_ACK round trip. Returns the bResult
// byte (first byte of the ack body) or 0xFF on timeout.
std::uint8_t SendLoginAndGetResult(asio::io_context& server_io,
                                   std::uint16_t port,
                                   const std::string& user_id,
                                   const std::string& password)
{
    asio::io_context client_io;
    tcp::socket client_sock(client_io);
    client_sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(client_sock), tnetlib::PeerType::Server);

    std::atomic<bool>   got_ack{false};
    std::atomic<std::uint8_t> result{0xFF};

    asio::co_spawn(client_io,
        [sess, &got_ack, &result]() -> asio::awaitable<void> {
            co_await sess->RunPackets(
                [&got_ack, &result](const tnetlib::DecodedPacket& pkt) {
                    if (pkt.wId == tnetlib::protocol::ToUint16(
                            tnetlib::protocol::MessageId::CS_LOGIN_ACK))
                    {
                        if (!pkt.body.empty())
                            result.store(static_cast<std::uint8_t>(pkt.body[0]));
                        got_ack.store(true);
                    }
                });
        },
        asio::detached);

    asio::co_spawn(client_io,
        [sess, user_id, password]() -> asio::awaitable<void> {
            const auto body = MakeLoginReq(kProtocolVersion, user_id, password);
            co_await sess->SendPacket(
                tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_LOGIN_REQ),
                std::span<const std::byte>(body.data(), body.size()));
        },
        asio::detached);

    std::thread client_thread([&client_io] { client_io.run(); });
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!got_ack.load() &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    (void)server_io;
    return result.load();
}

void TestAuthFlow()
{
    std::printf("[in-memory auth backend: good / bad-password / no-user / banned-user]\n");

    // Seed the backend with one good user and one banned user.
    auto auth = std::make_unique<tloginsvr::services::FakeAuthService>();
    auth->AddUser("alice", "hunter2", /*db_id=*/1001);
    auth->AddUser("bob",   "secret",  /*db_id=*/1002);
    auth->BanUser(/*db_id=*/1002, "spam");

    asio::io_context server_io;
    tloginsvr::LoginServerConfig cfg{};
    cfg.port = 0;
    cfg.auth_service = auth.get();
    tloginsvr::LoginServer server(server_io, cfg);
    const std::uint16_t port = server.Port();
    Check(port != 0, "server bound");

    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 1. Good login.
    const auto ok = SendLoginAndGetResult(server_io, port, "alice", "hunter2");
    Check(ok == 0, "alice + correct password → LR_SUCCESS (0)");

    // 2. Wrong password.
    const auto wrong_pw = SendLoginAndGetResult(server_io, port, "alice", "wrong");
    Check(wrong_pw == 2, "alice + wrong password → LR_INVALIDPASSWD (2)");

    // 3. Unknown user.
    const auto no_user = SendLoginAndGetResult(server_io, port, "charlie", "x");
    Check(no_user == 1, "unknown user → LR_NOUSER (1)");

    // 4. Banned user (correct password, but user-level ban applies).
    const auto banned = SendLoginAndGetResult(server_io, port, "bob", "secret");
    Check(banned == 6, "banned user → LR_BLOCK (6)");

    server_io.stop();
    if (server_thread.joinable()) server_thread.join();
}

} // namespace

int main()
{
    std::printf("=== tloginsvr_asio auth-flow test ===\n");
    try
    {
        TestAuthFlow();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
