#pragma once

// Spawn-point → monster linkage, loaded once at boot from TMAPMONCHART.
//
// TMONSPAWNCHART defines *where* a group spawns but carries no monster
// id; TMAPMONCHART maps each spawn point (wSpawnID = SpawnPoint.wID) to
// its candidate monsters (wMonID + essential / leader / probability).
// The SpawnManager joins the two: for each spawn point it looks up the
// entries here and realises monsters from MonsterTemplate.
//
// Read-only static content, like the other charts — the SOCI impl loads
// every row once and groups by spawn id; ForSpawn is an O(1) lookup.

#include "domain/monster.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace tmapsvr {

class IMapMonChart
{
public:
    virtual ~IMapMonChart() = default;

    // The candidate monsters for a spawn point, or an empty span when
    // the spawn point has no TMAPMONCHART rows (a data hole — the
    // SpawnManager skips it).
    virtual const std::vector<MapMonEntry>&
        ForSpawn(std::uint16_t spawn_id) const = 0;

    // Total rows loaded (boot log).
    virtual std::size_t Size() const = 0;
};

// Header-only in-memory impl — also the test fake. Add() appends a row;
// rows are grouped by spawn id on insert.
class InMemoryMapMonChart final : public IMapMonChart
{
public:
    void Add(const MapMonEntry& e)
    {
        m_rows[e.wSpawnID].push_back(e);
        ++m_count;
    }

    const std::vector<MapMonEntry>&
        ForSpawn(std::uint16_t spawn_id) const override
    {
        const auto it = m_rows.find(spawn_id);
        if (it == m_rows.end()) return m_empty;
        return it->second;
    }

    std::size_t Size() const override { return m_count; }

private:
    std::unordered_map<std::uint16_t, std::vector<MapMonEntry>> m_rows;
    std::vector<MapMonEntry>                                    m_empty;
    std::size_t                                                 m_count = 0;
};

} // namespace tmapsvr
