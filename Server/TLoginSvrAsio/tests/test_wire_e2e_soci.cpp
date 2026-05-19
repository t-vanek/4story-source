// True end-to-end smoke test: wire codec + dispatcher + SociAuthService
// against a live MSSQL TGLOBAL_RAGEZONE. Validates the seam that the
// in-process test_auth_flow (wire + FakeAuthService) and the SOCI
// test suite (DB + service, no wire) leave between them.
//
// Spawns an in-process LoginServer wired with a real SOCI auth + a
// fake connection registry (the registry's only job here is to keep
// the duplicate-kick path alive; we don't exercise it). A
// PeerType::Server client connects on loopback, sends a real
// CS_LOGIN_REQ for a freshly-seeded TACCOUNT_PW row, and asserts
// CS_LOGIN_ACK arrives with bResult == LR_SUCCESS.
//
// Requires:
//   TLOGINSVR_TEST_MSSQL_CONN — ODBC connection string to a live
//                               TGLOBAL_RAGEZONE (legacy schema).
// Skipped (0/0) when unset.

#include "../login_server.h"
#include "../services/soci_auth_service.h"
#include "../services/local_connection_registry.h"
#include "asio_session.h"
#include "MessageId.h"

#include "fourstory/db/session_pool.h"

#include <soci/soci.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>

#if defined(_WIN32)
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
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

// Same wire layout as test_auth_flow.cpp::MakeLoginReq — see the
// inline comment there for the field order rationale.
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

std::uint8_t SendLoginAndGetResult(std::uint16_t port,
                                   const std::string& user_id,
                                   const std::string& password)
{
    asio::io_context client_io;
    tcp::socket client_sock(client_io);
    client_sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(client_sock), tnetlib::PeerType::Server);

    std::atomic<bool>        got_ack{false};
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
        std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!got_ack.load() &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    return result.load();
}

// Seed a TACCOUNT_PW row with a plaintext password — modern's
// CheckPassword treats non-$2-prefixed `szPasswd` as legacy plaintext
// and direct-compares (then transparent-upgrades to BCrypt on the way
// out). Lets the test skip BCrypt-hash generation in C++.
// Real MSSQL has TACCOUNT_PW.dwUserID as IDENTITY — wrap the explicit
// dwUserID INSERT with SET IDENTITY_INSERT so the test owns the value.
// Also pre-seeds TUSERINFOTABLE.bAgreement = 1 so the happy-path auth
// doesn't trip LR_NEEDAGREEMENT.
void SeedAccount(soci::session& sql,
                 int uid, const std::string& uname,
                 const std::string& pw_plain)
{
    sql << "DELETE FROM \"TLOG\"          WHERE \"dwUserID\" = :u", soci::use(uid);
    sql << "DELETE FROM \"TCURRENTUSER\"  WHERE \"dwUserID\" = :u", soci::use(uid);
    sql << "DELETE FROM \"TUSERINFOTABLE\" WHERE \"dwUserID\" = :u", soci::use(uid);
    sql << "DELETE FROM \"TACCOUNT_PW\"   WHERE \"dwUserID\" = :u", soci::use(uid);

    sql << "SET IDENTITY_INSERT \"TACCOUNT_PW\" ON; "
           "INSERT INTO \"TACCOUNT_PW\" "
           "(\"dwUserID\", \"szUserID\", \"szPasswd\") "
           "VALUES (:u, :n, :p); "
           "SET IDENTITY_INSERT \"TACCOUNT_PW\" OFF;",
        soci::use(uid), soci::use(uname), soci::use(pw_plain);

    sql << "INSERT INTO \"TUSERINFOTABLE\" "
           "(\"dwUserID\", \"bCanCreateCharCount\", \"bAgreement\") "
           "VALUES (:u, 6, 1)",
        soci::use(uid);
}

void RunTests(const std::string& conn)
{
    fourstory::db::SessionPool pool(
        fourstory::db::Backend::Odbc, conn, /*pool_size=*/2);

    // PID-scoped uid so parallel runs don't collide.
    const int test_uid = 2900000 + (::getpid() % 1000);
    const std::string test_user = "wireE2E_" + std::to_string(::getpid());
    const std::string test_pw   = "passw0rd";

    {
        auto lease = pool.Acquire();
        SeedAccount(*lease, test_uid, test_user, test_pw);
    }

    // Wire up the SOCI auth service and a real connection registry so
    // the LoginServer's IsAgreed gate has a session entry to consult
    // after a successful login.
    tloginsvr::services::SociAuthService auth(pool);
    tloginsvr::services::LocalConnectionRegistry registry;

    asio::io_context server_io;
    tloginsvr::LoginServerConfig cfg{};
    cfg.port = 0;  // ephemeral
    cfg.auth_service        = &auth;
    cfg.connection_registry = &registry;
    tloginsvr::LoginServer server(server_io, cfg);
    const std::uint16_t port = server.Port();
    Check(port != 0, "wire+SOCI: server bound to ephemeral port");

    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(75));

    // 1. Happy path — real wire packet through real SOCI auth.
    {
        const auto r = SendLoginAndGetResult(port, test_user, test_pw);
        Check(r == 0,
            "real client + real DB + real password → LR_SUCCESS (0)");
    }

    // 2. Wrong password — same path but auth rejects.
    {
        const auto r = SendLoginAndGetResult(port, test_user, "WRONG");
        // LR_INVALIDPASSWD = 2 (NetCode.h)
        Check(r == 2,
            "real wire + bad password → LR_INVALIDPASSWD (2)");
    }

    // 3. Unknown user.
    {
        const auto r = SendLoginAndGetResult(port, "no_such_user_xyz", "anything");
        // LR_NOUSER = 1 (NetCode.h)
        Check(r == 1,
            "real wire + missing user → LR_NOUSER (1)");
    }

    server_io.stop();
    if (server_thread.joinable()) server_thread.join();

    // Cleanup.
    try
    {
        auto lease = pool.Acquire();
        soci::session& sql = *lease;
        sql << "DELETE FROM \"TLOG\"          WHERE \"dwUserID\" = :u", soci::use(test_uid);
        sql << "DELETE FROM \"TCURRENTUSER\"  WHERE \"dwUserID\" = :u", soci::use(test_uid);
        sql << "DELETE FROM \"TUSERINFOTABLE\" WHERE \"dwUserID\" = :u", soci::use(test_uid);
        sql << "DELETE FROM \"TACCOUNT_PW\"   WHERE \"dwUserID\" = :u", soci::use(test_uid);
    }
    catch (const std::exception& ex)
    {
        std::printf("  WARN  cleanup error (non-fatal): %s\n", ex.what());
    }
}

} // namespace

int main()
{
    std::printf("=== tloginsvr_asio wire+SOCI end-to-end smoke test ===\n");

    const char* conn = std::getenv("TLOGINSVR_TEST_MSSQL_CONN");
    if (conn == nullptr || conn[0] == '\0')
    {
        std::printf("  SKIP  TLOGINSVR_TEST_MSSQL_CONN not set\n");
        std::printf("\nResults: 0 passed, 0 failed (skipped)\n");
        return 0;
    }

    try
    {
        RunTests(conn);
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception: %s\n", ex.what());
        ++g_failed;
    }

    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
