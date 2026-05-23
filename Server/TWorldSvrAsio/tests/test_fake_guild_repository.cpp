// W3a-1 unit test for FakeGuildRepository — verifies the seed API
// + LoadAll + FindById contract, and the deep-copy isolation that
// tests rely on (mutating a returned guild does not bleed back
// into the seed).

#include "../services/fake_guild_repository.h"

#include <cstdio>
#include <memory>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

std::shared_ptr<tworldsvr::TGuild> MakeGuild(std::uint32_t id,
                                              const std::string& name,
                                              std::uint32_t fame)
{
    auto g = std::make_shared<tworldsvr::TGuild>();
    g->id     = id;
    g->name   = name;
    g->fame   = fame;
    g->level  = 5;
    return g;
}

} // namespace

int main()
{
    using namespace tworldsvr;

    FakeGuildRepository repo;

    // Empty repo round-trips cleanly.
    EXPECT(repo.LoadAll().empty());
    EXPECT(!repo.FindById(42).has_value());

    repo.AddGuild(MakeGuild(1, "Alpha", 100));
    repo.AddGuild(MakeGuild(2, "Beta",  250));

    auto all = repo.LoadAll();
    EXPECT(all.size() == 2);

    auto found = repo.FindById(1);
    EXPECT(found.has_value());
    if (found)
    {
        EXPECT((*found)->name == "Alpha");
        EXPECT((*found)->fame == 100);

        // Mutating the returned shared_ptr's TGuild must NOT
        // change the seed (deep-copy contract).
        (*found)->fame = 9999;
    }
    auto found2 = repo.FindById(1);
    EXPECT(found2.has_value());
    if (found2) EXPECT((*found2)->fame == 100);

    EXPECT(!repo.FindById(999).has_value());

    // W3a-30: Clone must deep-copy the W3a-25/27 fields too —
    // previously alliance_ids / enemy_ids / point_log /
    // pvp_month_point were silently dropped on LoadAll/FindById.
    {
        auto g = MakeGuild(3, "Gamma", 500);
        g->pvp_month_point = 7777;
        g->alliance_ids = {11, 22, 33};
        g->enemy_ids    = {44};
        TPointRewardEntry e;
        e.date_unix      = 1700000000;
        e.recipient_name = "Rewardee";
        e.point          = 250;
        g->point_log.push_back(e);
        repo.AddGuild(std::move(g));

        auto got = repo.FindById(3);
        EXPECT(got.has_value());
        if (got)
        {
            EXPECT((*got)->pvp_month_point == 7777);
            EXPECT((*got)->alliance_ids.size() == 3);
            if ((*got)->alliance_ids.size() == 3)
            {
                EXPECT((*got)->alliance_ids[0] == 11);
                EXPECT((*got)->alliance_ids[2] == 33);
            }
            EXPECT((*got)->enemy_ids.size() == 1);
            if (!(*got)->enemy_ids.empty())
                EXPECT((*got)->enemy_ids[0] == 44);
            EXPECT((*got)->point_log.size() == 1);
            if (!(*got)->point_log.empty())
            {
                EXPECT((*got)->point_log[0].recipient_name == "Rewardee");
                EXPECT((*got)->point_log[0].point == 250);
                EXPECT((*got)->point_log[0].date_unix == 1700000000);
            }
        }
    }

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_fake_guild_repo\n");
    else
        std::printf("FAIL test_tworldsvr_asio_fake_guild_repo (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
