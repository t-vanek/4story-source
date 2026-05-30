#pragma once

// Monster combat-stat chart, loaded once at boot from TMONATTRCHART.
//
// Stats vary by level, so the row is keyed by (monster id, level). The
// SpawnManager looks up (template id, template level) for a monster's
// real dwMaxHP; the combat layer will look up the same row for the
// attack / defense inputs to the damage formula.
//
// Read-only static content like the other charts — the SOCI impl loads
// every row once; Find is an O(1) lookup.

#include "domain/monster.h"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace tmapsvr {

class IMonAttrChart
{
public:
    virtual ~IMonAttrChart() = default;

    // Stats for a monster at a level, or nullopt when the chart has no
    // row (a data hole — the SpawnManager falls back to a placeholder HP).
    virtual std::optional<MonsterAttr>
        Find(std::uint16_t mon_id, std::uint8_t level) const = 0;

    virtual std::size_t Size() const = 0;
};

// (monster id, level) → 24-bit composite key. Header-only so both the
// SOCI impl and the test fake reuse it.
inline std::uint32_t MonAttrKey(std::uint16_t mon_id, std::uint8_t level)
{
    return (static_cast<std::uint32_t>(mon_id) << 8) | level;
}

// In-memory impl + test fake. Add() inserts a row.
class InMemoryMonAttrChart final : public IMonAttrChart
{
public:
    void Add(const MonsterAttr& a)
    {
        m_rows[MonAttrKey(a.wID, a.bLevel)] = a;
    }

    std::optional<MonsterAttr>
        Find(std::uint16_t mon_id, std::uint8_t level) const override
    {
        const auto it = m_rows.find(MonAttrKey(mon_id, level));
        if (it == m_rows.end()) return std::nullopt;
        return it->second;
    }

    std::size_t Size() const override { return m_rows.size(); }

private:
    std::unordered_map<std::uint32_t, MonsterAttr> m_rows;
};

} // namespace tmapsvr
