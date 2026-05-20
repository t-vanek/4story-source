// Characterization test for CS_CONREADY_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:402-415
//
// Wire shape: empty body. CS_CONREADY_REQ is the client's "I've parsed
// the CS_CONNECT_ACK and I'm ready to receive map data" signal. The
// legacy handler is a 3-line state machine over the per-session
// `m_bExit` / `m_pMAP` / `m_bMain` flags.
//
// Branches in legacy OnCS_CONREADY_REQ:
//
//   §1  CSHandler.cpp:406  pPlayer->m_bExit == TRUE
//       → drop (do nothing). The session is on its way out; map init
//         would race with the close.
//       → modern: ACTIVE — covered by the "before CONNECT" branch (no
//         valid map session ⇒ no map init).
//
//   §2  CSHandler.cpp:408-409  pPlayer->m_pMAP == nullptr (first time)
//       → InitMap(pPlayer) — allocates TMap for the player, registers
//         in cell grid, spawns NPCs/mons in AOI, fans out
//         CS_ADDCONNECT_ACK + CS_CHARINFO_ACK. The .NET rewrite calls
//         this the MapInitOrchestrator flow.
//       → modern §2b ACTIVE (F2b): when IPlayerService is wired,
//         snapshot is loaded at CS_CONNECT_REQ time and CHARINFO_ACK
//         is sent on CONREADY (standalone path, no TWorldSvr).
//         AOI broadcast (CS_ADDCONNECT_ACK) is PENDING F3.
//
//   §3  CSHandler.cpp:410-411  pPlayer->m_pMAP != null && m_bMain
//       → pMAP->EnterMAP(pPlayer, FALSE) — re-enter on map handoff.
//       → modern: PENDING — requires F3 map state.
//
//   §4  CSHandler.cpp:412-413  pPlayer->m_pMAP != null && !m_bMain
//       → drop (do nothing). Player is on a sub-map / pet AI path.
//       → modern: PENDING — requires F3.
//
// Wire-observable behavior of §1 (active): no ACK, no state change,
// session stays alive.

#include "handlers.h"
#include "map_server.h"
#include "services/fake_session_validator.h"
#include "services/player_service.h"
#include "wire_codec.h"

#include "asio_session.h"
#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

namespace asio = boost::asio;
using boost::asio::ip::tcp;
using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToMessageId;
using tnetlib::protocol::ToUint16;

