// W3c-1 unit test for CorpsRegistry — mirrors the PartyRegistry
// suite + the TCorps squad-set helpers.
//
//   1. Insert / Find / Remove round-trip.
//   2. Duplicate Insert rejected; original retained.
//   3. SnapshotIds reflects the live set.
//   4. TCorps squad helpers: AddParty (dedup) / RemoveParty /
//      IsParty / Size.
//   5. shared_ptr<TCorps> outlives Remove().
//   6. 16 threads inserting concurrently — Size + findability.

#include "../services/corps_registry.h"

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

std::shared_ptr<tworldsvr::TCorps> MakeCorps(std::uint16_t id,
                                             std::uint16_t commander = 1)
{
    auto c = std::make_shared<tworldsvr::TCorps>();
    c->id                 = id;
    c->commander_party_id = commander;
    return c;
}

} // namespace

int main()
{
    using namespace tworldsvr;

    // --- Scenario 1: basic round-trip ---------------------------------
    {
        CorpsRegistry reg;
        EXPECT(reg.Size() == 0);
        EXPECT(reg.Find(7) == nullptr);
        EXPECT(reg.Insert(MakeCorps(7, 3)));
        EXPECT(reg.Size() == 1);
        auto c = reg.Find(7);
        EXPECT(c != nullptr);
        EXPECT(c->id == 7);
        EXPECT(c->commander_party_id == 3);
        auto removed = reg.Remove(7);
        EXPECT(removed != nullptr);
        EXPECT(reg.Size() == 0);
        EXPECT(reg.Find(7) == nullptr);
    }

    // --- Scenario 2: duplicate rejected -------------------------------
    {
        CorpsRegistry reg;
        EXPECT(reg.Insert(MakeCorps(5, 1)));
        EXPECT(!reg.Insert(MakeCorps(5, 2)));
        auto c = reg.Find(5);
        EXPECT(c && c->commander_party_id == 1);
    }

    // --- Scenario 3: snapshot consistency ----------------------------
    {
        CorpsRegistry reg;
        reg.Insert(MakeCorps(1));
        reg.Insert(MakeCorps(2));
        reg.Insert(MakeCorps(3));
        EXPECT(reg.SnapshotIds().size() == 3);
        reg.Remove(2);
        auto ids = reg.SnapshotIds();
        EXPECT(ids.size() == 2);
        for (auto id : ids) EXPECT(id != 2);
    }

    // --- Scenario 4: TCorps squad helpers ----------------------------
    {
        TCorps c;
        c.id = 9;
        EXPECT(c.Size() == 0);
        EXPECT(!c.IsParty(10));
        EXPECT(c.AddParty(10));
        EXPECT(c.AddParty(20));
        EXPECT(!c.AddParty(10));      // dedup
        EXPECT(c.Size() == 2);
        EXPECT(c.IsParty(10));
        EXPECT(c.IsParty(20));
        EXPECT(!c.IsParty(30));
        EXPECT(c.RemoveParty(10));
        EXPECT(!c.RemoveParty(10));
        EXPECT(c.Size() == 1);
        EXPECT(c.IsParty(20));
    }

    // --- Scenario 5: shared_ptr semantics ----------------------------
    {
        CorpsRegistry reg;
        reg.Insert(MakeCorps(99, 4));
        auto outside = reg.Find(99);
        EXPECT(outside.use_count() >= 2);
        reg.Remove(99);
        EXPECT(outside.use_count() == 1);
        EXPECT(outside->commander_party_id == 4);
    }

    // --- Scenario 6: concurrent inserts ------------------------------
    {
        CorpsRegistry reg;
        constexpr int kThreads = 16;
        constexpr int kPerThread = 256;
        std::vector<std::thread> workers;
        workers.reserve(kThreads);
        std::atomic<int> failures{0};
        for (int t = 0; t < kThreads; ++t)
            workers.emplace_back([&, t]() {
                for (int i = 0; i < kPerThread; ++i)
                {
                    const std::uint16_t id =
                        static_cast<std::uint16_t>(t * kPerThread + i + 1);
                    if (!reg.Insert(MakeCorps(id))) failures.fetch_add(1);
                }
            });
        for (auto& w : workers) w.join();
        EXPECT(failures.load() == 0);
        EXPECT(reg.Size() == static_cast<std::size_t>(kThreads * kPerThread));
        int missing = 0;
        for (int t = 0; t < kThreads; ++t)
            for (int i = 0; i < kPerThread; ++i)
            {
                const std::uint16_t id =
                    static_cast<std::uint16_t>(t * kPerThread + i + 1);
                if (!reg.Find(id)) ++missing;
            }
        EXPECT(missing == 0);
    }

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_corps_registry (6 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_corps_registry (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
