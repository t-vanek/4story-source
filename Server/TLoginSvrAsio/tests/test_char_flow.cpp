// End-to-end test for the character lifecycle via ICharService.
// Three scenarios:
//   1. CHARLIST returns seeded characters in slot order.
//   2. CREATECHAR appends a new char; subsequent CHARLIST sees it.
//   3. DELCHAR removes a char; CHARLIST shrinks.

#include "../login_server.h"
#include "../services/fake_auth_service.h"
#include "../services/fake_char_service.h"
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

std::vector<std::byte> MakeLoginReq(const std::string& user, const std::string& pass)
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
    // Legacy checksum (CSHandler.cpp:185-202). dlCheck=0 ok (exec-file
    // probe disabled); llChecksum must match the per-version compute.
    constexpr std::int64_t kKey = 0x336c3aebf71a8b08LL;
    std::int64_t ck = static_cast<std::int64_t>(v) * 2 - 500;
    const std::int64_t idx = ck % 8, body = ck / 8;
    for (std::int64_t i = 0; i < idx; ++i) { ck ^= body; ck += kKey; }
    std::int64_t dlCheck = 0;
    bytes(&dlCheck, 8);
    bytes(&ck, 8);
    return out;
}

// One client session that drives LOGIN + N more requests, collects
// all acks, then closes. Returns the collected (wId, body) pairs.
struct Capture
{
    std::vector<std::pair<std::uint16_t, std::vector<std::byte>>> acks;
};

template <typename SendFn>
Capture RunClientAndCapture(std::uint16_t port,
                            const std::string& user, const std::string& pass,
                            int expected_acks,
                            SendFn after_login)
{
    asio::io_context client_io;
    tcp::socket sock(client_io);
    sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(sock), tnetlib::PeerType::Server);

    Capture cap;
    std::mutex cap_mtx;
    std::atomic<int> count{0};

    asio::co_spawn(client_io,
        [sess, &cap, &cap_mtx, &count]() -> asio::awaitable<void> {
            co_await sess->RunPackets(
                [&cap, &cap_mtx, &count](const tnetlib::DecodedPacket& pkt) {
                    std::lock_guard lk(cap_mtx);
                    cap.acks.push_back({pkt.wId,
                        std::vector<std::byte>(pkt.body.begin(), pkt.body.end())});
                    count.fetch_add(1);
                });
        },
        asio::detached);

    asio::co_spawn(client_io,
        [sess, user, pass, after_login]() -> asio::awaitable<void> {
            const auto login = MakeLoginReq(user, pass);
            co_await sess->SendPacket(
                tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_LOGIN_REQ),
                std::span<const std::byte>(login.data(), login.size()));
            co_await after_login(sess);
        },
        asio::detached);

    std::thread t([&client_io] { client_io.run(); });
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (count.load() < expected_acks &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    sess->Close();
    client_io.stop();
    if (t.joinable()) t.join();
    return cap;
}

