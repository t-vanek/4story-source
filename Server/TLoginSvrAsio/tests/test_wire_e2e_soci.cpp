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
#include "../services/soci_char_service.h"
#include "../services/local_connection_registry.h"
#include "../services/bcrypt_util.h"
#include "asio_session.h"
#include "MessageId.h"

#include "fourstory/db/session_pool.h"

#include <soci/soci.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
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

// Captured ACK from a single login round-trip — used by both the
// single-shot SendLoginAndGetResult helper and the multi-packet
// session flow below.
struct LoginAckCapture
{
    std::atomic<bool>        got{false};
    std::atomic<std::uint8_t> result{0xFF};
};

// Multi-packet capture for a single session: LOGIN_ACK + CHARLIST_ACK
// + (later) CREATECHAR_ACK + START_ACK. The intra-coroutine state is
// in MultiAckShared (atomic so the recv coro can write while the
// timeout loop reads); the test consumer gets a plain MultiAckResult
// snapshot at return so it can be copied around freely.
struct MultiAckShared
{
    std::atomic<bool>        got_login{false};
    std::atomic<std::uint8_t> login_result{0xFF};
    std::atomic<bool>        got_charlist{false};
    std::atomic<std::uint8_t> charlist_count{0xFF};
    std::atomic<bool>        got_create{false};
    std::atomic<std::uint8_t> create_status{0xFF};
    std::atomic<std::int32_t> create_char_id{0};
};
struct MultiAckResult
{
    bool         got_login = false;
    std::uint8_t login_result = 0xFF;
    bool         got_charlist = false;
    std::uint8_t charlist_count = 0xFF;
    bool         got_create = false;
    std::uint8_t create_status = 0xFF;
    std::int32_t create_char_id = 0;
};

