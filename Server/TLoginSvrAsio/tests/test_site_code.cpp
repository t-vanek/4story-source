// Regression for round-5 #1: JP/TW dwSiteCode parsing.
//
// The shipped client appends a DWORD dwSiteCode to CS_LOGIN_REQ when
// MODIFY_DIRECTLOGIN is set (TNationOption::SetNation flips that for
// TNATION_JAPAN and TNATION_TAIWAN, TNetSender.cpp:46). Round-1
// implemented this as a single BYTE bChanneling — round-5 widened
// the field to the full DWORD the client actually emits.
//
// This test ends up driving an end-to-end LOGIN_REQ via the server,
// then asserts the AuthRequest the in-memory fake captured carries
// site_code with the full 32-bit value (not just the low byte).

#include "../login_server.h"
#include "../services/fake_auth_service.h"
#include "asio_session.h"
#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

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

// FakeAuthService doesn't expose the AuthRequest it received, so we
// subclass it to capture the call. The shipped one is final-free —
// safe to extend.
class CapturingAuth : public tloginsvr::services::FakeAuthService
{
public:
    std::optional<tloginsvr::services::AuthRequest> last_req;
    tloginsvr::services::AuthResult Authenticate(
        const tloginsvr::services::AuthRequest& req) override
    {
        last_req = req;
        return tloginsvr::services::FakeAuthService::Authenticate(req);
    }
};

std::vector<std::byte> MakeLoginReq(std::uint16_t version,
                                    const std::string& user_id,
                                    const std::string& password,
                                    std::optional<std::uint32_t> site_code)
{
    std::vector<std::byte> out;
    auto append = [&](const void* src, std::size_t n) {
        const auto* p = reinterpret_cast<const std::byte*>(src);
        out.insert(out.end(), p, p + n);
    };
    auto str = [&](const std::string& s) {
        std::int32_t len = static_cast<std::int32_t>(s.size());
        append(&len, 4);
        append(s.data(), s.size());
    };

    append(&version, 2);
    str("");        // zombie3
    str(password);
    str("");        // zombie1
    str("");        // zombie2
    str(user_id);
    // Legacy checksum from CSHandler.cpp:185-202.
    constexpr std::int64_t kKey = 0x336c3aebf71a8b08LL;
    std::int64_t ck = static_cast<std::int64_t>(version) * 2 - 500;
    const std::int64_t idx  = ck % 8;
    const std::int64_t body = ck / 8;
    for (std::int64_t i = 0; i < idx; ++i) { ck ^= body; ck += kKey; }
    std::int64_t dlCheck = 0;
    append(&dlCheck, 8);
    append(&ck, 8);

    if (site_code.has_value())
    {
        const std::uint32_t sc = *site_code;
        append(&sc, 4);    // DWORD dwSiteCode — what MODIFY_DIRECTLOGIN clients emit
    }
    return out;
}

// Drive one CS_LOGIN_REQ → CS_LOGIN_ACK round trip and return the
// captured AuthRequest (if any).
std::optional<tloginsvr::services::AuthRequest> RunLogin(
    tloginsvr::Nation nation,
    std::optional<std::uint32_t> site_code)
{
    asio::io_context server_io;
    auto auth = std::make_unique<CapturingAuth>();
    auth->AddUser("alice", "secret", 42);

    tloginsvr::LoginServerConfig cfg{};
    cfg.port = 0;
    cfg.auth_service = auth.get();
    cfg.nation = nation;

    tloginsvr::LoginServer server(server_io, cfg);
    const std::uint16_t port = server.Port();
    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    asio::io_context client_io;
    tcp::socket client_sock(client_io);
    client_sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));
    auto client = std::make_shared<tnetlib::AsioSession>(
        std::move(client_sock), tnetlib::PeerType::Server);

    std::atomic<bool> got_ack{false};
    auto recv_coro = [client, &got_ack]() -> asio::awaitable<void> {
        co_await client->RunPackets(
            [&got_ack](const tnetlib::DecodedPacket& pkt) {
                if (pkt.wId == tnetlib::protocol::ToUint16(
                        tnetlib::protocol::MessageId::CS_LOGIN_ACK))
                    got_ack = true;
            });
    };
    asio::co_spawn(client_io, recv_coro(), asio::detached);

    auto send_coro = [client, site_code]() -> asio::awaitable<void> {
        const auto body = MakeLoginReq(
            kProtocolVersion, "alice", "secret", site_code);
        co_await client->SendPacket(
            tnetlib::protocol::ToUint16(
                tnetlib::protocol::MessageId::CS_LOGIN_REQ),
            std::span<const std::byte>(body.data(), body.size()));
    };
    asio::co_spawn(client_io, send_coro(), asio::detached);
    std::thread client_thread([&client_io] { client_io.run(); });

    for (int i = 0; i < 50 && !got_ack; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

    auto captured = auth->last_req;
    server_io.stop();
    client_io.stop();
    if (server_thread.joinable()) server_thread.join();
    if (client_thread.joinable()) client_thread.join();
    return captured;
}

void TestJpReadsFullDword()
{
    std::printf("[site_code — JP nation reads DWORD\n");
    // Use a value with a non-zero upper 24 bits so a "bChanneling = low
    // byte only" implementation would lose data and we can detect it.
    constexpr std::uint32_t kSentSiteCode = 0xDEADBEEFu;
    const auto req = RunLogin(tloginsvr::Nation::Japan, kSentSiteCode);
    Check(req.has_value(), "AuthRequest captured");
    if (!req) return;
    Check(req->site_code_present, "site_code_present=true on JP");
    Check(req->site_code == kSentSiteCode, "site_code == 0xDEADBEEF");
    Check(req->channeling() == 0xEFu, "channeling() == low byte");
}

void TestTwReadsFullDword()
{
    std::printf("[site_code — TW nation reads DWORD\n");
    constexpr std::uint32_t kSentSiteCode = 0x01020304u;
    const auto req = RunLogin(tloginsvr::Nation::Taiwan, kSentSiteCode);
    Check(req.has_value(), "AuthRequest captured");
    if (!req) return;
    Check(req->site_code_present, "site_code_present=true on TW");
    Check(req->site_code == kSentSiteCode, "site_code == 0x01020304");
}

void TestUsIgnoresTailBytes()
{
    std::printf("[site_code — US nation does not read site_code\n");
    // Send a body WITHOUT the trailing DWORD (US client never emits it).
    // The parser shouldn't synthesize one.
    const auto req = RunLogin(tloginsvr::Nation::US, std::nullopt);
    Check(req.has_value(), "AuthRequest captured");
    if (!req) return;
    Check(!req->site_code_present, "site_code_present=false on US");
    Check(req->site_code == 0, "site_code stays 0");
}

} // namespace

int main()
{
    TestJpReadsFullDword();
    TestTwReadsFullDword();
    TestUsIgnoresTailBytes();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
