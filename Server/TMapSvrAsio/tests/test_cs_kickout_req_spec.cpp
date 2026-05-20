// Characterization test for CS_KICKOUT_REQ.
//
// Source of truth: Server/TMapSvr/CSHandler.cpp:417-437
//
// Wire shape (body):
//   DWORD dwCharID — the char to kick from the map
//
// The handler returns EC_SESSION_INVALIDCHAR regardless of outcome,
// which closes the session that issued the kick — meaning a successful
// kick boots both the target AND the caller. That matches the
// legacy's "GM forcibly disconnects, then takes the connection down
// herself" semantic.
//
// Branches in legacy OnCS_KICKOUT_REQ:
//
//   §1  CSHandler.cpp:425-432  target found in m_mapPLAYER
//       → mark target's m_bCloseAll = TRUE + CloseSession(target)
//       → modern: PENDING — requires the F3 map-wide player registry.
//
//   §2  CSHandler.cpp:434  always (success or miss)
//       → SendDM_CLEARCURRENTUSER_REQ(dwCharID) to login DB peer
//         (clears the stale TCURRENTUSER row so the char can reconnect)
//       → modern: PENDING — requires F2b outbound DM peer (Login DB).
//
//   §3  CSHandler.cpp:436  always
//       → return EC_SESSION_INVALIDCHAR (closes the caller)
//       → modern: ACTIVE (covered: stray KICKOUT before CONNECT drops).
//
// Wire-observable for §3 alone: no ACK, session closes. The default
// dispatcher already log-and-drops unknown ids; the difference is
// that legacy CS_KICKOUT_REQ EXPLICITLY closes the caller. F4+ adds
// the per-handler close return value; F1 doesn't have that signal
// yet.
//
// Modern F1 effective behavior: KICKOUT before CONNECT drops silently
// without closing. That's a deviation from legacy §3 (close), but
// it's safe — the pre-auth watchdog will close anyway after 30s, and
// post-CONNECT we don't yet have anything to kick. Recorded as
// MODERN-MISMATCH for F4 to address.

#include "handlers.h"
#include "map_server.h"
#include "services/fake_session_validator.h"
#include "wire_codec.h"

#include "asio_session.h"

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
// §3 only (active fragment) — KICKOUT before any CONNECT
//
// MODERN-MISMATCH: legacy returns EC_SESSION_INVALIDCHAR (closes the
// session); modern silently log-and-drops. We assert the F1 behavior
// here and flag the gap.
// =====================================================================
void TestKickoutBeforeConnect()
{
    std::printf("[§3 KICKOUT before CONNECT  (CSHandler.cpp:436 — "
                "MODERN-MISMATCH for F4)]\n");

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
            // KICKOUT body = DWORD dwCharID. Use an arbitrary id since
            // the F1 dispatch doesn't read it.
            std::vector<std::byte> body;
            tmapsvr::wire::WritePOD<std::uint32_t>(body, 12345u);
            co_await sess->SendPacket(
                ToUint16(MessageId::CS_KICKOUT_REQ),
                std::span<const std::byte>(body.data(), body.size()));
        },
        asio::detached);

    std::thread client_thread([&client_io] { client_io.run(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // F1 behavior: no ACK, session stays open. Legacy would close.
    // Recorded mismatch — F4 reshapes the dispatcher to honor
    // per-handler close-after-drop.
    Check(received_count.load() == 0,
        "no ACK sent (legacy doesn't ACK KICKOUT either)");
    Check(!closed.load(),
        "MODERN-MISMATCH: F1 keeps session open; "
        "legacy CSHandler.cpp:436 closes (EC_SESSION_INVALIDCHAR)");

    sess->Close();
    client_io.stop();
    if (client_thread.joinable()) client_thread.join();
    io.stop();
    if (srv_thread.joinable()) srv_thread.join();
}

void TestTargetFoundClosesIt_PENDING()
{
    Pending("KICKOUT(charID) → close that char's session",
            "CSHandler.cpp:425-432 — requires F3 map-wide player registry");
}

void TestAlwaysClearsLoginRow_PENDING()
{
    Pending("KICKOUT → DM_CLEARCURRENTUSER_REQ to login peer",
            "CSHandler.cpp:434 — requires F2b outbound DM peer");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnCS_KICKOUT_REQ characterization spec ===\n");
    std::printf("    Source of truth: Server/TMapSvr/CSHandler.cpp:417-437\n\n");
    try
    {
        TestKickoutBeforeConnect();
        TestTargetFoundClosesIt_PENDING();
        TestAlwaysClearsLoginRow_PENDING();
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
