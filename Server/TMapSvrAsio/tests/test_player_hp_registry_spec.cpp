// Spec test for LocalPlayerHpRegistry.
//
// §1  Register + Get + Size
// §2  ApplyHpDelta — damage, heal, clamp [0, max_hp]
// §3  Unregister removes entry
// §4  Unknown char_id → safe no-op

#include "player_hp_registry.h"

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

void TestRegisterGet()
{
    std::printf("[§1 Register + Get + Size]\n");
    tmapsvr::LocalPlayerHpRegistry r;

    Check(r.Size() == 0, "empty size = 0");
    Check(r.Get(1) == nullptr, "Get on empty = nullptr");

    r.Register(1, 5000, 5000, 2000, 2000);
    r.Register(2, 3000, 3000, 1000, 1000);

    Check(r.Size() == 2, "size = 2 after two registers");
    const auto* v = r.Get(1);
    Check(v != nullptr, "Get(1) not null");
    if (v)
    {
        Check(v->hp == 5000 && v->max_hp == 5000, "hp/max_hp correct");
        Check(v->mp == 2000 && v->max_mp == 2000, "mp/max_mp correct");
        Check(v->IsAlive(), "IsAlive() true");
    }
}

void TestApplyHpDelta()
{
    std::printf("[§2 ApplyHpDelta — damage, heal, clamp]\n");
    tmapsvr::LocalPlayerHpRegistry r;
    r.Register(10, 10000, 10000, 500, 500);

    auto hp = r.ApplyHpDelta(10, -1000);
    Check(hp == 9000u, "-1000 → hp=9000");

    hp = r.ApplyHpDelta(10, +500);
    Check(hp == 9500u, "+500 → hp=9500");

    hp = r.ApplyHpDelta(10, -99999);
    Check(hp == 0u, "overkill clamped to 0");
    Check(r.Get(10)->IsDead(), "IsDead() after overkill");

    hp = r.ApplyHpDelta(10, +99999);
    Check(hp == 10000u, "overheal clamped to max_hp");
    Check(r.Get(10)->IsAlive(), "IsAlive() after full heal");
}

void TestUnregister()
{
    std::printf("[§3 Unregister removes entry]\n");
    tmapsvr::LocalPlayerHpRegistry r;
    r.Register(5, 1000, 1000, 100, 100);
    Check(r.Get(5) != nullptr, "registered");
    r.Unregister(5);
    Check(r.Get(5) == nullptr, "unregistered");
    Check(r.Size() == 0, "size = 0 after unregister");
}

void TestUnknownSafe()
{
    std::printf("[§4 Unknown char_id → safe no-op]\n");
    tmapsvr::LocalPlayerHpRegistry r;
    auto hp = r.ApplyHpDelta(999, -100);
    Check(hp == 0u, "ApplyHpDelta on unknown = 0");
    r.Unregister(999);  // no crash
    Check(true, "Unregister on unknown = no crash");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  LocalPlayerHpRegistry spec ===\n\n");
    try
    {
        TestRegisterGet();
        TestApplyHpDelta();
        TestUnregister();
        TestUnknownSafe();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
