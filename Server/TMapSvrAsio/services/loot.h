#pragma once

// Monster item-drop roll — faithful port of CTMonster::AddItem's item
// loop (TMonster.cpp:1102-1167). On death a monster rolls its drop table
// (TMONITEMCHART, one MonItemEntry per row) up to bDropCount times:
//
//   for i in [0, bDropCount):
//     if (bItemProb + addItemDrop > rand()%100):      // monster-level gate
//        choice = TRand(totalWeight)                   // weighted pick…
//        select the entry whose cumulative weight first exceeds choice
//        if (entry NORMAL1..4 probs all > rand()%100): // per-item gate ×4
//            drop the entry's item
//
// This reproduces the gate / selection / RNG-consumption order exactly
// (including the `&&` short-circuit on the four NORMAL gates — a failing
// earlier gate skips the later rand() calls, which the legacy relies on).
// `rand_below(n) → [0, n)` is the same stub the money roll uses; it
// stands in for both `rand()%100` and TNetDef.h's TRand(n) (whose
// multi-rand composition is only a Windows RAND_MAX workaround — the
// contract is still [0, n)).
//
// Item *resolution* is bounded to the already-portable case: a fixed
// chart item (bChartType != 0, wItemID != 0). The deferred tiers are
// SKIPPED (not aborted — the rest of the table still rolls) and each is a
// follow-up gated on the item-template / magic charts:
//   * magic-item entries  (bChartType == 0 → a prebuilt TITEMMAGIC copy),
//   * item-id range picks (wItemID == 0 → MonChoiceItem over [Min,Max]),
//   * magic / set / rare options (MakeSpecialItem, TMonster.cpp:1189).
// The legacy also stops when the corpse inventory fills (GetBlankPos →
// INVALID_SLOT); that capacity cap is applied at corpse-insert time (the
// loot wire wave), not in the pure roll.

#include "domain/monster.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace tmapsvr {

// Roll a monster's drop table. `entries` are its TMONITEMCHART rows;
// `base_item_prob` / `drop_count` are the TMONSTERCHART bItemProb /
// bDropCount; `add_item_drop` is the event/buff bonus (0 = none).
// Returns the dropped item template ids (empty when nothing drops).
inline std::vector<std::uint16_t> RollItemDrops(
    const std::vector<MonItemEntry>& entries,
    std::uint8_t  base_item_prob,
    std::uint8_t  drop_count,
    std::uint8_t  add_item_drop,
    const std::function<std::uint32_t(std::uint32_t)>& rand_below)
{
    std::vector<std::uint16_t> dropped;
    if (entries.empty())
        return dropped;

    // Sum of weights == legacy m_dwMaxWeight (precomputed at load there;
    // a drop table is a handful of rows so summing per-kill is cheap).
    // No positive weight → nothing can be selected (the legacy guards
    // `if(!m_dwMaxWeight) return;`).
    std::uint32_t total_weight = 0;
    for (const auto& e : entries)
        total_weight += e.wWeight;
    if (total_weight == 0)
        return dropped;

    const std::uint32_t prob = static_cast<std::uint32_t>(base_item_prob)
                             + static_cast<std::uint32_t>(add_item_drop);

    for (std::uint32_t i = 0; i < drop_count; ++i)
    {
        // Monster-level drop gate (prob may exceed 100 → always drops).
        if (!(prob > rand_below(100)))
            continue;

        // Weighted selection: choice in [0, total_weight); pick the entry
        // whose running weight sum first exceeds it.
        const std::uint32_t choice = rand_below(total_weight);
        std::uint32_t cur = 0;
        std::size_t   sel = 0;
        for (std::size_t j = 0; j < entries.size(); ++j)
        {
            cur += entries[j].wWeight;
            if (choice < cur) { sel = j; break; }
        }
        const MonItemEntry& e = entries[sel];

        // Per-item NORMAL gate ×4 — all four must pass, evaluated in order
        // (TMonster.cpp:1123-1126). `&&` short-circuits, matching legacy.
        if (e.bItemProb[MIP_NORMAL1] > rand_below(100) &&
            e.bItemProb[MIP_NORMAL2] > rand_below(100) &&
            e.bItemProb[MIP_NORMAL3] > rand_below(100) &&
            e.bItemProb[MIP_NORMAL4] > rand_below(100))
        {
            // Faithful resolution: a fixed chart item. Deferred tiers
            // (magic item / range pick) produce no concrete id this wave.
            if (e.bChartType != 0 && e.wItemID != 0)
                dropped.push_back(e.wItemID);
        }
    }

    return dropped;
}

} // namespace tmapsvr