namespace {

int g_passed  = 0;
int g_failed  = 0;
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

// =====================================================================
// §1  CONREADY before CONNECT → drop, no state change
// =====================================================================
void TestConReadyBeforeConnect()
{
    std::printf("[§1 CONREADY before CONNECT → silent drop  "
                "(CSHandler.cpp:406, m_bExit fallthrough)]\n");

    asio::io_context io;
    tmapsvr::FakeMapSessionValidator validator;
    validator.SetAcceptAll(true);

    tmapsvr::MapServerConfig cfg{};
    cfg.port              = 0;
    cfg.validator         = &validator;
    cfg.accepted_versions = { 0x2918 };
    cfg.pre_auth_timeout  = std::chrono::seconds(10);

    tmapsvr::MapServer server(io, cfg);
    const auto port = server.Port();

    asio::co_spawn(io, server.Run(), asio::detached);
    std::thread srv_thread([&io] { io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    asio::io_context client_io;
    tcp::socket sock(client_io);
    sock.connect(
        tcp::endpoint(asio::ip::make_address_v4("127.0.0.1"), port));

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(sock), tnetlib::PeerType::Server);

    std::atomic<int>  received_count{ 0 };
    std::atomic<bool> closed{ false };

    asio::co_spawn(client_io,
        [sess, &received_count, &closed]() -> asio::awaitable<void> {
            try {
                co_await sess->RunPackets(
                    [&received_count](const tnetlib::DecodedPacket&) {
                        received_count.fetch_add(1);
                    });
            } catch (...) {}
            closed.store(true);
        },
        asio::detached);

    asio::co_spawn(client_io,
        [sess]() -> asio::awaitable<void> {
            std::vector<std::byte> empty_body;
            co_await sess->SendPacket(
                ToUint16(MessageId::CS_CONREADY_REQ),
                std::span<const std::byte>(empty_body.data(),
                                            empty_body.size()));
        },
        asio::detached);

    std::thread client_thread([&client_io] { client_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    Check(received_count.load() == 0,
        "no ACK sent (legacy returns EC_NOERROR silently)");
    Check(!closed.load(),
        "session stays open (legacy doesn't close on stray CONREADY)");

    sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    io.stop();
    if (srv_thread.joinable()) srv_thread.join();
}

// =====================================================================
// §2b  CONREADY after valid CONNECT + IPlayerService loaded snapshot
//      → CS_CHARINFO_ACK  (standalone path, F2b Part 2)
// =====================================================================
void TestCharInfoAckSent()
{
    std::printf("[§2b CONREADY with snapshot → CS_CHARINFO_ACK sent "
                "(CSHandler.cpp:408-409, standalone)]\n");

    // Build a fake character for the server to return.
    constexpr std::uint32_t kUser  = 100u;
    constexpr std::uint32_t kChar  = 42u;
    constexpr std::uint32_t kKey   = 0xABCDu;
    constexpr std::uint16_t kVer   = 0x2918u;

    tmapsvr::FakePlayerService player_svc;
    {
        tmapsvr::legacy::CharSnapshot snap{};
        snap.char_id = kChar;
        snap.user_id = kUser;
        snap.dw_key  = kKey;
        snap.name    = "HeroChar";
        snap.level   = 50;
        snap.exp     = 123000u;
        snap.hp      = 8000u;
        snap.mp      = 4000u;
        snap.gold    = 9999u;
        snap.appearance.race       = 1;
        snap.appearance.char_class = 3;
        snap.position.map_id = 201;
        snap.position.pos_x  = 3664.0f;
        snap.position.pos_z  = 557.0f;
        player_svc.AddChar(std::move(snap));
    }

    asio::io_context io;
    tmapsvr::FakeMapSessionValidator validator;
    validator.SetAcceptAll(true);

    tmapsvr::MapServerConfig cfg{};
    cfg.port              = 0;
    cfg.validator         = &validator;
    cfg.accepted_versions = { kVer };
    cfg.pre_auth_timeout  = std::chrono::seconds(10);
    cfg.player_service    = &player_svc;

    tmapsvr::MapServer server(io, cfg);
    const auto port = server.Port();

    asio::co_spawn(io, server.Run(), asio::detached);
    std::thread srv_thread([&io] { io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    asio::io_context client_io;
    tcp::socket sock(client_io);
    sock.connect(
        tcp::endpoint(asio::ip::make_address_v4("127.0.0.1"), port));

    auto sess = std::make_shared<tnetlib::AsioSession>(
        std::move(sock), tnetlib::PeerType::Server);

    std::atomic<int>  received_count{ 0 };
    std::atomic<bool> got_charinfo{ false };
    std::atomic<bool> closed{ false };

    asio::co_spawn(client_io,
        [sess, &received_count, &got_charinfo, &closed]()
            -> asio::awaitable<void> {
            try {
                co_await sess->RunPackets(
                    [&](const tnetlib::DecodedPacket& pkt) {
                        received_count.fetch_add(1);
                        if (ToMessageId(pkt.wId) == MessageId::CS_CHARINFO_ACK)
                            got_charinfo.store(true);
                    });
            } catch (...) {}
            closed.store(true);
        },
        asio::detached);

    // Step 1: send CS_CONNECT_REQ
    asio::co_spawn(client_io,
        [sess, kVer, kUser, kChar, kKey]() -> asio::awaitable<void> {
            std::vector<std::byte> body;
            wire::WritePOD<std::uint16_t>(body, kVer);     // version
            wire::WritePOD<std::uint8_t>(body, 1);          // channel
            wire::WritePOD<std::uint32_t>(body, kUser);     // user_id
            wire::WritePOD<std::uint32_t>(body, kChar);     // char_id
            wire::WritePOD<std::uint32_t>(body, kKey);      // dw_key
            wire::WritePOD<std::uint32_t>(body, 0);         // ip_addr
            wire::WritePOD<std::uint16_t>(body, 0);         // port
            wire::WritePOD<std::int64_t>(body, 0);          // checksum
            co_await sess->SendPacket(
                ToUint16(MessageId::CS_CONNECT_REQ),
                std::span<const std::byte>(body.data(), body.size()));
        },
        asio::detached);

    std::thread client_thread([&client_io] { client_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // At this point we should have received CS_CONNECT_ACK (count >= 1)
    const int after_connect = received_count.load();
    Check(after_connect >= 1,
        "CS_CONNECT_ACK received after CS_CONNECT_REQ");

    // Step 2: send CS_CONREADY_REQ
    asio::co_spawn(client_io,
        [sess]() -> asio::awaitable<void> {
            std::vector<std::byte> empty;
            co_await sess->SendPacket(
                ToUint16(MessageId::CS_CONREADY_REQ),
                std::span<const std::byte>(empty.data(), empty.size()));
        },
        asio::detached);

    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    Check(got_charinfo.load(),
        "CS_CHARINFO_ACK received after CS_CONREADY_REQ "
        "(snapshot loaded at connect time)");
    Check(!closed.load(),
        "session stays open after CHARINFO_ACK");

    sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    io.stop();
    if (srv_thread.joinable()) srv_thread.join();
}

// =====================================================================
// §2  InitMap full path (AOI broadcast)
// =====================================================================
void TestInitMap_PENDING()
{
    Pending("CONREADY first time → InitMap AOI broadcast (CS_ADDCONNECT_ACK)",
            "CSHandler.cpp:408-409 — requires F3 IMapState + ICellGrid");
}

// =====================================================================
// §3  EnterMAP (re-enter on handoff)
// =====================================================================
void TestReEnterMap_PENDING()
{
    Pending("CONREADY after handoff → EnterMAP",
            "CSHandler.cpp:410-411 — requires F3 IMapState");
}

// =====================================================================
// §4  pMAP set but not main (sub-map / pet AI path)
// =====================================================================
void TestSubMapPath_PENDING()
{
    Pending("CONREADY on non-main session → silent drop",
            "CSHandler.cpp:412-413 — folded into §1 once F3 lands");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_CONREADY_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:402-415\n\n");
    try
    {
        TestConReadyBeforeConnect();
        TestCharInfoAckSent();
        TestInitMap_PENDING();
        TestReEnterMap_PENDING();
        TestSubMapPath_PENDING();
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
