#pragma once

// Monster drop-table chart, loaded once at boot from TMONITEMCHART.
//
// Each monster (wMonID) owns N drop-table entries (MonItemEntry); the
// combat layer rolls them on death via services/loot.h to decide what
// items fall. TMONSTERCHART carries the monster-level bItemProb /
// bDropCount that gate the roll; this chart carries the per-entry weight
// + NORMAL probabilities the roll selects on.
//
// Read-only static content, like the other charts — the SOCI impl loads
// every row once and groups by monster id; ForMon is an O(1) lookup that
// returns the entry list (empty for a monster with no drop table).

#include "domain/monster.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace tmapsvr {

class IMonItemChart
{
public:
    virtual ~IMonItemChart() = default;

    // The drop-table entries for a monster, or an empty vector when the
    // monster has no TMONITEMCHART rows (most monsters that drop nothing
    // but money — the loot roll then yields no items).
    virtual const std::vector<MonItemEntry>&
        ForMon(std::uint16_t mon_id) const = 0;

    // Total rows loaded (boot log).
    virtual std::size_t Size() const = 0;
};

// Header-only in-memory impl — also the test fake. Add() appends a row;
// rows are grouped by monster id on insert.
class InMemoryMonItemChart final : public IMonItemChart
{
public:
    void Add(const MonItemEntry& e)
    {
        m_rows[e.wMonID].push_back(e);
        ++m_count;
    }

    const std::vector<MonItemEntry>&
        ForMon(std::uint16_t mon_id) const override
    {
        const auto it = m_rows.find(mon_id);
        if (it == m_rows.end()) return m_empty;
        return it->second;
    }

    std::size_t Size() const override { return m_count; }

private:
    std::unordered_map<std::uint16_t, std::vector<MonItemEntry>> m_rows;
    std::vector<MonItemEntry>                                    m_empty;
    std::size_t                                                  m_count = 0;
};

} // namespace tmapsvr
