// Characterization test for CS_CONNECT_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:249-399
//                  Server/TMapSvr/CSSender.cpp:78-102  (ACK shape)
//                  Server/TMapSvr/SSHandler.cpp:1332-1381 (CN_SUCCESS path)
//                  Lib/Own/TProtocol/include/NetCode.h:319-328 (CN_* enum)
//
// Wire shape of CS_CONNECT_REQ body (legacy CSHandler.cpp:253-283):
//   WORD  wVersion
//   BYTE  bChannel
//   DWORD dwUserID
//   DWORD dwID         (= dwCharID)
//   DWORD dwKEY
//   DWORD dwIPAddr     (legacy ignores server-side)
//   WORD  wPort        (legacy ignores server-side)
//   INT64 llChecksum   (legacy computes magic-based checksum; modern trusts dwKEY)
//
// Wire shape of CS_CONNECT_ACK body (legacy CSSender.cpp:78-102):
//   BYTE  bResult     ← CN_SUCCESS | CN_NOCHANNEL | CN_NOCHAR
//                       | CN_ALREADYEXIST | CN_INVALIDVER | CN_INTERNAL
//   BYTE  bSvrCount
//   bSvrCount × BYTE   server-id list (only meaningful on CN_SUCCESS)
//
// Branches in legacy OnCS_CONNECT_REQ:
//
//   §1  CSHandler.cpp:258-262  bad wVersion
//       → SendCS_CONNECT_ACK(CN_INVALIDVER) + return EC_SESSION_INVALIDCHAR
//       → modern: ACTIVE
//
//   §2  CSHandler.cpp:298-299  llChecksum1 != llChecksum2
//       → return EC_SESSION_INVALIDCHAR (NO ack to client)
//       → modern: PENDING — F1 ignores the legacy checksum-of-checksum
//         field entirely (we trust dwKEY → TCURRENTUSER lookup instead).
//         Legacy checksum is wire obfuscation, not crypto; it's there
//         to filter random/buggy clients. F4+ may re-add it as a wire
//         parity check once we have the magic table extracted.
//
//   §3  CSHandler.cpp:301-302  pPlayer->m_dwID != 0
//       → return EC_SESSION_INVALIDCHAR (duplicate CONNECT on same socket)
//       → modern: ACTIVE — duplicate detection via MapSessionState.connected
//
//   §4  CSHandler.cpp:305-320  CSPCheckMapChar fails (DEAD CODE in legacy)
//       The SP call is commented out in the shipped source; the
//       surrounding bRet=FALSE init means this branch is never taken
//       in production. Modern restores the semantic via SOCI lookup
//       against TCURRENTUSER, mapped onto CN_INTERNAL.
//       → modern: ACTIVE (mapped to CN_INTERNAL on validator deny)
//
//   §5  CSHandler.cpp:322-330  same-session duplicate (pPlayer == itCHAR)
//       → log "Trying to crash my server, huh?" + return EC_SESSION_INVALIDCHAR
//       → modern: PENDING — requires per-session-state introspection;
//         covered conceptually by §3 (same socket).
//
//   §6  CSHandler.cpp:333-355  duplicate dwID, old session exited, in suspender
//       → kick old; promote new to suspender; set bCheckedSession;
//         return EC_NOERROR (NO immediate ACK — comes from CS_CONREADY_REQ)
//       → modern: PENDING — F3 introduces map state + the suspender concept.
//
//   §7  CSHandler.cpp:357-361  duplicate dwID, old session NOT exited
//       → set old.m_bCloseAll=TRUE; new.m_bCloseAll=FALSE;
//         return EC_SESSION_INVALIDCHAR (NO ack)
//       → modern: PENDING — F3 introduces cluster-wide player registry.
//
//   §8  CSHandler.cpp:364-399  happy path
//       → set player state, insert into m_mapPLAYER, SendMW_ADDCHAR_ACK
//         to World, return EC_NOERROR. CS_CONNECT_ACK to client comes
//         later from OnMW_CONRESULT_REQ (SSHandler.cpp:1371).
//       → modern: PARTIAL — F1 sends CN_SUCCESS immediately (skips the
//         MW round-trip). F2b will reshape this to match legacy: defer
//         the ACK until MW_CONRESULT_REQ lands.

