// W3a-4d unit test for GuildLevelCache + FakeGuildLevelRepository.
// Pure logic — no asio.
//
// Scenarios:
//   1. Empty cache: Find(any) → nullptr, Size() == 0.
//   2. LoadFrom with valid rows + out-of-range rows: only valid
//      rows land; Size() reflects the valid count; Find(level)
//      returns the populated row with the right peer_slots.
//   3. Second LoadFrom replaces; old rows disappear.
//   4. Fake repository round-trips AddRow → LoadAll verbatim.

#include "../services/fake_guild_level_repository.h"
#include "../services/guild_level_cache.h"

#include <cstdio>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

tworldsvr::TGuildLevelRow MakeRow(std::uint8_t level,
                                   std::uint32_t exp,
                                   std::uint8_t max_count,
                                   std::array<std::uint8_t, 5> peers)
{
    tworldsvr::TGuildLevelRow r;
    r.level      = level;
    r.exp        = exp;
    r.max_count  = max_count;
    r.peer_slots = peers;
    return r;
}

} // namespace

int main()
{
    using namespace tworldsvr;

    // --- 1: empty -----------------------------------------------------
    {
        GuildLevelCache cache;
        EXPECT(cache.Size() == 0);
        EXPECT(cache.Find(1) == nullptr);
        EXPECT(cache.Find(0) == nullptr);
        EXPECT(cache.Find(99) == nullptr);
    }

    // --- 2: LoadFrom filters out-of-range rows ----------------------
    {
        GuildLevelCache cache;
        cache.LoadFrom({
            MakeRow(1, 100,  20, {1, 0, 0, 0, 0}),
            MakeRow(2, 500,  30, {2, 1, 0, 0, 0}),
            MakeRow(5, 4000, 60, {5, 3, 2, 1, 1}),
            MakeRow(0, 0,    0,  {0, 0, 0, 0, 0}), // dropped
            MakeRow(99, 0,   0,  {0, 0, 0, 0, 0}), // dropped (> kMaxGuildLevel)
        });
        EXPECT(cache.Size() == 3);

        auto* r1 = cache.Find(1);
        EXPECT(r1 != nullptr);
        if (r1)
        {
            EXPECT(r1->max_count == 20);
            EXPECT(r1->peer_slots[0] == 1);
        }
        auto* r5 = cache.Find(5);
        EXPECT(r5 != nullptr);
        if (r5)
        {
            EXPECT(r5->exp == 4000);
            EXPECT(r5->peer_slots[4] == 1);
        }
        EXPECT(cache.Find(3) == nullptr);   // not seeded
        EXPECT(cache.Find(0) == nullptr);   // sentinel
    }

    // --- 3: second LoadFrom replaces --------------------------------
    {
        GuildLevelCache cache;
        cache.LoadFrom({MakeRow(1, 100, 20, {1, 0, 0, 0, 0})});
        EXPECT(cache.Find(1) != nullptr);
        cache.LoadFrom({MakeRow(2, 500, 30, {2, 1, 0, 0, 0})});
        EXPECT(cache.Find(1) == nullptr);   // old row gone
        EXPECT(cache.Find(2) != nullptr);
        EXPECT(cache.Size() == 1);
    }

    // --- 4: FakeGuildLevelRepository round-trip ---------------------
    {
        FakeGuildLevelRepository repo;
        EXPECT(repo.LoadAll().empty());
        repo.AddRow(MakeRow(1, 100, 20, {1, 0, 0, 0, 0}));
        repo.AddRow(MakeRow(2, 500, 30, {2, 1, 0, 0, 0}));
        auto rows = repo.LoadAll();
        EXPECT(rows.size() == 2);

        GuildLevelCache cache;
        cache.LoadFrom(rows);
        EXPECT(cache.Find(1)->max_count == 20);
        EXPECT(cache.Find(2)->max_count == 30);
    }

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_guild_level_cache (4 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_guild_level_cache (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
