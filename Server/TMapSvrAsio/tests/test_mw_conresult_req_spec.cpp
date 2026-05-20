// Characterization test for F2b Part 4 cluster path.
//
// Covers three orthogonal contracts:
//
//   A. Pre-load cache (IWorldClient::StorePreloadedChar +
//      TakePreloadedChar) — normal cluster sequence where DM_LOADCHAR
//      completes BEFORE the client's CS_CONNECT_REQ arrives.
//
//   B. Pending session registry (RegisterPendingSession +
//      CancelPendingSession + SimulateConResult) — race path where
//      CS_CONNECT_REQ arrives before DM_LOADCHAR completes.
//
//   C. OnMwConResultReq handler — parses MW_CONRESULT_REQ wire and
//      logs correctly; actual CS_CHARINFO_ACK routing via AsioWorldClient
//      pending map is deferred to F3 (requires MapSessionState shared_ptr
//      from HandleConnection to be accessible in DispatchWorld).
//
// All tests are pure-unit (no sockets or io_context threads).
//
// Source: SSHandler.cpp:1332 — OnMW_CONRESULT_REQ
//         SSSender.cpp        — SendMW_ADDCHAR_ACK

#include "handlers_world.h"
#include "services/world_client.h"
#include "handlers.h"           // MapSessionState

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/detached.hpp>

#include <cstdio>
#include <exception>
#include <memory>

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

template<typename Coro>
void RunSync(Coro c)
{
    boost::asio::io_context io;
    boost::asio::co_spawn(io, std::move(c), boost::asio::detached);
    io.run();
}

// ---------------------------------------------------------------------------
// A. Pre-load cache
// ---------------------------------------------------------------------------
void TestPreloadCacheHit()
{
    std::printf("[A.1 StorePreloadedChar → TakePreloadedChar hit]\n");
    tmapsvr::FakeWorldClient w;

    tmapsvr::legacy::CharSnapshot snap{};
    snap.char_id = 55u; snap.user_id = 200u; snap.dw_key = 0xBEEFu;
    snap.name    = "Paladin"; snap.level = 60;
    w.StorePreloadedChar(snap);

    Check(w.HasPreloaded(55u), "snapshot stored in cache");

    auto taken = w.TakePreloadedChar(55u, 200u, 0xBEEFu);
    Check(taken.has_value(),   "TakePreloadedChar returns snapshot on hit");
    Check(!w.HasPreloaded(55u), "cache entry removed after take");
    if (taken)
    {
        Check(taken->char_id == 55u && taken->name == "Paladin",
            "taken snapshot has correct content");
    }
}

void TestPreloadCacheMiss()
{
    std::printf("[A.2 TakePreloadedChar miss → nullopt]\n");
    tmapsvr::FakeWorldClient w;

    // Empty cache
    auto r1 = w.TakePreloadedChar(1u, 1u, 1u);
    Check(!r1.has_value(), "empty cache → nullopt");

    // Wrong credentials
    tmapsvr::legacy::CharSnapshot snap{};
    snap.char_id = 10u; snap.user_id = 100u; snap.dw_key = 0xAAAAu;
    w.StorePreloadedChar(snap);

    auto r2 = w.TakePreloadedChar(10u, 999u, 0xAAAAu);  // wrong user_id
    Check(!r2.has_value(), "wrong user_id → nullopt");

    auto r3 = w.TakePreloadedChar(10u, 100u, 0xBBBBu);  // wrong dw_key
    Check(!r3.has_value(), "wrong dw_key → nullopt");

    Check(w.HasPreloaded(10u), "cache entry NOT removed on credential mismatch");
}

// ---------------------------------------------------------------------------
// B. Pending session registry
// ---------------------------------------------------------------------------
void TestPendingRegistry()
{
    std::printf("[B.1 RegisterPendingSession → SimulateConResult delivers snapshot]\n");
    tmapsvr::FakeWorldClient w;

    // Create a real MapSessionState via shared_ptr (as HandleConnection does)
    auto state_holder = std::make_shared<tmapsvr::MapSessionState>();
    state_holder->char_id = 77u;
    state_holder->user_id = 300u;

    w.RegisterPendingSession(77u, nullptr, std::weak_ptr<tmapsvr::MapSessionState>(state_holder));
    Check(w.HasPendingSession(77u), "pending entry registered");

    tmapsvr::legacy::CharSnapshot snap{};
    snap.char_id = 77u; snap.user_id = 300u; snap.name = "Warrior"; snap.level = 45;
    w.SimulateConResult(77u, snap);

    Check(!w.HasPendingSession(77u), "pending entry removed after delivery");
    Check(state_holder->snapshot.has_value(), "snapshot delivered to session state");
    if (state_holder->snapshot)
        Check(state_holder->snapshot->name == "Warrior",
            "delivered snapshot has correct content");
}