#include "config.h"
#include "handlers.h"
#include "map_server.h"
#include "services/fake_session_validator.h"
#include "wire_codec.h"

#include "asio_session.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace asio = boost::asio;
using boost::asio::ip::tcp;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

namespace {

int g_passed = 0;
int g_failed = 0;
int g_pending = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS     %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL     %s\n", label); }
}

void Pending(const char* label, const char* legacy_ref)
{
    ++g_pending;
    std::printf("  PENDING  %s   (%s)\n", label, legacy_ref);
}

// Build the legacy CS_CONNECT_REQ body byte-for-byte. Matches CSHandler.cpp:253-283.
std::vector<std::byte>
BuildConnectReq(std::uint16_t version, std::uint8_t channel,
                std::uint32_t user_id, std::uint32_t char_id,
                std::uint32_t dw_key)
{
    std::vector<std::byte> body;
    tmapsvr::wire::WritePOD<std::uint16_t>(body, version);
    tmapsvr::wire::WritePOD<std::uint8_t> (body, channel);
    tmapsvr::wire::WritePOD<std::uint32_t>(body, user_id);
    tmapsvr::wire::WritePOD<std::uint32_t>(body, char_id);
    tmapsvr::wire::WritePOD<std::uint32_t>(body, dw_key);
    tmapsvr::wire::WritePOD<std::uint32_t>(body, 0u);   // dwIPAddr — ignored
    tmapsvr::wire::WritePOD<std::uint16_t>(body, 0u);   // wPort    — ignored
    tmapsvr::wire::WritePOD<std::int64_t> (body, 0);    // llChecksum — F1 trusts dwKEY
    return body;
}

struct AckParts { std::uint8_t result; std::uint8_t svr_count; };

// Run a single handshake: connect, send one CS_CONNECT_REQ, optionally
// send a second packet, return any received ACKs (in order).
struct HandshakeResult
{
    std::vector<AckParts> acks;
    bool                  socket_closed_by_peer = false;
};

HandshakeResult
RunHandshake(std::uint16_t server_port,
             const std::vector<std::byte>& first_body,
             const std::vector<std::byte>* second_body = nullptr,
             std::chrono::milliseconds wait_for = std::chrono::milliseconds(500))
{
    asio::io_context client_io;
    tcp::socket sock(client_io);
    boost::system::error_code ec;
    sock.connect(
        tcp::endpoint(asio::ip::make_address_v4("127.0.0.1"), server_port),
        ec);
    HandshakeResult out;
    if (ec) return out;

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(sock), tnetlib::PeerType::Server);

    std::atomic<int> received_count{ 0 };
    std::atomic<bool> closed{ false };

    asio::co_spawn(client_io,
        [sess, &out, &received_count, &closed]() -> asio::awaitable<void> {
            try {
                co_await sess->RunPackets(
                    [&out, &received_count](const tnetlib::DecodedPacket& pkt) {
                        if (pkt.body.size() >= 2)
                        {
                            AckParts a{};
                            a.result    = static_cast<std::uint8_t>(pkt.body[0]);
                            a.svr_count = static_cast<std::uint8_t>(pkt.body[1]);
                            out.acks.push_back(a);
                        }
                        received_count.fetch_add(1);
                    });
            } catch (...) {}
            closed.store(true);
        },
        asio::detached);

    asio::co_spawn(client_io,
        [sess, first_body, second_body]() -> asio::awaitable<void> {
            co_await sess->SendPacket(
                ToUint16(MessageId::CS_CONNECT_REQ),
                std::span<const std::byte>(first_body.data(), first_body.size()));
            if (second_body)
            {
                co_await sess->SendPacket(
                    ToUint16(MessageId::CS_CONNECT_REQ),
                    std::span<const std::byte>(second_body->data(), second_body->size()));
            }
        },
        asio::detached);

    std::thread t([&client_io] { client_io.run(); });
    const auto deadline = std::chrono::steady_clock::now() + wait_for;
    while (!closed.load() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

    sess->Close();
    client_io.stop();
    if (t.joinable()) t.join();
    out.socket_closed_by_peer = closed.load();
    return out;
}

