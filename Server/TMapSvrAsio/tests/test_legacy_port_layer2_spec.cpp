// Spec test for layer-2 compound POD types.
//
// Unlike layer 1 these types hold `std::string` so they're NOT
// trivially copyable; copy/move semantics matter. The tests focus
// on:
//   1. Value-init zeroes scalar fields and gives empty strings.
//   2. Round-tripping through `operator=` / copy-ctor preserves
//      both numeric and string content.
//   3. Default-move leaves the source in a valid (empty-string)
//      state — the std::string contract — so a moved-from
//      PartyMember still satisfies value invariants for tests that
//      reuse the moved-from variable.

#include "legacy_port/types_layer2.h"

#include <cstdio>
#include <exception>
#include <type_traits>
#include <utility>

namespace {

int g_passed = 0;
int g_failed = 0;

void Check(bool ok, const char* label)
{
    if (ok) { ++g_passed; std::printf("  PASS  %s\n", label); }
    else    { ++g_failed; std::printf("  FAIL  %s\n", label); }
}

void TestPartyMember()
{
    std::printf("[legacy::PartyMember — value-init + copy/move]\n");
    using tmapsvr::legacy::PartyMember;

    // Strings are heap-backed → not trivially copyable. Still must
    // be default-constructible without throwing.
    Check(std::is_default_constructible_v<PartyMember>,
        "PartyMember is default-constructible");
    Check(std::is_copy_constructible_v<PartyMember>,
        "PartyMember is copy-constructible");
    Check(std::is_move_constructible_v<PartyMember>,
        "PartyMember is move-constructible");

    PartyMember p{};
    Check(p.id == 0 && p.level == 0 && p.max_hp == 0 && p.hp == 0 &&
          p.max_mp == 0 && p.mp == 0 && p.race == 0 && p.sex == 0 &&
          p.face == 0 && p.hair == 0,
        "PartyMember{} value-inits all numeric fields to zero");
    Check(p.name.empty() && p.guild.empty(),
        "PartyMember{} leaves strings empty");

    p.id     = 42;
    p.name   = "Alice";
    p.guild  = "Knights";
    p.level  = 50;
    p.max_hp = 5000;
    p.hp     = 4321;
    p.race   = 1;

    PartyMember copy = p;
    Check(copy.id == 42 && copy.name == "Alice" && copy.guild == "Knights",
        "copy-ctor preserves numeric + string fields");
    Check(copy.level == 50 && copy.max_hp == 5000 && copy.hp == 4321,
        "copy-ctor preserves hp/level fields");

    // Move-from leaves source in valid empty-string state (the
    // standard-library small-string-optimization implementations
    // may or may not actually free; what we care about is that
    // the source is still safe to re-assign or test for empty
    // without a sanitizer trip).
    PartyMember moved = std::move(p);
    Check(moved.id == 42 && moved.name == "Alice",
        "move-ctor takes ownership of numeric + string content");
    // p is now in a moved-from state — id is trivially copied so
    // it stays 42, but we don't assert that. Just re-assign and
    // verify subsequent use is safe.
    p = PartyMember{};
    Check(p.name.empty() && p.id == 0,
        "moved-from variable is safely reset by re-assignment");
}

} // namespace

int main()
{
    std::printf("=== tmapsvr_asio  legacy_port layer-2 spec ===\n\n");
    try
    {
        TestPartyMember();
    }
    catch (const std::exception& ex)
    {
        std::printf("  FAIL  unexpected exception: %s\n", ex.what());
        ++g_failed;
    }
    std::printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
