// W3a-36 unit test: SweepExpiredTactics.
//
// Scenarios:
//   1. No tactics members → no-op.
//   2. Mixed expired / live contracts → only expired removed,
//      freed chars' tactics_guild_id cleared, live ones kept.
//   3. end_time == 0 is treated as "no expiry" (never pruned).
//   4. Null CharRegistry → still prunes the roster (back-pointer
//      clear skipped).

#include "../services/char_registry.h"
#include "../services/guild_registry.h"
#include "../services/guild_tactics_sweep.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <cstdio>
#include <ctime>
#include <memory>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

std::shared_ptr<tworldsvr::TGuild> MakeGuild(std::uint32_t id)
{
    auto g = std::make_shared<tworldsvr::TGuild>();
    g->id = id;
    return g;
}

tworldsvr::TTacticsMember MakeMember(std::uint32_t id,
                                      std::int64_t end_time)
{
    tworldsvr::TTacticsMember m;
    m.id       = id;
    m.end_time = end_time;
    return m;
}

void RunSweep(tworldsvr::GuildRegistry& g, tworldsvr::CharRegistry* c)
{
    boost::asio::io_context io;
    auto fut = boost::asio::co_spawn(io,
        tworldsvr::SweepExpiredTactics(g, c), boost::asio::use_future);
    io.run();
    fut.get();
}

void Test_Empty()
{
    tworldsvr::GuildRegistry guilds;
    guilds.Insert(MakeGuild(1));
    tworldsvr::CharRegistry chars;
    RunSweep(guilds, &chars);   // no members → no crash
    if (auto g = guilds.Find(1))
    {
        std::lock_guard gl(g->lock);
        EXPECT(g->tactics_members.empty());
    }
}

void Test_MixedExpiry()
{
    const std::int64_t now =
        static_cast<std::int64_t>(std::time(nullptr));

    tworldsvr::GuildRegistry guilds;
    auto g = MakeGuild(8);
    g->tactics_members.push_back(MakeMember(700, now - 100)); // expired
    g->tactics_members.push_back(MakeMember(701, now + 1000)); // live
    guilds.Insert(g);

    tworldsvr::CharRegistry chars;
    // Seed char 700 + 701 with tactics_guild_id = 8.
    {
        auto c700 = std::make_shared<tworldsvr::TChar>();
        c700->char_id = 700; c700->key = 1; c700->tactics_guild_id = 8;
        chars.Insert(c700);
        auto c701 = std::make_shared<tworldsvr::TChar>();
        c701->char_id = 701; c701->key = 2; c701->tactics_guild_id = 8;
        chars.Insert(c701);
    }

    RunSweep(guilds, &chars);

    if (auto gg = guilds.Find(8))
    {
        std::lock_guard gl(gg->lock);
        EXPECT(gg->tactics_members.size() == 1);
        EXPECT(gg->FindTactics(700) == nullptr);  // expired removed
        EXPECT(gg->FindTactics(701) != nullptr);  // live kept
    }
    // char 700's back-pointer cleared, 701's kept.
    if (auto c = chars.Find(700))
    {
        std::lock_guard cg(c->lock);
        EXPECT(c->tactics_guild_id == 0);
    }
    if (auto c = chars.Find(701))
    {
        std::lock_guard cg(c->lock);
        EXPECT(c->tactics_guild_id == 8);
    }
}

void Test_ZeroEndTimeNeverExpires()
{
    tworldsvr::GuildRegistry guilds;
    auto g = MakeGuild(9);
    g->tactics_members.push_back(MakeMember(800, 0));  // 0 = no expiry
    guilds.Insert(g);
    tworldsvr::CharRegistry chars;
    RunSweep(guilds, &chars);
    if (auto gg = guilds.Find(9))
    {
        std::lock_guard gl(gg->lock);
        EXPECT(gg->tactics_members.size() == 1);
    }
}

void Test_NullCharRegistry()
{
    const std::int64_t now =
        static_cast<std::int64_t>(std::time(nullptr));
    tworldsvr::GuildRegistry guilds;
    auto g = MakeGuild(10);
    g->tactics_members.push_back(MakeMember(900, now - 1));
    guilds.Insert(g);
    RunSweep(guilds, /*chars=*/nullptr);   // null → skip back-pointer
    if (auto gg = guilds.Find(10))
    {
        std::lock_guard gl(gg->lock);
        EXPECT(gg->tactics_members.empty());
    }
}

} // namespace

int main()
{
    Test_Empty();
    Test_MixedExpiry();
    Test_ZeroEndTimeNeverExpires();
    Test_NullCharRegistry();

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_tactics_sweep "
                    "(4 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_tactics_sweep "
                    "(%d failure%s)\n", g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