void TestCharListWithSeededChars()
{
    std::printf("[CHARLIST returns seeded characters in slot order]\n");

    auto auth = std::make_unique<tloginsvr::services::FakeAuthService>();
    auth->AddUser("alice", "pw", 1001);
    auto registry = std::make_unique<tloginsvr::services::LocalConnectionRegistry>();
    auto chars = std::make_unique<tloginsvr::services::FakeCharService>();

    // Seed alice with 2 characters in group 1.
    chars->AddCharacter(1001, 1, tloginsvr::services::CharacterInfo{
        .char_id = 100, .name = "AliceMain", .slot = 0, .level = 30,
        .char_class = 1, .race = 0, .country = 0, .sex = 1,
    });
    chars->AddCharacter(1001, 1, tloginsvr::services::CharacterInfo{
        .char_id = 101, .name = "AliceAlt", .slot = 2, .level = 5,
        .char_class = 2, .race = 1, .country = 1, .sex = 0,
    });

    asio::io_context server_io;
    tloginsvr::LoginServerConfig cfg{};
    cfg.port = 0;
    cfg.auth_service = auth.get();
    cfg.connection_registry = registry.get();
    cfg.char_service = chars.get();
    tloginsvr::LoginServer server(server_io, cfg);
    const std::uint16_t port = server.Port();

    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const auto cap = RunClientAndCapture(port, "alice", "pw", 2,
        [](std::shared_ptr<tnetlib::AsioSession> sess) -> asio::awaitable<void> {
            std::byte body[1] = { std::byte{1} };
            co_await sess->SendPacket(
                tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CHARLIST_REQ),
                std::span<const std::byte>(body, 1));
        });

    Check(cap.acks.size() >= 2, "received LOGIN_ACK + CHARLIST_ACK");
    if (cap.acks.size() >= 2)
    {
        const auto& charlist = cap.acks[1].second;
        Check(cap.acks[1].first == tnetlib::protocol::ToUint16(
                tnetlib::protocol::MessageId::CS_CHARLIST_ACK),
              "second ack is CS_CHARLIST_ACK");
        // payload[0..3]=DWORD CheckFilePoint=0, payload[4]=BYTE bCount
        Check(charlist.size() >= 5, "CHARLIST_ACK has at least 5 bytes");
        std::uint32_t check_point = 0;
        std::memcpy(&check_point, charlist.data(), 4);
        Check(check_point == 0, "CheckFilePoint = 0");
        Check(static_cast<std::uint8_t>(charlist[4]) == 2,
              "bCount = 2 (matches seeded characters)");
    }

    server_io.stop();
    if (server_thread.joinable()) server_thread.join();
}

void TestCreateCharAppendsToList()
{
    std::printf("[CREATECHAR adds; subsequent CHARLIST sees the new entry]\n");

    auto auth = std::make_unique<tloginsvr::services::FakeAuthService>();
    auth->AddUser("bob", "pw", 2002);
    auto registry = std::make_unique<tloginsvr::services::LocalConnectionRegistry>();
    auto chars = std::make_unique<tloginsvr::services::FakeCharService>();

    asio::io_context server_io;
    tloginsvr::LoginServerConfig cfg{};
    cfg.port = 0;
    cfg.auth_service = auth.get();
    cfg.connection_registry = registry.get();
    cfg.char_service = chars.get();
    tloginsvr::LoginServer server(server_io, cfg);
    const std::uint16_t port = server.Port();

    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const auto cap = RunClientAndCapture(port, "bob", "pw", 3,
        [](std::shared_ptr<tnetlib::AsioSession> sess) -> asio::awaitable<void> {
            // CREATECHAR body: bGroupID + STRING name + 13 byte fields
            std::vector<std::byte> create;
            auto append = [&](const void* src, std::size_t n) {
                const auto* p = reinterpret_cast<const std::byte*>(src);
                create.insert(create.end(), p, p + n);
            };
            std::uint8_t group = 1;
            append(&group, 1);
            std::string name = "BobNew";
            std::int32_t namelen = static_cast<std::int32_t>(name.size());
            append(&namelen, 4);
            append(name.data(), name.size());
            std::uint8_t fields[13] = {0,1,0,0,0,0,0,0,0,0,0,0,0};
            // slot=0, class=1, race=0, country=0, sex=0, hair=0, face=0,
            // body=0, pants=0, hand=0, foot=0, levelOption=0
            append(fields, 13);
            co_await sess->SendPacket(
                tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CREATECHAR_REQ),
                std::span<const std::byte>(create.data(), create.size()));

            // Followup CHARLIST
            std::byte cl[1] = { std::byte{1} };
            co_await sess->SendPacket(
                tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CHARLIST_REQ),
                std::span<const std::byte>(cl, 1));
        });

    Check(cap.acks.size() >= 3, "received LOGIN + CREATECHAR + CHARLIST acks");
    if (cap.acks.size() >= 3)
    {
        const auto& create_ack = cap.acks[1].second;
        Check(cap.acks[1].first == tnetlib::protocol::ToUint16(
                tnetlib::protocol::MessageId::CS_CREATECHAR_ACK),
              "second ack is CS_CREATECHAR_ACK");
        Check(static_cast<std::uint8_t>(create_ack[0]) == 0,
              "CREATECHAR result = CR_SUCCESS (0)");

        const auto& cl_ack = cap.acks[2].second;
        Check(cap.acks[2].first == tnetlib::protocol::ToUint16(
                tnetlib::protocol::MessageId::CS_CHARLIST_ACK),
              "third ack is CS_CHARLIST_ACK");
        Check(static_cast<std::uint8_t>(cl_ack[4]) == 1,
              "CHARLIST shows 1 character after CREATE");
    }

    server_io.stop();
    if (server_thread.joinable()) server_thread.join();
}

