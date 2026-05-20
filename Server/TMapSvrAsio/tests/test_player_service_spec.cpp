// Spec test for IPlayerService + FakePlayerService.
//
// Legacy parity reference: Server/TMapSvr/SSHandler.cpp — the
// OnDM_LOADCHAR_REQ handler uses the same (char_id, user_id, dw_key)
// triple for the DB lookup. The fake must mirror that contract exactly.
//
// Branches covered:
//   §1 empty store                  → LoadChar returns nullopt
//   §2 exact match                  → returns CharSnapshot
//   §3 dw_key mismatch              → nullopt (stale session token)
//   §4 user_id mismatch             → nullopt
//   §5 char_id unknown              → nullopt
//   §6 AddChar upserts              → old token rejected, new accepted
//   §7 multiple chars coexist       → each resolves independently
//
// No Asio / sockets infrastructure needed — purely in-memory.

#include "services/player_service.h"

#include <cstdio>
#include <exception>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

tmapsvr::legacy::CharSnapshot MakeSnap(
    std::uint32_t char_id,
    std::uint32_t user_id,
    std::uint32_t key,
    const char*   name  = "Hero",
    std::uint8_t  level = 50)
{
    tmapsvr::legacy::CharSnapshot s{};
    s.char_id = char_id;
    s.user_id = user_id;
    s.dw_key  = key;
    s.name    = name;
    s.level   = level;
    return s;
}

void TestFakePlayerService()
{
    std::printf("[FakePlayerService — LoadChar contract]\n");
    tmapsvr::FakePlayerService svc;

    // §1 Empty store → nullopt
    auto r0 = svc.LoadChar(1, 100, 0xABCD);
    Check(!r0.has_value(),
        "§1 LoadChar on empty store returns nullopt");

    // §2 Exact match → snapshot
    svc.AddChar(MakeSnap(1, 100, 0xABCD));
    auto r1 = svc.LoadChar(1, 100, 0xABCD);
    Check(r1.has_value(),
        "§2 LoadChar finds char after AddChar");
    Check(r1->char_id == 1 && r1->user_id == 100 && r1->dw_key == 0xABCDu,
        "§2 LoadChar returns correct snapshot ids");
    Check(r1->name == "Hero" && r1->level == 50,
        "§2 LoadChar returns correct snapshot content");

    // §3 dw_key mismatch → nullopt
    auto r2 = svc.LoadChar(1, 100, 0x9999);
    Check(!r2.has_value(),
        "§3 LoadChar rejects stale dw_key");

    // §4 user_id mismatch → nullopt
    auto r3 = svc.LoadChar(1, 999, 0xABCD);
    Check(!r3.has_value(),
        "§4 LoadChar rejects wrong user_id");

    // §5 Unknown char_id → nullopt
    auto r4 = svc.LoadChar(2, 100, 0xABCD);
    Check(!r4.has_value(),
        "§5 LoadChar returns nullopt for unknown char_id");

    // §6 AddChar upserts — new key replaces old
    auto snap2 = MakeSnap(1, 100, 0x1234, "Renamed", 99);
    svc.AddChar(snap2);
    auto r5 = svc.LoadChar(1, 100, 0x1234);
    Check(r5.has_value() && r5->name == "Renamed" && r5->level == 99,
        "§6 AddChar upserts — new snapshot replaces old");
    auto r6 = svc.LoadChar(1, 100, 0xABCD);
    Check(!r6.has_value(),
        "§6 Old token rejected after upsert");

    // §7 Multiple chars coexist
    svc.AddChar(MakeSnap(2, 200, 0xBEEF, "Alice", 30));
    svc.AddChar(MakeSnap(3, 300, 0xCAFE, "Bob",   40));
    auto ra = svc.LoadChar(2, 200, 0xBEEF);
    auto rb = svc.LoadChar(3, 300, 0xCAFE);
    Check(ra.has_value() && ra->char_id == 2 && ra->name == "Alice",
        "§7 First of two additional chars loads correctly");
    Check(rb.has_value() && rb->char_id == 3 && rb->name == "Bob",
        "§7 Second of two additional chars loads correctly");

    // Cross-contamination: char 2's token doesn't find char 3
    auto rc = svc.LoadChar(3, 200, 0xBEEF);
    Check(!rc.has_value(),
        "§7 Cross-char token lookup returns nullopt");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  services/player_service spec ===\n\n");
    try
    {
        TestFakePlayerService();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
