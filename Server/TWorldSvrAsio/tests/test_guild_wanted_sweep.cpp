// W3a-19 unit test: GuildWantedRegistry::PruneExpired +
// SweepExpiredWanted coroutine integration.
//
// Pure-logic tests, no socket / dispatch. Three scenarios:
//   1. PruneExpired only removes entries with end_time < now;
//      kept entries stay intact + applicant reverse index for
//      pruned entries is cleaned up.
//   2. SweepExpiredWanted persists the deletions via the repo
//      (one DeleteWanted call per pruned guild_id).
//   3. SweepExpiredWanted with a null repo still does the
//      in-memory prune (defensive — production should always
//      supply a repo, but the helper is defined as
//      nullptr-tolerant).

#include "../services/fake_guild_repository.h"
#include "../services/guild_constants.h"
#include "../services/guild_wanted_registry.h"
#include "../services/guild_wanted_sweep.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <cstdint>
#include <cstdio>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

tworldsvr::TGuildWanted MakeWanted(std::uint32_t gid,
                                    std::int64_t end_time,
                                    std::uint8_t country = 0)
{
    tworldsvr::TGuildWanted w;
    w.guild_id  = gid;
    w.country   = country;
    w.min_level = 1;
    w.max_level = 99;
    w.end_time  = end_time;
    w.name      = "g" + std::to_string(gid);
    w.title     = "title";
    w.text      = "text";
    return w;
}

void Test_PruneExpired_RemovesOnlyExpired()
{
    tworldsvr::GuildWantedRegistry reg;
    reg.AddOrUpdate(MakeWanted(1, /*end_time=*/100));
    reg.AddOrUpdate(MakeWanted(2, /*end_time=*/200));
    reg.AddOrUpdate(MakeWanted(3, /*end_time=*/300));

    // Prune at now=200 → guild 1 expired, 2 + 3 kept.
    const auto removed = reg.PruneExpired(/*now=*/200);
    EXPECT(removed.size() == 1);
    EXPECT(removed[0] == 1);

    EXPECT(!reg.Find(1).has_value());
    EXPECT(reg.Find(2).has_value());
    EXPECT(reg.Find(3).has_value());
}

void Test_PruneExpired_DropsApplicantReverseIndex()
{
    tworldsvr::GuildWantedRegistry reg;
    // Use end_time = far future for the "kept" entry so AddApp's
    // wall-clock expiry gate (std::time(nullptr) vs end_time)
    // doesn't reject the applicant. Sweep is driven by an
    // explicit now arg so it doesn't need the wall clock.
    constexpr std::int64_t kFarFuture = 1LL << 40;
    reg.AddOrUpdate(MakeWanted(1, /*end_time=*/kFarFuture));
    reg.AddOrUpdate(MakeWanted(2, /*end_time=*/kFarFuture));

    // Char 42 applies to wanted 1, char 43 to wanted 2.
    tworldsvr::TGuildWantedApp app1{};
    app1.char_id   = 42;
    app1.wanted_id = 1;
    app1.level     = 10;
    app1.name      = "ApplicantA";
    EXPECT(reg.AddApp(app1, /*country=*/0) ==
        tworldsvr::guild::kSuccess);
    EXPECT(reg.FindAppByChar(42) == 1);

    tworldsvr::TGuildWantedApp app2{};
    app2.char_id   = 43;
    app2.wanted_id = 2;
    app2.level     = 11;
    app2.name      = "ApplicantB";
    EXPECT(reg.AddApp(app2, /*country=*/0) ==
        tworldsvr::guild::kSuccess);

    // Force wanted 1 expired by upserting it with a past end_time
    // (AddOrUpdate is destructive — applicants stay since they
    // live in the registry's per-entry vector; we'd need a fresh
    // entry to drop them, but the W3a-19 sweep is the surgical
    // path we're actually testing). The upsert preserves the
    // already-pushed applicants list inside the parent entry…
    // actually, no — AddOrUpdate just replaces the entry with the
    // new TGuildWanted struct, dropping applicants. So instead
    // we directly prune at "now = far past + 1" — only entries
    // strictly past their end_time get culled. Since both have
    // kFarFuture, neither expires.
    //
    // To exercise the prune→reverse-index path, we instead
    // re-AddOrUpdate wanted 1 with a past end_time AFTER moving
    // its applicants to a fresh storage spot. Simpler: build a
    // second registry seeded the way we want.
    tworldsvr::GuildWantedRegistry reg2;
    reg2.AddOrUpdate(MakeWanted(1, /*end_time=*/kFarFuture));
    reg2.AddOrUpdate(MakeWanted(2, /*end_time=*/kFarFuture));
    EXPECT(reg2.AddApp(app1, 0) == tworldsvr::guild::kSuccess);
    EXPECT(reg2.AddApp(app2, 0) == tworldsvr::guild::kSuccess);

    // Prune at now=kFarFuture+1 → both expire, both applicants'
    // reverse-index pointers cleared.
    const auto removed = reg2.PruneExpired(kFarFuture + 1);
    EXPECT(removed.size() == 2);
    EXPECT(reg2.FindAppByChar(42) == 0);
    EXPECT(reg2.FindAppByChar(43) == 0);
}