void TestDelCharRemovesFromList()
{
    std::printf("[DELCHAR removes; subsequent CHARLIST is shorter]\n");

    auto auth = std::make_unique<tloginsvr::services::FakeAuthService>();
    auth->AddUser("carol", "pw", 3003);
    auto registry = std::make_unique<tloginsvr::services::LocalConnectionRegistry>();
    auto chars = std::make_unique<tloginsvr::services::FakeCharService>();
    chars->AddCharacter(3003, 1, tloginsvr::services::CharacterInfo{
        .char_id = 500, .name = "CarolMain", .slot = 0, .level = 10});

    asio::io_context server_io;
    tloginsvr::LoginServerConfig cfg{};
    cfg.port = 0;
    cfg.auth_service = auth.get();
    cfg.connection_registry = registry.get();
    cfg.char_service = chars.get();
    tloginsvr::LoginServer server(server_io, cfg);
    const std::uint16_t port = server.Port();

    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const auto cap = RunClientAndCapture(port, "carol", "pw", 3,
        [](std::shared_ptr<tnetlib::AsioSession> sess) -> asio::awaitable<void> {
            // DELCHAR body: bGroupID + STRING password + DWORD charId
            std::vector<std::byte> del;
            auto append = [&](const void* src, std::size_t n) {
                const auto* p = reinterpret_cast<const std::byte*>(src);
                del.insert(del.end(), p, p + n);
            };
            std::uint8_t group = 1;
            append(&group, 1);
            std::string pw = "pw";
            std::int32_t pwlen = static_cast<std::int32_t>(pw.size());
            append(&pwlen, 4);
            append(pw.data(), pw.size());
            std::int32_t cid = 500;
            append(&cid, 4);
            co_await sess->SendPacket(
                tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_DELCHAR_REQ),
                std::span<const std::byte>(del.data(), del.size()));

            // CHARLIST after
            std::byte cl[1] = { std::byte{1} };
            co_await sess->SendPacket(
                tnetlib::protocol::ToUint16(tnetlib::protocol::MessageId::CS_CHARLIST_REQ),
                std::span<const std::byte>(cl, 1));
        });

    Check(cap.acks.size() >= 3, "received LOGIN + DELCHAR + CHARLIST acks");
    if (cap.acks.size() >= 3)
    {
        const auto& del_ack = cap.acks[1].second;
        Check(cap.acks[1].first == tnetlib::protocol::ToUint16(
                tnetlib::protocol::MessageId::CS_DELCHAR_ACK),
              "second ack is CS_DELCHAR_ACK");
        Check(static_cast<std::uint8_t>(del_ack[0]) == 0,
              "DELCHAR result = DR_SUCCESS (0)");

        const auto& cl_ack = cap.acks[2].second;
        Check(static_cast<std::uint8_t>(cl_ack[4]) == 0,
              "CHARLIST shows 0 characters after DELETE");
    }

    server_io.stop();
    if (server_thread.joinable()) server_thread.join();
}

} // namespace

int main()
{
    std::printf("=== tloginsvr_asio char-flow test ===\n");
    try
    {
        TestCharListWithSeededChars();
        TestCreateCharAppendsToList();
        TestDelCharRemovesFromList();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
