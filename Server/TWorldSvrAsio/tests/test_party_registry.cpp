// W3b-1 unit test for PartyRegistry — mirrors the GuildRegistry
// suite plus the TParty member-set helpers:
//
//   1. Insert / Find / Remove round-trip.
//   2. Duplicate Insert returns false; original retained.
//   3. SnapshotIds reflects the live set.
//   4. TParty member helpers: AddMember (dedup + order seeding),
//      RemoveMember, IsMember, IsChief, Size.
//   5. shared_ptr<TParty> outlives Remove().
//   6. 16 threads inserting parties concurrently — Size() correct,
//      every id findable.

#include "../services/party_registry.h"

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

std::shared_ptr<tworldsvr::TParty> MakeParty(std::uint16_t id,
                                             std::uint32_t chief = 100)
{
    auto p = std::make_shared<tworldsvr::TParty>();
    p->id            = id;
    p->chief_char_id = chief;
    p->obtain_type   = 0;
    return p;
}

} // namespace

int main()
{
    using namespace tworldsvr;

    // --- Scenario 1: basic round-trip ---------------------------------
    {
        PartyRegistry reg;
        EXPECT(reg.Size() == 0);
        EXPECT(reg.Find(42) == nullptr);

        EXPECT(reg.Insert(MakeParty(42, 500)));
        EXPECT(reg.Size() == 1);

        auto found = reg.Find(42);
        EXPECT(found != nullptr);
        EXPECT(found->id == 42);
        EXPECT(found->chief_char_id == 500);

        auto removed = reg.Remove(42);
        EXPECT(removed != nullptr);
        EXPECT(removed->id == 42);
        EXPECT(reg.Size() == 0);
        EXPECT(reg.Find(42) == nullptr);
    }

    // --- Scenario 2: duplicate rejected -------------------------------
    {
        PartyRegistry reg;
        EXPECT(reg.Insert(MakeParty(7, 1)));
        EXPECT(!reg.Insert(MakeParty(7, 2)));
        auto p = reg.Find(7);
        EXPECT(p != nullptr);
        EXPECT(p->chief_char_id == 1);
    }

    // --- Scenario 3: snapshot consistency ----------------------------
    {
        PartyRegistry reg;
        reg.Insert(MakeParty(1));
        reg.Insert(MakeParty(2));
        reg.Insert(MakeParty(3));
        EXPECT(reg.SnapshotIds().size() == 3);
        reg.Remove(2);
        auto ids = reg.SnapshotIds();
        EXPECT(ids.size() == 2);
        for (auto id : ids) EXPECT(id != 2);
    }

    // --- Scenario 4: TParty member helpers ---------------------------
    {
        TParty p;
        p.id            = 9;
        p.chief_char_id = 100;
        EXPECT(p.Size() == 0);
        EXPECT(!p.IsMember(100));
        EXPECT(p.IsChief(100));
        EXPECT(!p.IsChief(0));        // chief 0 never matches

        EXPECT(p.AddMember(100));     // first member seeds order
        EXPECT(p.order_char_id == 100);
        EXPECT(p.AddMember(200));
        EXPECT(p.order_char_id == 100);   // unchanged after 2nd add
        EXPECT(!p.AddMember(100));    // dedup
        EXPECT(p.Size() == 2);
        EXPECT(p.IsMember(100));
        EXPECT(p.IsMember(200));
        EXPECT(!p.IsMember(300));

        EXPECT(p.RemoveMember(100));
        EXPECT(!p.RemoveMember(100)); // already gone
        EXPECT(p.Size() == 1);
        EXPECT(!p.IsMember(100));
        EXPECT(p.IsMember(200));
        // Chief back-pointer is independent of the member list:
        // removing the chief char from members doesn't clear it
        // (succession is the W3b-3 DEL handler's job).
        EXPECT(p.IsChief(100));
    }

    // --- Scenario 5: shared_ptr semantics ----------------------------
    {
        PartyRegistry reg;
        reg.Insert(MakeParty(99, 7));
        auto outside = reg.Find(99);
        EXPECT(outside.use_count() >= 2);
        reg.Remove(99);
        EXPECT(outside.use_count() == 1);
        EXPECT(outside->chief_char_id == 7);
    }

    // --- Scenario 6: concurrent inserts ------------------------------
    {
        PartyRegistry reg;
        constexpr int kThreads = 16;
        constexpr int kPerThread = 256;   // 4096 < 65535 id space
        std::vector<std::thread> workers;
        workers.reserve(kThreads);
        std::atomic<int> failures{0};

        for (int t = 0; t < kThreads; ++t)
        {
            workers.emplace_back([&, t]() {
                for (int i = 0; i < kPerThread; ++i)
                {
                    const std::uint16_t id =
                        static_cast<std::uint16_t>(t * kPerThread + i + 1);
                    if (!reg.Insert(MakeParty(id)))
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
                const std::uint16_t id =
                    static_cast<std::uint16_t>(t * kPerThread + i + 1);
                if (!reg.Find(id)) ++missing;
            }
        EXPECT(missing == 0);
    }

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_party_registry (6 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_party_registry (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