// CN_* enum from NetCode.h:319-328. Duplicated locally so the test
// doesn't pull legacy TProtocol headers.
constexpr std::uint8_t CN_SUCCESS    = 0;
constexpr std::uint8_t CN_NOCHANNEL  = 1;
constexpr std::uint8_t CN_INVALIDVER = 4;
constexpr std::uint8_t CN_INTERNAL   = 5;

// Fixture: spin up MapServer with a configurable validator + version
// gate, return the bound port.
struct ServerFixture
{
    asio::io_context                              io;
    tmapsvr::FakeMapSessionValidator              validator;
    std::unique_ptr<tmapsvr::MapServer>           server;
    std::thread                                   thread;

    explicit ServerFixture(bool accept_all,
                           std::vector<std::uint16_t> versions = { 0x2918 })
    {
        validator.SetAcceptAll(accept_all);

        tmapsvr::MapServerConfig cfg{};
        cfg.port              = 0;  // ephemeral
        cfg.validator         = &validator;
        cfg.accepted_versions = std::move(versions);
        server = std::make_unique<tmapsvr::MapServer>(io, cfg);

        asio::co_spawn(io, server->Run(), asio::detached);
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
// §1  bad wVersion → CN_INVALIDVER (CSHandler.cpp:258-262)
// =====================================================================
void TestBadVersion()
{
    std::printf("[§1 bad wVersion → CN_INVALIDVER  (CSHandler.cpp:258-262)]\n");
    ServerFixture fx(/*accept_all=*/true);
    const auto body = BuildConnectReq(/*ver=*/0x9999, 0, 1001, 2002, 0xdeadbeef);
    const auto r = RunHandshake(fx.Port(), body);

    Check(r.acks.size() == 1,
        "exactly one CS_CONNECT_ACK comes back");
    if (r.acks.size() == 1)
    {
        Check(r.acks[0].result == CN_INVALIDVER,
            "result == CN_INVALIDVER");
        Check(r.acks[0].svr_count == 0,
            "svr_count == 0 (no server-id list on error)");
    }
    Check(r.socket_closed_by_peer,
        "server closes the socket after the ACK");
}

// =====================================================================
// §2  llChecksum mismatch → close, NO ack (CSHandler.cpp:298-299)
// =====================================================================
void TestChecksumMismatch_PENDING()
{
    Pending("checksum mismatch → close, NO ack",
            "CSHandler.cpp:285-299 — legacy magic-table checksum not ported");
}

// =====================================================================
// §3  duplicate CONNECT on same session → close, NO ack
//     (CSHandler.cpp:301-302  pPlayer->m_dwID != 0)
// =====================================================================
void TestDuplicateOnSameSession()
{
    std::printf("[§3 duplicate connect on same socket  (CSHandler.cpp:301-302)]\n");
    ServerFixture fx(/*accept_all=*/true);
    const auto first  = BuildConnectReq(0x2918, 0, 1001, 2002, 0xdeadbeef);
    const auto second = BuildConnectReq(0x2918, 0, 1001, 2002, 0xdeadbeef);
    const auto r = RunHandshake(fx.Port(), first, &second);

    // First connect → CN_SUCCESS. Second connect on the same socket
    // → modern closes (no ack). So we expect exactly one ACK total.
    Check(r.acks.size() == 1,
        "first CONNECT acked, second CONNECT dropped without ack");
    if (!r.acks.empty())
        Check(r.acks[0].result == CN_SUCCESS,
            "first connect → CN_SUCCESS");
    Check(r.socket_closed_by_peer,
        "server closes the socket after the duplicate");
}

// =====================================================================
// §4  validator deny (modern restoration of CSPCheckMapChar semantic)
//     → CN_INTERNAL (CSHandler.cpp:305-320 — currently dead in legacy)
// =====================================================================
void TestValidatorDeny()
{
    std::printf("[§4 validator deny → CN_INTERNAL  (CSHandler.cpp:305-320)]\n");
    ServerFixture fx(/*accept_all=*/false);
    // Seed a different tuple — the (1001, 2002, deadbeef) we'll send is
    // not in the allow-list, so Validate returns false.
    fx.validator.Seed(/*uid*/9999, /*cid*/9, /*key*/0x12345678);

    const auto body = BuildConnectReq(0x2918, 0, 1001, 2002, 0xdeadbeef);
    const auto r = RunHandshake(fx.Port(), body);

    Check(r.acks.size() == 1,
        "one CS_CONNECT_ACK with the failure code");
    if (r.acks.size() == 1)
        Check(r.acks[0].result == CN_INTERNAL,
            "result == CN_INTERNAL");
    Check(r.socket_closed_by_peer,
        "server closes after the failure ACK");
}

// =====================================================================
// §5  same-session duplicate (legacy logs "Trying to crash my server")
//     This branch is effectively impossible to hit from the wire — it
//     requires two different `dwID` values on the same socket where the
//     player object is reused. Modern path treats §3 as the canonical
//     same-socket dup; §5 is folded into it.
// =====================================================================
void TestSameSessionDuplicate_PENDING()
{
    Pending("same-session duplicate ('Trying to crash')",
            "CSHandler.cpp:322-330 — collapses into §3 on the wire");
}

// =====================================================================
// §6  duplicate dwID across sessions, old in suspender
//     → kick old; new → suspender; NO immediate ack (deferred to
//       CS_CONREADY_REQ).
// =====================================================================
void TestSuspenderFlow_PENDING()
{
    Pending("duplicate-id suspender flow",
            "CSHandler.cpp:333-355 — requires F3 map-wide player registry");
}

// =====================================================================
// §7  duplicate dwID across sessions, old NOT exited
//     → mark old m_bCloseAll; close new with EC_SESSION_INVALIDCHAR.
// =====================================================================
void TestDuplicateIdLiveSession_PENDING()
{
    Pending("duplicate-id, old still alive",
            "CSHandler.cpp:357-361 — requires F3 map-wide player registry");
}

// =====================================================================
// §8  happy path → MW_ADDCHAR_REQ to World; CS_CONNECT_ACK deferred
//     until OnMW_CONRESULT_REQ (SSHandler.cpp:1371).
//
// F1 deviation: modern sends CS_CONNECT_ACK(CN_SUCCESS) immediately
// without going through World. We assert the F1 behavior here and
// mark the legacy-parity gap as MODERN-MISMATCH for F2b to close.
// =====================================================================
void TestHappyPath()
{
    std::printf("[§8 happy path → CN_SUCCESS  (CSHandler.cpp:364-399 / SSHandler.cpp:1371)]\n");
    ServerFixture fx(/*accept_all=*/true);
    const auto body = BuildConnectReq(0x2918, 0, 1001, 2002, 0xdeadbeef);
    const auto r = RunHandshake(fx.Port(), body);

    Check(r.acks.size() == 1,
        "exactly one CS_CONNECT_ACK comes back (F1 — see MODERN-MISMATCH)");
    if (r.acks.size() == 1)
    {
        Check(r.acks[0].result == CN_SUCCESS,
            "result == CN_SUCCESS");
        Check(r.acks[0].svr_count == 0,
            "svr_count == 0 — F1 doesn't push the server-id list yet");
    }
    // MODERN-MISMATCH (CSHandler.cpp:388-393 vs F1 OnConnectReq):
    //   Legacy:  sends MW_ADDCHAR_ACK to World, then waits for
    //            MW_CONRESULT_REQ which carries vServerID + bResult,
    //            THEN forwards CS_CONNECT_ACK to client with that
    //            vServerID list.
    //   Modern (F1): sends CS_CONNECT_ACK(CN_SUCCESS, []) immediately;
    //            no World round-trip; no server-id list.
    //   Resolution: F2b adds an outbound World peer dial + the
    //            DeferAck flow; this test gets a `svr_count > 0`
    //            assertion when the mock-peer fixture lands.
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_CONNECT_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:249-399\n\n");
    try
    {
        TestBadVersion();
        TestChecksumMismatch_PENDING();
        TestDuplicateOnSameSession();
        TestValidatorDeny();
        TestSameSessionDuplicate_PENDING();
        TestSuspenderFlow_PENDING();
        TestDuplicateIdLiveSession_PENDING();
        TestHappyPath();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL     unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed, %d pending\n",
        g_passed, g_failed, g_pending);
    return g_failed == 0 ? 0 : 1;
}
