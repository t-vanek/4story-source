// W2 unit test for CharRegistry — verifies the partitioned hash
// map + active-user index do what their contract says under both
// single-threaded use and concurrent inserts.
//
// Scenarios:
//   1. Insert / Find / Remove round-trip.
//   2. Duplicate Insert returns false; original entry is retained.
//   3. Size() and SnapshotIds() reflect the running set.
//   4. Active-user counter tracks Mark/Unmark correctly.
//   5. 16 threads inserting 1024 chars each — verify Size() ==
//      16384 and every char_id is findable. Exercises the shard
//      locking under real contention.
//   6. After Remove, the TChar lives until the last external
//      reference dies (shared_ptr ownership contract).

#include "../services/char_registry.h"

#include <atomic>
#include <chrono>
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

std::shared_ptr<tworldsvr::TChar> MakeChar(std::uint32_t char_id,
                                            std::uint32_t user_id,
                                            std::uint32_t key = 0xDEADBEEF)
{
    auto c = std::make_shared<tworldsvr::TChar>();
    c->char_id        = char_id;
    c->user_id        = user_id;
    c->key            = key;
    c->main_server_id = 1;
    return c;
}

} // namespace

int main()
{
    using namespace tworldsvr;

    // --- Scenario 1: basic round-trip ---------------------------------
    {
        CharRegistry reg;
        EXPECT(reg.Size() == 0);
        EXPECT(reg.Find(42) == nullptr);

        EXPECT(reg.Insert(MakeChar(42, 100)));
        EXPECT(reg.Size() == 1);

        auto found = reg.Find(42);
        EXPECT(found != nullptr);
        EXPECT(found->char_id == 42);
        EXPECT(found->user_id == 100);

        auto removed = reg.Remove(42);
        EXPECT(removed != nullptr);
        EXPECT(removed->char_id == 42);
        EXPECT(reg.Size() == 0);
        EXPECT(reg.Find(42) == nullptr);

        // Idempotent remove returns nullptr.
        EXPECT(reg.Remove(42) == nullptr);
    }

    // --- Scenario 2: duplicate Insert is rejected ---------------------
    {
        CharRegistry reg;
        EXPECT(reg.Insert(MakeChar(7, 200, 0x11111111)));
        // Second insert with the same char_id but a different key
        // must NOT overwrite — the registry treats duplicates as
        // "additional connection" and leaves the existing entry.
        EXPECT(!reg.Insert(MakeChar(7, 200, 0x22222222)));
        auto existing = reg.Find(7);
        EXPECT(existing != nullptr);
        EXPECT(existing->key == 0x11111111);
    }

    // --- Scenario 3: Snapshot reflects live set -----------------------
    {
        CharRegistry reg;
        reg.Insert(MakeChar(1, 100));
        reg.Insert(MakeChar(2, 100));
        reg.Insert(MakeChar(3, 200));
        auto ids = reg.SnapshotIds();
        EXPECT(ids.size() == 3);
        reg.Remove(2);
        ids = reg.SnapshotIds();
        EXPECT(ids.size() == 2);
        // After remove, no id equals 2.
        for (auto id : ids) EXPECT(id != 2);
    }

    // --- Scenario 4: active-user index --------------------------------
    {
        CharRegistry reg;
        EXPECT(reg.ActiveUserCount() == 0);
        EXPECT(!reg.IsUserActive(500));

        reg.MarkUserActive(500);
        reg.MarkUserActive(501);
        reg.MarkUserActive(500); // idempotent
        EXPECT(reg.ActiveUserCount() == 2);
        EXPECT(reg.IsUserActive(500));
        EXPECT(reg.IsUserActive(501));

        reg.MarkUserInactive(500);
        EXPECT(reg.ActiveUserCount() == 1);
        EXPECT(!reg.IsUserActive(500));
        EXPECT(reg.IsUserActive(501));
    }

    // --- Scenario 5: concurrent inserts -------------------------------
    {
        CharRegistry reg;
        constexpr int kThreads = 16;
        constexpr int kPerThread = 1024;
        std::vector<std::thread> workers;
        workers.reserve(kThreads);
        std::atomic<int> insert_failures{0};

        for (int t = 0; t < kThreads; ++t)
        {
            workers.emplace_back([&, t]() {
                for (int i = 0; i < kPerThread; ++i)
                {
                    const std::uint32_t char_id =
                        static_cast<std::uint32_t>(t * kPerThread + i + 1);
                    if (!reg.Insert(MakeChar(char_id, char_id))) // unique
                        insert_failures.fetch_add(1);
                }
            });
        }
        for (auto& w : workers) w.join();

        EXPECT(insert_failures.load() == 0);
        EXPECT(reg.Size() == static_cast<std::size_t>(kThreads * kPerThread));

        // Every char_id is findable from a single reader thread.
        int missing = 0;
        for (int t = 0; t < kThreads; ++t)
            for (int i = 0; i < kPerThread; ++i)
            {
                const std::uint32_t char_id =
                    static_cast<std::uint32_t>(t * kPerThread + i + 1);
                if (!reg.Find(char_id)) ++missing;
            }
        EXPECT(missing == 0);
    }

    // --- Scenario 6: shared_ptr semantics ------------------------------
    {
        CharRegistry reg;
        reg.Insert(MakeChar(99, 99));
        auto outside = reg.Find(99);
        EXPECT(outside.use_count() >= 2); // registry + outside
        reg.Remove(99);
        EXPECT(outside.use_count() == 1); // only outside
        EXPECT(outside->char_id == 99);   // still usable after Remove
    }

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_char_registry (6 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_char_registry (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
