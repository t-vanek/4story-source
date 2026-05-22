// W3a-3 unit test for the CharRegistry name index — verifies the
// secondary lookup added on top of the W2 char_id index works
// correctly and isn't tripped up by case differences, duplicate
// names, or Remove() racing the name shard.
//
// Scenarios:
//   1. Insert without name + Rename + FindByName round-trip.
//   2. FindByName is case-insensitive.
//   3. Rename clears the old name index entry.
//   4. Empty new_name in Rename keeps the char but drops it from
//      the name index (used by W3a-3 CloseChar prep paths).
//   5. Duplicate name rejected — second Rename returns false and
//      the first char stays indexed.
//   6. Remove() drops both indices.
//   7. Concurrent renames on disjoint chars all succeed.

#include "../services/char_registry.h"

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

std::shared_ptr<tworldsvr::TChar> MakeChar(std::uint32_t char_id,
                                            std::uint32_t user_id = 100,
                                            std::uint32_t key = 0xCAFEBABE)
{
    auto c = std::make_shared<tworldsvr::TChar>();
    c->char_id = char_id; c->user_id = user_id; c->key = key;
    return c;
}

} // namespace

int main()
{
    using namespace tworldsvr;

    // --- 1: Insert + Rename + FindByName -----------------------------
    {
        CharRegistry reg;
        EXPECT(reg.Insert(MakeChar(42)));
        EXPECT(reg.FindByName("Alice") == nullptr);
        EXPECT(reg.Rename(42, "Alice"));

        auto found = reg.FindByName("Alice");
        EXPECT(found != nullptr);
        EXPECT(found && found->char_id == 42);

        // The stored name retains the original casing.
        if (found) {
            std::lock_guard g(found->lock);
            EXPECT(found->name == "Alice");
        }
    }

    // --- 2: case-insensitive lookup ----------------------------------
    {
        CharRegistry reg;
        reg.Insert(MakeChar(7));
        reg.Rename(7, "Bob");
        EXPECT(reg.FindByName("BOB") != nullptr);
        EXPECT(reg.FindByName("bob") != nullptr);
        EXPECT(reg.FindByName("BoB") != nullptr);
    }

    // --- 3: Rename clears the old index entry ------------------------
    {
        CharRegistry reg;
        reg.Insert(MakeChar(1));
        reg.Rename(1, "OldName");
        EXPECT(reg.FindByName("OldName") != nullptr);
        EXPECT(reg.Rename(1, "NewName"));
        EXPECT(reg.FindByName("OldName") == nullptr);
        EXPECT(reg.FindByName("NewName") != nullptr);
    }

    // --- 4: empty new_name only drops the index ----------------------
    {
        CharRegistry reg;
        reg.Insert(MakeChar(2));
        reg.Rename(2, "Dropped");
        EXPECT(reg.FindByName("Dropped") != nullptr);
        EXPECT(reg.Rename(2, "")); // drop-only
        EXPECT(reg.FindByName("Dropped") == nullptr);
        // Char itself is still in the char_id index.
        EXPECT(reg.Find(2) != nullptr);
    }

    // --- 5: duplicate name refused -----------------------------------
    {
        CharRegistry reg;
        reg.Insert(MakeChar(10));
        reg.Insert(MakeChar(11));
        EXPECT(reg.Rename(10, "Shared"));
        EXPECT(!reg.Rename(11, "Shared"));      // collision
        EXPECT(!reg.Rename(11, "SHARED"));      // case-insensitive
        auto winner = reg.FindByName("Shared");
        EXPECT(winner && winner->char_id == 10);
    }

    // --- 6: Remove() drops both indices -----------------------------
    {
        CharRegistry reg;
        reg.Insert(MakeChar(99));
        reg.Rename(99, "Charlie");
        EXPECT(reg.FindByName("Charlie") != nullptr);
        auto removed = reg.Remove(99);
        EXPECT(removed != nullptr);
        EXPECT(reg.Find(99) == nullptr);
        EXPECT(reg.FindByName("Charlie") == nullptr);
    }

    // --- 7: concurrent renames on disjoint chars --------------------
    {
        CharRegistry reg;
        constexpr int kThreads = 16;
        constexpr int kPerThread = 256;
        for (int t = 0; t < kThreads; ++t)
            for (int i = 0; i < kPerThread; ++i)
                reg.Insert(MakeChar(static_cast<std::uint32_t>(
                    t * kPerThread + i + 1)));

        std::atomic<int> failures{0};
        std::vector<std::thread> workers;
        workers.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t)
        {
            workers.emplace_back([&, t]() {
                for (int i = 0; i < kPerThread; ++i)
                {
                    const auto id = static_cast<std::uint32_t>(
                        t * kPerThread + i + 1);
                    const std::string name = "C" + std::to_string(id);
                    if (!reg.Rename(id, name))
                        failures.fetch_add(1);
                }
            });
        }
        for (auto& w : workers) w.join();

        EXPECT(failures.load() == 0);

        int missing = 0;
        for (int t = 0; t < kThreads; ++t)
            for (int i = 0; i < kPerThread; ++i)
            {
                const auto id = static_cast<std::uint32_t>(
                    t * kPerThread + i + 1);
                if (!reg.FindByName("C" + std::to_string(id))) ++missing;
            }
        EXPECT(missing == 0);
    }

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_char_name_index (7 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_char_name_index (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