void Test_SweepExpiredWanted_PersistsDeletions()
{
    tworldsvr::GuildWantedRegistry reg;
    tworldsvr::FakeGuildRepository repo;

    // Seed two expired entries. Pick end_time well below any
    // realistic std::time(nullptr) sample so the sweep prunes
    // them deterministically.
    reg.AddOrUpdate(MakeWanted(7, /*end_time=*/100));
    reg.AddOrUpdate(MakeWanted(8, /*end_time=*/100));

    boost::asio::io_context io;
    auto fut = boost::asio::co_spawn(io,
        tworldsvr::SweepExpiredWanted(reg, &repo, /*db_pool=*/nullptr),
        boost::asio::use_future);
    io.run();
    fut.get();   // propagate any exception

    // Both entries gone from the registry.
    EXPECT(!reg.Find(7).has_value());
    EXPECT(!reg.Find(8).has_value());

    // Repo recorded one DeleteWanted per pruned id.
    std::size_t del_7 = 0, del_8 = 0;
    for (const auto& c : repo.Calls())
    {
        if (c.kind == tworldsvr::FakeGuildRepository::Call::Kind
                        ::kDeleteWanted)
        {
            if (c.guild_id == 7) ++del_7;
            if (c.guild_id == 8) ++del_8;
        }
    }
    EXPECT(del_7 == 1);
    EXPECT(del_8 == 1);
}

void Test_SweepExpiredWanted_NullRepoStillPrunes()
{
    tworldsvr::GuildWantedRegistry reg;
    reg.AddOrUpdate(MakeWanted(9, /*end_time=*/100));

    boost::asio::io_context io;
    auto fut = boost::asio::co_spawn(io,
        tworldsvr::SweepExpiredWanted(reg, /*repo=*/nullptr,
            /*db_pool=*/nullptr),
        boost::asio::use_future);
    io.run();
    fut.get();

    EXPECT(!reg.Find(9).has_value());
}

void Test_SweepExpiredWanted_NoExpiredIsNoOp()
{
    tworldsvr::GuildWantedRegistry reg;
    tworldsvr::FakeGuildRepository repo;
    // End-time far in the future — sweep finds nothing.
    reg.AddOrUpdate(MakeWanted(10, /*end_time=*/(1LL << 40)));

    boost::asio::io_context io;
    auto fut = boost::asio::co_spawn(io,
        tworldsvr::SweepExpiredWanted(reg, &repo, /*db_pool=*/nullptr),
        boost::asio::use_future);
    io.run();
    fut.get();

    EXPECT(reg.Find(10).has_value());
    // No DeleteWanted calls.
    for (const auto& c : repo.Calls())
    {
        EXPECT(c.kind !=
            tworldsvr::FakeGuildRepository::Call::Kind::kDeleteWanted);
    }
}

} // namespace

int main()
{
    Test_PruneExpired_RemovesOnlyExpired();
    Test_PruneExpired_DropsApplicantReverseIndex();
    Test_SweepExpiredWanted_PersistsDeletions();
    Test_SweepExpiredWanted_NullRepoStillPrunes();
    Test_SweepExpiredWanted_NoExpiredIsNoOp();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_guild_wanted_sweep "
                    "(5 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_guild_wanted_sweep "
                    "(%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
