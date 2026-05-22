// W3a-1 unit test for GuildRegistry — mirrors the CharRegistry
// suite, scaled to the guild use cases:
//
//   1. Insert / Find / Remove round-trip.
//   2. Duplicate Insert returns false; original retained.
//   3. SnapshotIds reflects the live set.
//   4. 16 threads inserting 1024 guilds each — Size() == 16384,
//      every id findable.
//   5. shared_ptr<TGuild> outlives Remove() (legacy disorg-timer
//      contract — guild lingers in m_mapTGuildEx for grace).

#include "../services/guild_registry.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <thread>
#include <vector>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

std::shared_ptr<tworldsvr::TGuild> MakeGuild(std::uint32_t id,
                                              const std::string& name = "G",
                                              std::uint32_t chief = 100)
{
    auto g = std::make_shared<tworldsvr::TGuild>();
    g->id            = id;
    g->name          = name;
    g->chief_char_id = chief;
    g->level         = 1;
    return g;
}

} // namespace

int main()
{
    using namespace tworldsvr;

    // --- Scenario 1: basic round-trip ---------------------------------
    {
        GuildRegistry reg;
        EXPECT(reg.Size() == 0);
        EXPECT(reg.Find(42) == nullptr);

        EXPECT(reg.Insert(MakeGuild(42, "Alpha")));
        EXPECT(reg.Size() == 1);

        auto found = reg.Find(42);
        EXPECT(found != nullptr);
        EXPECT(found->id == 42);
        EXPECT(found->name == "Alpha");

        auto removed = reg.Remove(42);
        EXPECT(removed != nullptr);
        EXPECT(removed->id == 42);
        EXPECT(reg.Size() == 0);
    }

    // --- Scenario 2: duplicate rejected -------------------------------
    {
        GuildRegistry reg;
        EXPECT(reg.Insert(MakeGuild(7, "First")));
        EXPECT(!reg.Insert(MakeGuild(7, "Second")));
        auto g = reg.Find(7);
        EXPECT(g != nullptr);
        EXPECT(g->name == "First");
    }

    // --- Scenario 3: snapshot consistency ----------------------------
    {
        GuildRegistry reg;
        reg.Insert(MakeGuild(1));
        reg.Insert(MakeGuild(2));
        reg.Insert(MakeGuild(3));
        EXPECT(reg.SnapshotIds().size() == 3);
        reg.Remove(2);
        auto ids = reg.SnapshotIds();
        EXPECT(ids.size() == 2);
        for (auto id : ids) EXPECT(id != 2);
    }

    // --- Scenario 4: concurrent inserts ------------------------------
    {
        GuildRegistry reg;
        constexpr int kThreads = 16;
        constexpr int kPerThread = 1024;
        std::vector<std::thread> workers;
        workers.reserve(kThreads);
        std::atomic<int> failures{0};

        for (int t = 0; t < kThreads; ++t)
        {
            workers.emplace_back([&, t]() {
                for (int i = 0; i < kPerThread; ++i)
                {
                    const std::uint32_t id =
                        static_cast<std::uint32_t>(t * kPerThread + i + 1);
                    if (!reg.Insert(MakeGuild(id)))
                        failures.fetch_add(1);
                }
            });
        }
        for (auto& w : workers) w.join();

        EXPECT(failures.load() == 0);
        EXPECT(reg.Size() == static_cast<std::size_t>(kThreads * kPerThread));

        int missing = 0;
        for (int t = 0; t < kThreads; ++t)
            for (int i = 0; i < kPerThread; ++i)
            {
                const std::uint32_t id =
                    static_cast<std::uint32_t>(t * kPerThread + i + 1);
                if (!reg.Find(id)) ++missing;
            }
        EXPECT(missing == 0);
    }

    // --- Scenario 5: shared_ptr semantics ----------------------------
    {
        GuildRegistry reg;
        reg.Insert(MakeGuild(99, "Lingering"));
        auto outside = reg.Find(99);
        EXPECT(outside.use_count() >= 2);
        reg.Remove(99);
        EXPECT(outside.use_count() == 1);
        EXPECT(outside->name == "Lingering");
    }

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_guild_registry (5 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_guild_registry (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