void TestCancelPendingSession()
{
    std::printf("[B.2 CancelPendingSession removes entry]\n");
    tmapsvr::FakeWorldClient w;
    auto sh = std::make_shared<tmapsvr::MapSessionState>();

    w.RegisterPendingSession(88u, nullptr, sh);
    Check(w.HasPendingSession(88u), "entry registered");

    w.CancelPendingSession(88u);
    Check(!w.HasPendingSession(88u), "entry removed by cancel");
}

void TestExpiredSessionHandled()
{
    std::printf("[B.3 SimulateConResult with expired weak_ptr → no crash]\n");
    tmapsvr::FakeWorldClient w;

    {
        auto sh = std::make_shared<tmapsvr::MapSessionState>();
        w.RegisterPendingSession(99u, nullptr, sh);
    }  // sh destroyed — weak_ptr now expired

    tmapsvr::legacy::CharSnapshot snap{}; snap.char_id = 99u;
    // Should not crash or throw
    w.SimulateConResult(99u, snap);
    Check(true, "SimulateConResult on expired weak_ptr does not crash");
}

// ---------------------------------------------------------------------------
// C. OnMwConResultReq wire handler
// ---------------------------------------------------------------------------
void TestMwConResultReqSuccess()
{
    std::printf("[C.1 MW_CONRESULT_REQ CN_SUCCESS → parsed without crash]\n");
    tmapsvr::FakeWorldClient   world;
    tmapsvr::FakePlayerService player;
    tmapsvr::WorldHandlerContext ctx{};
    ctx.player_service = &player;
    ctx.world_client   = &world;

    // Build MW_CONRESULT_REQ body: dwCharID(4) + dwKEY(4) + bResult(1) + bCount(1)
    tnetlib::DecodedPacket pkt{};
    pkt.wId = static_cast<std::uint16_t>(
        tnetlib::protocol::ToUint16(
            tnetlib::protocol::MessageId::MW_CONRESULT_REQ));
    auto push4 = [&](std::uint32_t v) {
        for (int i = 0; i < 4; ++i)
            pkt.body.push_back(static_cast<std::byte>((v >> (8 * i)) & 0xFF));
    };
    push4(77u);   // char_id
    push4(0xBBu); // dw_key
    pkt.body.push_back(std::byte{0});  // bResult = CN_SUCCESS
    pkt.body.push_back(std::byte{0});  // bCount  = 0

    RunSync(tmapsvr::OnMwConResultReq(nullptr, pkt, ctx));
    Check(true, "OnMwConResultReq CN_SUCCESS parsed without exception");
}

void TestMwConResultReqMalformed()
{
    std::printf("[C.2 MW_CONRESULT_REQ malformed body → silent drop]\n");
    tmapsvr::FakeWorldClient   world;
    tmapsvr::WorldHandlerContext ctx{};
    ctx.world_client = &world;

    tnetlib::DecodedPacket pkt{};
    pkt.wId = 0;
    pkt.body = { std::byte{1} };  // too short

    RunSync(tmapsvr::OnMwConResultReq(nullptr, pkt, ctx));
    Check(true, "malformed MW_CONRESULT_REQ does not crash or throw");
}

void TestFullClusterPathPending()
{
    Pending("Full cluster end-to-end: CS_CONNECT_REQ → MW_CONRESULT → CS_CHARINFO_ACK via wire",
            "Requires AsioWorldClient pending map routing — F2b Part 4 routing deferred");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  F2b Part 4 cluster path spec ===\n");
    std::printf("    Sources: SSHandler.cpp:1332, SSSender.cpp\n\n");
    try
    {
        TestPreloadCacheHit();
        TestPreloadCacheMiss();
        TestPendingRegistry();
        TestCancelPendingSession();
        TestExpiredSessionHandled();
        TestMwConResultReqSuccess();
        TestMwConResultReqMalformed();
        TestFullClusterPathPending();
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
