// Characterization test for DM_LOADCHAR_REQ (WorldSvr → MapSvr).
//
// Source of truth: Server/TMapSvr/SSHandler.cpp:3311-3445
//
// Wire shape: DWORD dwCharID, DWORD dwKEY, DWORD dwUserID (12 bytes).
//
// Branches:
//
//   §1  SSHandler.cpp:3311  malformed body (< 12 bytes)
//       → silent drop (no ACK sent).
//       → modern: ACTIVE.
//
//   §2  SSHandler.cpp:3320  player_service returns nullopt (CN_NOCHAR)
//       → world_client->SendDmLoadCharAck(char_id, key, nullptr).
//       → modern: ACTIVE.
//
//   §3  SSHandler.cpp:3325  player_service returns snapshot (CN_SUCCESS)
//       → world_client->SendDmLoadCharAck(char_id, key, &snap).
//       → modern: ACTIVE.
//
//   §4  world_client == nullptr
//       → drop (log-and-discard — no world to respond to).
//       → modern: ACTIVE.
//
// The test drives OnDmLoadCharReq directly (not via the wire dispatcher)
// so no sockets are needed — pure in-memory.

#include "handlers_world.h"
#include "services/player_service.h"
#include "services/world_client.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <cstdio>
#include <exception>
#include <vector>

namespace {

int g_passed  = 0;
int g_failed  = 0;
int g_pending = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS     %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL     %s\n", label); }
}

void Pending(const char* label, const char* ref)
{
    ++g_pending;
    std::printf("  PENDING  %s   (%s)\n", label, ref);
}

// Build a DM_LOADCHAR_REQ wire body from (char_id, dw_key, user_id).
tnetlib::DecodedPacket MakeLoadCharReq(std::uint32_t char_id,
                                       std::uint32_t dw_key,
                                       std::uint32_t user_id)
{
    tnetlib::DecodedPacket pkt{};
    pkt.wId = static_cast<std::uint16_t>(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::DM_LOADCHAR_REQ));
    auto push = [&](std::uint32_t v) {
        for (int i = 0; i < 4; ++i)
            pkt.body.push_back(static_cast<std::byte>((v >> (8 * i)) & 0xFF));
    };
    push(char_id);
    push(dw_key);
    push(user_id);
    return pkt;
}

// Run a coroutine synchronously on an io_context.
template<typename Coro>
void RunSync(Coro coro)
{
    boost::asio::io_context io;
    boost::asio::co_spawn(io, std::move(coro), boost::asio::detached);
    io.run();
}

// =====================================================================
// §1  Malformed body (< 12 bytes)
// =====================================================================
void TestMalformedBody()
{
    std::printf("[§1 DM_LOADCHAR_REQ malformed body → silent drop]\n");

    tmapsvr::FakeWorldClient  world;
    tmapsvr::FakePlayerService player;
    tmapsvr::WorldHandlerContext ctx{};
    ctx.player_service = &player;
    ctx.world_client   = &world;

    tnetlib::DecodedPacket pkt{};
    pkt.wId = 0;
    // Only 4 bytes — too short to hold all 3 DWORDs
    pkt.body = { std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0} };

    RunSync(tmapsvr::OnDmLoadCharReq(nullptr, pkt, ctx));

    Check(world.LoadCharAckCalls().empty(),
        "§1 no DM_LOADCHAR_ACK sent on malformed body");
    Check(world.AddCharAckCalls().empty(),
        "§1 no MW_ADDCHAR_ACK sent on malformed body");
}

// =====================================================================
// §2  Player not found → CN_NOCHAR
// =====================================================================
void TestCharNotFound()
{
    std::printf("[§2 DM_LOADCHAR_REQ char not found → CN_NOCHAR ACK]\n");

    tmapsvr::FakeWorldClient   world;
    tmapsvr::FakePlayerService player;  // empty — no chars registered
    tmapsvr::WorldHandlerContext ctx{};
    ctx.player_service = &player;
    ctx.world_client   = &world;

    auto pkt = MakeLoadCharReq(42, 0xABCD, 100);
    RunSync(tmapsvr::OnDmLoadCharReq(nullptr, pkt, ctx));

    Check(world.LoadCharAckCalls().size() == 1,
        "§2 DM_LOADCHAR_ACK sent");
    if (!world.LoadCharAckCalls().empty())
    {
        const auto& c = world.LoadCharAckCalls().front();
        Check(c.char_id == 42u,    "§2 char_id propagated to ACK");
        Check(c.dw_key  == 0xABCDu, "§2 dw_key propagated to ACK");
        Check(!c.success,           "§2 ACK carries CN_NOCHAR (success=false)");
    }
}