std::uint8_t SendLoginAndGetResult(std::uint16_t port,
                                   const std::string& user_id,
                                   const std::string& password)
{
    asio::io_context client_io;
    tcp::socket client_sock(client_io);
    client_sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(client_sock), tnetlib::PeerType::Server);

    LoginAckCapture cap;

    asio::co_spawn(client_io,
        [sess, &cap]() -> asio::awaitable<void> {
            co_await sess->RunPackets(
                [&cap](const tnetlib::DecodedPacket& pkt) {
                    if (pkt.wId == tnetlib::protocol::ToUint16(
                            tnetlib::protocol::MessageId::CS_LOGIN_ACK))
                    {
                        if (!pkt.body.empty())
                            cap.result.store(static_cast<std::uint8_t>(pkt.body[0]));
                        cap.got.store(true);
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
    while (!cap.got.load() &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    return cap.result.load();
}

// Runs CS_LOGIN_REQ then CS_CHARLIST_REQ on the same session, captures
// both ACKs. Group id is fixed to 1 for the charlist; client wire
// format is just BYTE bGroupID.
MultiAckResult SendLoginThenCharList(std::uint16_t port,
                                     const std::string& user_id,
                                     const std::string& password)
{
    asio::io_context client_io;
    tcp::socket client_sock(client_io);
    client_sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(client_sock), tnetlib::PeerType::Server);

    auto cap = std::make_shared<MultiAckShared>();

    asio::co_spawn(client_io,
        [sess, cap]() -> asio::awaitable<void> {
            co_await sess->RunPackets(
                [cap](const tnetlib::DecodedPacket& pkt) {
                    using tnetlib::protocol::MessageId;
                    using tnetlib::protocol::ToUint16;
                    if (pkt.wId == ToUint16(MessageId::CS_LOGIN_ACK))
                    {
                        if (!pkt.body.empty())
                            cap->login_result.store(static_cast<std::uint8_t>(pkt.body[0]));
                        cap->got_login.store(true);
                    }
                    else if (pkt.wId == ToUint16(MessageId::CS_CHARLIST_ACK))
                    {
                        // Wire body: DWORD dwCheckPoint (4) + BYTE bCount
                        // (+ per-char payload). 5 bytes is the minimum
                        // for an empty-list ACK.
                        if (pkt.body.size() >= 5)
                            cap->charlist_count.store(static_cast<std::uint8_t>(pkt.body[4]));
                        cap->got_charlist.store(true);
                    }
                });
        },
        asio::detached);

    asio::co_spawn(client_io,
        [sess, cap, user_id, password]() -> asio::awaitable<void> {
            using tnetlib::protocol::MessageId;
            using tnetlib::protocol::ToUint16;

            // 1. Send CS_LOGIN_REQ.
            const auto login_body = MakeLoginReq(kProtocolVersion, user_id, password);
            co_await sess->SendPacket(
                ToUint16(MessageId::CS_LOGIN_REQ),
                std::span<const std::byte>(login_body.data(), login_body.size()));

            // 2. Wait for LOGIN_ACK before sending CHARLIST_REQ — the
            //    server's IsAgreed gate looks up the connection-registry
            //    entry that the login handler registers on Success.
            const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(3);
            while (!cap->got_login.load() &&
                   std::chrono::steady_clock::now() < deadline)
            {
                co_await asio::steady_timer(co_await asio::this_coro::executor,
                                            std::chrono::milliseconds(10))
                    .async_wait(asio::use_awaitable);
            }
            if (cap->login_result.load() != 0)
                co_return;  // bail if login didn't succeed

            // 3. Send CS_CHARLIST_REQ (1-byte body: bGroupID).
            const std::byte charlist_body[1] = { std::byte{1} };
            co_await sess->SendPacket(
                ToUint16(MessageId::CS_CHARLIST_REQ),
                std::span<const std::byte>(charlist_body, 1));
        },
        asio::detached);

    std::thread client_thread([&client_io] { client_io.run(); });
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!cap->got_charlist.load() &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();

    MultiAckResult out;
    out.got_login      = cap->got_login.load();
    out.login_result   = cap->login_result.load();
    out.got_charlist   = cap->got_charlist.load();
    out.charlist_count = cap->charlist_count.load();
    return out;
}

// Build CS_CREATECHAR_REQ body. Wire layout (handlers.cpp:832-873):
//   BYTE   bGroupID
//   STRING strName (INT32 len + bytes, max 64)
//   BYTE   bSlot, bClass, bRace, bCountry, bSex, bHair, bFace,
//          bBody, bPants, bHand, bFoot, bLevelOption
std::vector<std::byte> MakeCreateCharReq(std::uint8_t group,
                                         const std::string& name,
                                         std::uint8_t slot,
                                         std::uint8_t char_class,
                                         std::uint8_t race,
                                         std::uint8_t country)
{
    std::vector<std::byte> out;
    auto u8 = [&](std::uint8_t v) { out.push_back(std::byte{v}); };
    auto str = [&](const std::string& s) {
        std::int32_t len = static_cast<std::int32_t>(s.size());
        out.resize(out.size() + 4);
        std::memcpy(out.data() + out.size() - 4, &len, 4);
        const auto* p = reinterpret_cast<const std::byte*>(s.data());
        out.insert(out.end(), p, p + s.size());
    };
    u8(group);
    str(name);
    u8(slot);
    u8(char_class);
    u8(race);
    u8(country);
    u8(0);  // bSex
    u8(0);  // bHair
    u8(0);  // bFace
    u8(0);  // bBody
    u8(0);  // bPants
    u8(0);  // bHand
    u8(0);  // bFoot
    u8(0);  // bLevelOption (no veteran bonus)
    return out;
}

// Login → CHARLIST → CREATECHAR on a single session. Reuses the
// MultiAck state machine — CHARLIST_ACK signals the per-session
// agreement gate is hot, CREATECHAR_ACK then exercises the world
// pool through the wire dispatcher.
MultiAckResult SendLoginCharListCreate(std::uint16_t port,
                                       const std::string& user_id,
                                       const std::string& password,
                                       const std::string& char_name)
{
    asio::io_context client_io;
    tcp::socket client_sock(client_io);
    client_sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port));

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(client_sock), tnetlib::PeerType::Server);

    auto cap = std::make_shared<MultiAckShared>();

    asio::co_spawn(client_io,
        [sess, cap]() -> asio::awaitable<void> {
            co_await sess->RunPackets(
                [cap](const tnetlib::DecodedPacket& pkt) {
                    using tnetlib::protocol::MessageId;
                    using tnetlib::protocol::ToUint16;
                    if (pkt.wId == ToUint16(MessageId::CS_LOGIN_ACK))
                    {
                        if (!pkt.body.empty())
                            cap->login_result.store(static_cast<std::uint8_t>(pkt.body[0]));
                        cap->got_login.store(true);
                    }
                    else if (pkt.wId == ToUint16(MessageId::CS_CHARLIST_ACK))
                    {
                        if (pkt.body.size() >= 5)
                            cap->charlist_count.store(static_cast<std::uint8_t>(pkt.body[4]));
                        cap->got_charlist.store(true);
                    }
                    else if (pkt.wId == ToUint16(MessageId::CS_CREATECHAR_ACK))
                    {
                        // BYTE status + INT32 char_id + ...
                        if (!pkt.body.empty())
                            cap->create_status.store(static_cast<std::uint8_t>(pkt.body[0]));
                        if (pkt.body.size() >= 5)
                        {
                            std::int32_t cid = 0;
                            std::memcpy(&cid, pkt.body.data() + 1, 4);
                            cap->create_char_id.store(cid);
                        }
                        cap->got_create.store(true);
                    }
                });
        },
        asio::detached);

    asio::co_spawn(client_io,
        [sess, cap, user_id, password, char_name]() -> asio::awaitable<void> {
            using tnetlib::protocol::MessageId;
            using tnetlib::protocol::ToUint16;

            // 1. LOGIN.
            const auto login_body = MakeLoginReq(kProtocolVersion, user_id, password);
            co_await sess->SendPacket(
                ToUint16(MessageId::CS_LOGIN_REQ),
                std::span<const std::byte>(login_body.data(), login_body.size()));

            // Wait for LOGIN_ACK.
            const auto login_deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(3);
            while (!cap->got_login.load() &&
                   std::chrono::steady_clock::now() < login_deadline)
            {
                co_await asio::steady_timer(co_await asio::this_coro::executor,
                                            std::chrono::milliseconds(10))
                    .async_wait(asio::use_awaitable);
            }
            if (cap->login_result.load() != 0) co_return;

            // 2. CHARLIST (also flips the per-session group_id for
            //    DELCHAR / START_REQ later).
            const std::byte cl_body[1] = { std::byte{1} };
            co_await sess->SendPacket(
                ToUint16(MessageId::CS_CHARLIST_REQ),
                std::span<const std::byte>(cl_body, 1));

            const auto cl_deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(3);
            while (!cap->got_charlist.load() &&
                   std::chrono::steady_clock::now() < cl_deadline)
            {
                co_await asio::steady_timer(co_await asio::this_coro::executor,
                                            std::chrono::milliseconds(10))
                    .async_wait(asio::use_awaitable);
            }

            // 3. CREATECHAR. Class 1, race 0, peace country (4 →
            //    starts at level 1 without a veteran bonus).
            const auto cc_body = MakeCreateCharReq(
                /*group=*/1, char_name,
                /*slot=*/0, /*char_class=*/1, /*race=*/0, /*country=*/4);
            co_await sess->SendPacket(
                ToUint16(MessageId::CS_CREATECHAR_REQ),
                std::span<const std::byte>(cc_body.data(), cc_body.size()));
        },
        asio::detached);

    std::thread client_thread([&client_io] { client_io.run(); });
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(8);
    while (!cap->got_create.load() &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();

    MultiAckResult out;
    out.got_login       = cap->got_login.load();
    out.login_result    = cap->login_result.load();
    out.got_charlist    = cap->got_charlist.load();
    out.charlist_count  = cap->charlist_count.load();
    out.got_create      = cap->got_create.load();
    out.create_status   = cap->create_status.load();
    out.create_char_id  = cap->create_char_id.load();
    return out;
}

// Seed a TACCOUNT_PW row with a freshly-computed BCrypt hash — modern's
// CheckPassword is bcrypt-only after the hard cutover, so the test
// must store a `$2a$` hash. Real MSSQL has TACCOUNT_PW.dwUserID as
// IDENTITY — wrap the explicit dwUserID INSERT with SET
// IDENTITY_INSERT so the test owns the value. Also pre-seeds
// TUSERINFOTABLE.bAgreement = 1 so the happy-path auth doesn't trip
// LR_NEEDAGREEMENT.
void SeedAccount(soci::session& sql,
                 int uid, const std::string& uname,
                 const std::string& pw_plain)
{
    sql << "DELETE FROM \"TLOG\"          WHERE \"dwUserID\" = :u", soci::use(uid);
    sql << "DELETE FROM \"TCURRENTUSER\"  WHERE \"dwUserID\" = :u", soci::use(uid);
    sql << "DELETE FROM \"TUSERINFOTABLE\" WHERE \"dwUserID\" = :u", soci::use(uid);
    sql << "DELETE FROM \"TACCOUNT_PW\"   WHERE \"dwUserID\" = :u", soci::use(uid);

    const std::string pw_hash =
        tloginsvr::services::bcrypt_util::MakeBcryptHash(pw_plain);

    sql << "SET IDENTITY_INSERT \"TACCOUNT_PW\" ON; "
           "INSERT INTO \"TACCOUNT_PW\" "
           "(\"dwUserID\", \"szUserID\", \"szPasswd\") "
           "VALUES (:u, :n, :p); "
           "SET IDENTITY_INSERT \"TACCOUNT_PW\" OFF;",
        soci::use(uid), soci::use(uname), soci::use(pw_hash);

    sql << "INSERT INTO \"TUSERINFOTABLE\" "
           "(\"dwUserID\", \"bCanCreateCharCount\", \"bAgreement\") "
           "VALUES (:u, 6, 1)",
        soci::use(uid);
}

void RunTests(const std::string& conn,
              const std::string& world_conn)
{
    fourstory::db::SessionPool pool(
        fourstory::db::Backend::Odbc, conn, /*pool_size=*/2);
    std::unique_ptr<fourstory::db::SessionPool> world_pool;
    if (!world_conn.empty())
    {
        world_pool = std::make_unique<fourstory::db::SessionPool>(
            fourstory::db::Backend::Odbc, world_conn, /*pool_size=*/2);
    }
    fourstory::db::SessionPool& world_ref =
        world_pool ? *world_pool : pool;

    // PID-scoped uid so parallel runs don't collide. Each login wires
    // up TCURRENTUSER (live-session row); pre-cleanup makes the test
    // idempotent.
    const int test_uid = 2900000 + (::getpid() % 1000);
    const std::string test_user = "wireE2E_" + std::to_string(::getpid());
    const std::string test_pw   = "passw0rd";

    {
        auto lease = pool.Acquire();
        SeedAccount(*lease, test_uid, test_user, test_pw);
    }

    // Wire up SOCI auth + char services. Connection registry tracks
    // per-session state (agreement gate + duplicate-kick). Char
    // service exercises the CHARLIST round-trip via the world pool.
    tloginsvr::services::SociAuthService auth(pool);
    tloginsvr::services::SociCharService chars(pool, world_ref);
    tloginsvr::services::LocalConnectionRegistry registry;

    asio::io_context server_io;
    tloginsvr::LoginServerConfig cfg{};
    cfg.port = 0;  // ephemeral
    cfg.auth_service        = &auth;
    cfg.connection_registry = &registry;
    cfg.char_service        = &chars;
    tloginsvr::LoginServer server(server_io, cfg);
    const std::uint16_t port = server.Port();
    Check(port != 0, "wire+SOCI: server bound to ephemeral port");

    asio::co_spawn(server_io, server.Run(), asio::detached);
    std::thread server_thread([&server_io] { server_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(75));

    // Between subtests we delete the live TCURRENTUSER row so the
    // next login isn't bounced as Duplicate.
    auto clear_session = [&] {
        auto lease = pool.Acquire();
        *lease << "DELETE FROM \"TCURRENTUSER\" WHERE \"dwUserID\" = :u",
            soci::use(test_uid);
    };

    // 1. Happy path — real wire packet through real SOCI auth.
    {
        const auto r = SendLoginAndGetResult(port, test_user, test_pw);
        Check(r == 0,
            "real client + real DB + real password → LR_SUCCESS (0)");
        clear_session();
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

    // 4. Wire CHARLIST round-trip on the same session as the login.
    //    Validates: per-session agreement gate flipped by login
    //    handler on AuthStatus::Success, CharList handler dispatch,
    //    SociCharService::List against MSSQL, CS_CHARLIST_ACK
    //    serialization. Fresh test user has zero chars → bCount=0.
    {
        const auto multi = SendLoginThenCharList(port, test_user, test_pw);
        Check(multi.got_login,
            "multi-packet: LOGIN_ACK received");
        Check(multi.login_result == 0,
            "multi-packet: LOGIN_ACK status LR_SUCCESS");
        Check(multi.got_charlist,
            "multi-packet: CHARLIST_ACK received");
        Check(multi.charlist_count == 0,
            "multi-packet: CHARLIST_ACK bCount == 0 (no seeded chars)");
        clear_session();
    }

    // 5. Wire CREATECHAR round-trip end-to-end. Validates the full G9
    //    port behind the wire dispatcher: LOGIN → CHARLIST flips the
    //    agreement gate, then CREATECHAR_REQ runs SociCharService::
    //    Create against MSSQL (TCHARTABLE OUTPUT INSERTED for the
    //    IDENTITY-assigned dwCharID, plus TINVENTABLE / TTITLETABLE /
    //    TCABINETTABLE / TSKILLTABLE / THOTKEYTABLE / TPOSTTABLE /
    //    TRECALLMONTABLE / TPETTABLE side effects), and the handler
    //    serializes CS_CREATECHAR_ACK with status + char_id back.
    //    Char name limited to IsValidCharName (alphanum, ≤16 chars).
    const std::string char_name =
        "WirE" + std::to_string(::getpid() % 100000);
    {
        const auto multi = SendLoginCharListCreate(
            port, test_user, test_pw, char_name);
        Check(multi.got_login,
            "create: LOGIN_ACK received");
        Check(multi.login_result == 0,
            "create: LOGIN_ACK status LR_SUCCESS");
        Check(multi.got_charlist,
            "create: CHARLIST_ACK received (agreement gate ok)");
        Check(multi.got_create,
            "create: CREATECHAR_ACK received");
        Check(multi.create_status == 0,
            "create: CREATECHAR_ACK status CR_SUCCESS");
        Check(multi.create_char_id > 0,
            "create: CREATECHAR_ACK char_id assigned by IDENTITY");
        clear_session();
    }

    server_io.stop();
    if (server_thread.joinable()) server_thread.join();

    // Cleanup. World-pool cleanup runs first — Create dropped rows
    // into TCHARTABLE plus a dozen per-char side tables. Walk them
    // in FK order. Use the test name as a LIKE prefix so any half-
    // committed previous run is also caught.
    const std::string char_like = "WirE%";
    try
    {
        auto wlease = world_ref.Acquire();
        soci::session& wsql = *wlease;
        const char* per_char_tables[] = {
            "TINVENTABLE", "TTITLETABLE", "TCABINETTABLE",
            "TSKILLTABLE", "THOTKEYTABLE", "TPOSTTABLE",
            "TRECALLMONTABLE", "TPETTABLE",
        };
        for (const char* tbl : per_char_tables)
        {
            try
            {
                std::string q =
                    std::string("DELETE FROM \"") + tbl +
                    "\" WHERE \"dwCharID\" IN (SELECT \"dwCharID\" "
                    "FROM \"TCHARTABLE\" WHERE \"szNAME\" LIKE :p)";
                wsql << q, soci::use(char_like);
            }
            catch (const std::exception&) { /* optional on dev */ }
        }
        try
        {
            wsql << "DELETE FROM \"TDBITEMINDEXTABLE\"";
        }
        catch (const std::exception&) { /* singleton counter, best-effort */ }
        wsql << "DELETE FROM \"TCHARTABLE\" WHERE \"szNAME\" LIKE :p",
            soci::use(char_like);
    }
    catch (const std::exception& ex)
    {
        std::printf("  WARN  world cleanup error (non-fatal): %s\n", ex.what());
    }

    try
    {
        auto lease = pool.Acquire();
        soci::session& sql = *lease;
        sql << "DELETE FROM \"TLOG\"          WHERE \"dwUserID\" = :u", soci::use(test_uid);
        sql << "DELETE FROM \"TCURRENTUSER\"  WHERE \"dwUserID\" = :u", soci::use(test_uid);
        sql << "DELETE FROM \"TUSERINFOTABLE\" WHERE \"dwUserID\" = :u", soci::use(test_uid);
        sql << "DELETE FROM \"TACCOUNT_PW\"   WHERE \"dwUserID\" = :u", soci::use(test_uid);
        try
        {
            sql << "DELETE FROM \"TALLCHARTABLE\"  WHERE \"szName\" LIKE :p",
                soci::use(char_like);
        }
        catch (const std::exception&) { /* optional */ }
        try
        {
            sql << "DELETE FROM \"TRESERVEDNAME\" WHERE \"szName\" LIKE :p",
                soci::use(char_like);
        }
        catch (const std::exception&) { /* optional */ }
        try
        {
            sql << "DELETE FROM \"TKEEPINGNAME\"  WHERE \"szName\" LIKE :p",
                soci::use(char_like);
        }
        catch (const std::exception&) { /* optional */ }
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

    const char* world_conn_cstr = std::getenv("TLOGINSVR_TEST_MSSQL_WORLD_CONN");
    const std::string world_conn =
        (world_conn_cstr != nullptr && world_conn_cstr[0] != '\0')
            ? world_conn_cstr : "";

    try
    {
        RunTests(conn, world_conn);
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  exception: %s\n", ex.what());
        ++g_failed;
    }

    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
