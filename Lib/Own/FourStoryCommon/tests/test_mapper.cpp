// Standalone compile + smoke test for the mapper framework.
// Run: builds as `test_fourstory_mapper`.

#include "fourstory/mapper/mapper.h"

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

namespace test {

// ── Demo entities ────────────────────────────────────────────────────

// A "legacy DB row" struct with fixed-buffer name + int IDs.
struct LegacyCharRow
{
    int       dwCharID  = 0;
    int       dwUserID  = 0;
    char      szName[50]{};
    int       bLevel    = 0;
    int       dwExp     = 0;
    int       bCountry  = 0;
};

// Modern domain snapshot — std::string, uint8/uint32 typed fields.
struct CharSnapshot
{
    std::uint32_t  dwCharID = 0;
    std::uint32_t  dwUserID = 0;
    std::string    szName;
    std::uint8_t   bLevel   = 1;
    std::uint32_t  dwEXP    = 0;
    std::uint8_t   bCountry = 0;
};

// "Lobby view" — narrower projection for the CS_CHARLIST_ACK packet.
struct CharListEntry
{
    std::uint32_t id     = 0;
    std::string   name;
    std::uint8_t  level  = 1;
};

// Profile that wires both mappings together.
class CharProfile : public fourstory::mapper::MapperProfile
{
public:
    const char* Name() const override { return "CharProfile"; }

    void Configure() override
    {
        using namespace fourstory::mapper;

        // Legacy row → modern snapshot.
        MapperConfig<LegacyCharRow, CharSnapshot>()
            .Set(&CharSnapshot::dwCharID, &LegacyCharRow::dwCharID)
            .Set(&CharSnapshot::dwUserID, &LegacyCharRow::dwUserID)
            .Set(&CharSnapshot::szName,   &LegacyCharRow::szName)
            .Set(&CharSnapshot::bLevel,   &LegacyCharRow::bLevel)
            .Set(&CharSnapshot::dwEXP,    &LegacyCharRow::dwExp)
            .Set(&CharSnapshot::bCountry, &LegacyCharRow::bCountry);

        // Snapshot → lobby view (narrower projection).
        MapperConfig<CharSnapshot, CharListEntry>()
            .Set(&CharListEntry::id,    &CharSnapshot::dwCharID)
            .Set(&CharListEntry::name,  &CharSnapshot::szName)
            .Set(&CharListEntry::level, &CharSnapshot::bLevel);
    }
};

} // namespace test

int main()
{
    using namespace fourstory::mapper;
    using namespace test;

    MapperRegistry::Get().Register<CharProfile>();
    MapperRegistry::Get().ApplyAll();

    // ── Test 1: legacy char[50] → std::string + int → uint8 narrow ──
    LegacyCharRow row{};
    row.dwCharID = 12345;
    row.dwUserID = 9876;
    std::snprintf(row.szName, sizeof(row.szName), "%s", "Hero");
    row.bLevel   = 42;
    row.dwExp    = 100000;
    row.bCountry = 3;

    auto snap = Adapt<CharSnapshot>(row);
    assert(snap.dwCharID == 12345);
    assert(snap.dwUserID == 9876);
    assert(snap.szName   == "Hero");
    assert(snap.bLevel   == 42);
    assert(snap.dwEXP    == 100000);
    assert(snap.bCountry == 3);

    // ── Test 2: snapshot → lobby view (narrower projection) ────────
    auto lobby = Adapt<CharListEntry>(snap);
    assert(lobby.id    == 12345);
    assert(lobby.name  == "Hero");
    assert(lobby.level == 42);

    // ── Test 3: bulk vector mapping ─────────────────────────────────
    std::vector<LegacyCharRow> rows;
    for (int i = 0; i < 5; ++i)
    {
        LegacyCharRow r{};
        r.dwCharID = 1000 + i;
        std::snprintf(r.szName, sizeof(r.szName), "Char%d", i);
        r.bLevel = static_cast<std::uint8_t>(10 + i);
        rows.push_back(r);
    }
    auto snaps = AdaptAll<CharSnapshot>(rows);
    assert(snaps.size() == 5);
    assert(snaps[2].dwCharID == 1002);
    assert(snaps[2].szName   == "Char2");
    assert(snaps[2].bLevel   == 12);

    // ── Test 4: AdaptTo() — populate existing instance ─────────────
    CharSnapshot existing{};
    existing.dwCharID = 99;
    AdaptTo(row, existing);
    assert(existing.dwCharID == 12345);

    // ── Test 5: per-instance TypeMap with lambda + Default + AfterMap
    TypeMap<LegacyCharRow, CharListEntry> direct;
    direct
        .Set(&CharListEntry::id, &LegacyCharRow::dwCharID)
        .Set(&CharListEntry::name,
             [](const LegacyCharRow& r) { return std::string("Lord ") + r.szName; })
        .Default(&CharListEntry::level, std::uint8_t{99})
        .AfterMap([](const LegacyCharRow& /*r*/, CharListEntry& d)
        {
            d.name += "!";
        });

    auto le = direct.Map(row);
    assert(le.id    == 12345);
    assert(le.name  == "Lord Hero!");
    assert(le.level == 99);

    std::printf("test_mapper: all assertions passed "
                "(%zu profiles registered, %zu mappings on direct TypeMap)\n",
        MapperRegistry::Get().Count(),
        direct.RuleCount());
    return 0;
}