// =====================================================================
// §3  Player found → CN_SUCCESS
// =====================================================================
void TestCharFound()
{
    std::printf("[§3 DM_LOADCHAR_REQ char found → CN_SUCCESS ACK]\n");

    tmapsvr::FakeWorldClient   world;
    tmapsvr::FakePlayerService player;

    tmapsvr::legacy::CharSnapshot snap{};
    snap.char_id = 99u;
    snap.user_id = 200u;
    snap.dw_key  = 0xBEEFu;
    snap.name    = "Paladin";
    snap.level   = 55;
    player.AddChar(snap);

    tmapsvr::WorldHandlerContext ctx{};
    ctx.player_service = &player;
    ctx.world_client   = &world;

    auto pkt = MakeLoadCharReq(99, 0xBEEF, 200);
    RunSync(tmapsvr::OnDmLoadCharReq(nullptr, pkt, ctx));

    Check(world.LoadCharAckCalls().size() == 1,
        "§3 DM_LOADCHAR_ACK sent");
    if (!world.LoadCharAckCalls().empty())
    {
        const auto& c = world.LoadCharAckCalls().front();
        Check(c.char_id == 99u,     "§3 char_id in ACK");
        Check(c.dw_key  == 0xBEEFu, "§3 dw_key in ACK");
        Check(c.success,             "§3 ACK carries CN_SUCCESS (success=true)");
    }
}

// =====================================================================
// §4  world_client == nullptr → drop
// =====================================================================
void TestNoWorldClient()
{
    std::printf("[§4 DM_LOADCHAR_REQ with null world_client → drop]\n");

    tmapsvr::FakePlayerService player;
    tmapsvr::WorldHandlerContext ctx{};
    ctx.player_service = &player;
    ctx.world_client   = nullptr;  // no world client

    auto pkt = MakeLoadCharReq(1, 1, 1);
    // Should not throw
    RunSync(tmapsvr::OnDmLoadCharReq(nullptr, pkt, ctx));
    Check(true, "§4 no exception thrown when world_client is null");
}

// =====================================================================
// FakeWorldClient contract tests
// =====================================================================
void TestFakeWorldClientContract()
{
    std::printf("[FakeWorldClient — record / inspect contract]\n");

    tmapsvr::FakeWorldClient w;

    Check(w.AddCharAckCalls().empty(), "initially no AddCharAck calls");
    Check(w.LoadCharAckCalls().empty(), "initially no LoadCharAck calls");
    Check(w.IsConnected(), "default IsConnected() = true");

    w.SendMwAddCharAck(10, 0xAA, 0x7F000001u, 5815, 100);
    Check(w.AddCharAckCalls().size() == 1,
        "AddCharAck call recorded");
    Check(w.AddCharAckCalls()[0].char_id == 10u,
        "char_id propagated");
    Check(w.AddCharAckCalls()[0].user_id == 100u,
        "user_id propagated");

    w.SendDmLoadCharAck(20, 0xBB, nullptr);
    Check(w.LoadCharAckCalls().size() == 1,
        "LoadCharAck(nullptr) call recorded");
    Check(!w.LoadCharAckCalls()[0].success,
        "success=false for nullptr snap");

    tmapsvr::legacy::CharSnapshot snap{};
    snap.char_id = 30; snap.user_id = 50; snap.dw_key = 0xCC;
    w.SendDmLoadCharAck(30, 0xCC, &snap);
    Check(w.LoadCharAckCalls().size() == 2,
        "LoadCharAck(&snap) call recorded");
    Check(w.LoadCharAckCalls()[1].success,
        "success=true for non-null snap");

    w.SetConnected(false);
    Check(!w.IsConnected(), "SetConnected(false) works");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  OnDmLoadCharReq characterization spec ===\n");
    std::printf("    Source: Server/TMapSvr/SSHandler.cpp:3311-3445\n\n");
    try
    {
        TestMalformedBody();
        TestCharNotFound();
        TestCharFound();
        TestNoWorldClient();
        TestFakeWorldClientContract();
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
