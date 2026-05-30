// Unit test: loot.h — the monster item-drop roll, faithful to
// CTMonster::AddItem's item loop (TMonster.cpp:1102-1167). Covers the
// monster-level prob gate, weighted entry selection, the four NORMAL
// per-item gates (incl. && short-circuit), drop-count looping, the
// add-item-drop bonus, and the bounded item resolution (fixed chart item
// drops; magic-item / zero-weight entries don't).

#include "services/loot.h"
#include "domain/monster.h"

#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <vector>

namespace {
int g_fails = 0;
#define EXPECT(cond) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #cond); \
        ++g_fails; \
    } \
} while (0)

using tmapsvr::MonItemEntry;

// A fixed chart-item entry that passes its four NORMAL gates for any
// rand < 100 — the common "this row always resolves" building block.
MonItemEntry ChartItem(std::uint16_t item_id, std::uint16_t weight)
{
    MonItemEntry e;
    e.bChartType = 1;
    e.wItemID    = item_id;
    e.wWeight    = weight;
    e.bItemProb[tmapsvr::MIP_NORMAL1] = 100;
    e.bItemProb[tmapsvr::MIP_NORMAL2] = 100;
    e.bItemProb[tmapsvr::MIP_NORMAL3] = 100;
    e.bItemProb[tmapsvr::MIP_NORMAL4] = 100;
    return e;
}

// rand_below stub that replays a scripted sequence (then 0s forever).
// Copyable so it binds to std::function.
std::function<std::uint32_t(std::uint32_t)> Seq(std::vector<std::uint32_t> vals)
{
    auto idx = std::make_shared<std::size_t>(0);
    return [vals = std::move(vals), idx](std::uint32_t) -> std::uint32_t
    {
        return (*idx < vals.size()) ? vals[(*idx)++] : 0u;
    };
}

const auto Zero = [](std::uint32_t) -> std::uint32_t { return 0u; };
} // namespace

int main()
{
    using namespace tmapsvr;

    // --- empty table / zero weight → no drops -------------------------
    {
        EXPECT(RollItemDrops({}, 100, 5, 0, Zero).empty());

        std::vector<MonItemEntry> zero{ ChartItem(500, 0) };   // weight 0
        EXPECT(RollItemDrops(zero, 100, 5, 0, Zero).empty());
    }

    // --- monster-level prob gate --------------------------------------
    {
        std::vector<MonItemEntry> tbl{ ChartItem(500, 10) };

        // prob 0 never passes (0 > rand[0,99] is always false).
        EXPECT(RollItemDrops(tbl, 0, 1, 0, Zero).empty());

        // prob 100 passes the gate; everything resolves → one drop of 500.
        auto d = RollItemDrops(tbl, 100, 1, 0, Zero);
        EXPECT(d.size() == 1 && d[0] == 500);
    }

    // --- the four NORMAL gates (a failing one blocks the drop) --------
    {
        MonItemEntry e = ChartItem(500, 10);
        e.bItemProb[MIP_NORMAL2] = 0;          // 2nd gate: 0 > rand → fail
        std::vector<MonItemEntry> tbl{ e };
        // gate(100>0) pass, choice 0, NORMAL1(100>0) pass, NORMAL2(0>0) fail.
        EXPECT(RollItemDrops(tbl, 100, 1, 0, Zero).empty());
    }

    // --- weighted selection lands in the right bucket -----------------
    {
        // A: weight 10 → choice [0,10);  B: weight 90 → choice [10,100).
        std::vector<MonItemEntry> tbl{ ChartItem(100, 10), ChartItem(200, 90) };

        // choice 5 → A. seq = gate, choice, 4×normal.
        auto da = RollItemDrops(tbl, 100, 1, 0, Seq({ 0, 5, 0, 0, 0, 0 }));
        EXPECT(da.size() == 1 && da[0] == 100);

        // choice 50 → B (cur after A = 10, 50 >= 10; after B = 100, 50 < 100).
        auto db = RollItemDrops(tbl, 100, 1, 0, Seq({ 0, 50, 0, 0, 0, 0 }));
        EXPECT(db.size() == 1 && db[0] == 200);

        // boundary: choice 10 → B (10 < 10 false on A, 10 < 100 true on B).
        auto dc = RollItemDrops(tbl, 100, 1, 0, Seq({ 0, 10, 0, 0, 0, 0 }));
        EXPECT(dc.size() == 1 && dc[0] == 200);
    }

    // --- drop_count loops independently -------------------------------
    {
        std::vector<MonItemEntry> tbl{ ChartItem(500, 10) };
        auto d = RollItemDrops(tbl, 100, 3, 0, Zero);   // all pass thrice
        EXPECT(d.size() == 3 && d[0] == 500 && d[1] == 500 && d[2] == 500);
    }

    // --- add_item_drop bonus lifts a failing base prob over the gate --
    {
        std::vector<MonItemEntry> tbl{ ChartItem(500, 10) };

        // base 40 vs rand 50 → 40 > 50 false → no drop.
        EXPECT(RollItemDrops(tbl, 40, 1, 0, Seq({ 50 })).empty());

        // base 40 + bonus 40 = 80 vs rand 50 → 80 > 50 true → drop.
        auto d = RollItemDrops(tbl, 40, 1, 40, Seq({ 50, 0, 0, 0, 0, 0 }));
        EXPECT(d.size() == 1 && d[0] == 500);
    }

    // --- deferred resolution: magic-item entry rolls but yields no id -
    {
        MonItemEntry magic = ChartItem(999, 10);
        magic.bChartType = 0;                  // magic item → deferred
        std::vector<MonItemEntry> tbl{ magic };
        // passes every gate, but bChartType 0 resolves to nothing this wave.
        EXPECT(RollItemDrops(tbl, 100, 1, 0, Zero).empty());

        // …and a chart-type entry with no fixed id (range pick) also skips.
        MonItemEntry range = ChartItem(0, 10); // wItemID 0 → MonChoiceItem, deferred
        std::vector<MonItemEntry> tbl2{ range };
        EXPECT(RollItemDrops(tbl2, 100, 1, 0, Zero).empty());
    }

    if (g_fails == 0)
        std::printf("test_loot: prob gate + weighted select + NORMAL gates + "
                    "drop-count + bonus + bounded resolution OK\n");
    return g_fails == 0 ? 0 : 1;
}
