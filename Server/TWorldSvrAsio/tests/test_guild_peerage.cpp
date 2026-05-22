// W3a-5 unit test for CheckPeerage. Pure logic — no asio, no
// registry, no DB. Builds synthetic TGuild + TGuildLevelRow
// instances and exercises every legacy branch.
//
// Scenarios mirror the legacy switch (TGuild.cpp:205) plus the
// W3a-5 relaxed null-level branch:
//   1. new_peer = 0 (clear)            → always TRUE
//   2. new_peer > MAX_GUILD_PEER_COUNT → FALSE
//   3. slot cap reached                → FALSE
//   4. slot cap NOT reached            → TRUE (level 1/2 band)
//   5. chief-only band, non-chief duty → FALSE
//   6. chief-only band, chief duty     → TRUE
//   7. null level_row (no TGUILDCHART) → relaxed (TRUE up to cap=∞)

#include "../services/guild_constants.h"
#include "../services/guild_peerage.h"
#include "../services/guild_registry.h"

#include <cstdio>

namespace {

int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

tworldsvr::TGuildLevelRow MakeLvl(std::uint8_t level,
                                   std::array<std::uint8_t, 5> peers)
{
    tworldsvr::TGuildLevelRow r;
    r.level      = level;
    r.peer_slots = peers;
    return r;
}

// TGuild owns a std::mutex so it can't be moved or copied —
// callers pass a long-lived reference into the helper instead.
void InitGuildAt(tworldsvr::TGuild& g,
                 std::uint8_t       level,
                 std::vector<std::pair<std::uint32_t,
                                        std::uint8_t>> members_peer)
{
    g.level = level;
    g.members.clear();
    for (const auto& [id, peer] : members_peer)
    {
        tworldsvr::TGuildMember m;
        m.char_id = id;
        m.peer    = peer;
        m.duty    = 0;
        g.members.push_back(m);
    }
}

} // namespace

int main()
{
    using namespace tworldsvr;
    using tworldsvr::guild::CheckPeerage;
    constexpr std::uint8_t kChief    = guild::kDutyChief;     // 2
    constexpr std::uint8_t kViceChief = guild::kDutyViceChief; // 1
    constexpr std::uint8_t kNone     = guild::kDutyNone;      // 0
    constexpr std::uint8_t kBaron    = 1;
    constexpr std::uint8_t kViscount = 2;
    constexpr std::uint8_t kDuke     = 5;

    // --- 1: new_peer = 0 always OK ---------------------------------
    {
        auto lvl = MakeLvl(5, {0, 0, 0, 0, 0});
        TGuild g; InitGuildAt(g,5, {});
        EXPECT(CheckPeerage(&lvl, kChief, 0, g));
        EXPECT(CheckPeerage(&lvl, kNone,  0, g));
        EXPECT(CheckPeerage(nullptr, kNone, 0, g));   // null level too
    }

    // --- 2: out-of-range new_peer ---------------------------------
    {
        auto lvl = MakeLvl(5, {5, 5, 5, 5, 5});
        TGuild g; InitGuildAt(g,5, {});
        EXPECT(!CheckPeerage(&lvl, kChief, 6, g));     // > 5
        EXPECT(!CheckPeerage(&lvl, kChief, 99, g));
    }

    // --- 3: slot cap reached -------------------------------------
    {
        auto lvl = MakeLvl(2, {2, 0, 0, 0, 0});        // 2 barons max
        TGuild g; InitGuildAt(g,2, {{10, kBaron}, {11, kBaron}});
        EXPECT(!CheckPeerage(&lvl, kChief, kBaron, g));  // full
    }

    // --- 4: slot cap NOT reached + level 1/2 (any duty) ----------
    {
        auto lvl = MakeLvl(2, {3, 0, 0, 0, 0});
        TGuild g; InitGuildAt(g,2, {{10, kBaron}});
        EXPECT(CheckPeerage(&lvl, kNone,  kBaron, g));
        EXPECT(CheckPeerage(&lvl, kChief, kBaron, g));
    }

    // --- 5: chief-only band, non-chief duty refused --------------
    {
        // Level 3-4 → BARON is chief-only.
        auto lvl = MakeLvl(3, {3, 0, 0, 0, 0});
        TGuild g; InitGuildAt(g,3, {});
        EXPECT(!CheckPeerage(&lvl, kNone,      kBaron, g));
        EXPECT(!CheckPeerage(&lvl, kViceChief, kBaron, g));
    }

    // --- 6: chief-only band, chief duty allowed -----------------
    {
        auto lvl = MakeLvl(3, {3, 0, 0, 0, 0});
        TGuild g; InitGuildAt(g,3, {});
        EXPECT(CheckPeerage(&lvl, kChief, kBaron, g));
    }

    // Level 5-6 → VISCOUNT chief-only; BARON now any duty.
    {
        auto lvl = MakeLvl(6, {3, 3, 0, 0, 0});
        TGuild g; InitGuildAt(g,6, {});
        EXPECT(CheckPeerage(&lvl, kNone, kBaron, g));            // any duty
        EXPECT(!CheckPeerage(&lvl, kNone, kViscount, g));        // chief-only
        EXPECT(CheckPeerage(&lvl, kChief, kViscount, g));
    }

    // Level 10 → DUKE chief-only.
    {
        auto lvl = MakeLvl(10, {5, 5, 5, 5, 5});
        TGuild g; InitGuildAt(g,10, {});
        EXPECT(!CheckPeerage(&lvl, kNone,      kDuke, g));
        EXPECT(CheckPeerage(&lvl, kChief,      kDuke, g));
        EXPECT(CheckPeerage(&lvl, kViceChief,  kBaron, g));
    }

    // --- 7: null level_row → relaxed gate ------------------------
    //
    // Slot cap is the only thing left to enforce; without a chart
    // there's nothing to compare against, so we trust the wire.
    // (The W3a-4d "missing TGUILDCHART = relaxed dev gate"
    // contract; legacy refuses, we don't.)
    {
        TGuild g; InitGuildAt(g, 5, {});
        EXPECT(CheckPeerage(nullptr, kNone,  kBaron, g));
        EXPECT(CheckPeerage(nullptr, kChief, kDuke, g));
        EXPECT(!CheckPeerage(nullptr, kChief, 6, g));   // still global cap
    }

    if (g_fails == 0)
        std::printf("PASS test_tworldsvr_asio_guild_peerage (7 scenarios)\n");
    else
        std::printf("FAIL test_tworldsvr_asio_guild_peerage (%d failure%s)\n",
            g_fails, g_fails == 1 ? "" : "s");
    return g_fails == 0 ? 0 : 1;
}
