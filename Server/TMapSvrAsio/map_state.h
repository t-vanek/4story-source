#pragma once

// IMapState — abstract AOI cell grid for one map channel.
//
// Models the 3×3-cell AOI system from the legacy TMap/TCell classes.
// The world is divided into CELL_SIZE×CELL_SIZE (64×64) unit squares;
// each cell holds the char_ids of players currently inside it.
// AOI = the 3×3 neighbourhood of cells centred on the player's cell
// (up to 9 cells, capped by world boundaries).
//
// IMapState is intentionally pure-data: it does not send packets.
// Callers (handlers_map.cpp) iterate the returned id lists and do the
// async sends themselves — keeping IO and AOI logic fully separated.
//
// Legacy references:
//   TMap.h / TMap.cpp    — CTMap::EnterMAP, LeaveMAP, OnMove,
//                          GetNeighbor
//   TCell.h / TCell.cpp  — CTCell, CTCell::EnterPlayer
//   NetCode.h            — CELL_SIZE = 64

#include "legacy_port/types_layer2.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tmapsvr {

using legacy::PlayerPresence;

// Result of IMapState::OnMove — symmetric difference of the AOI sets.
struct MoveDelta
{
    std::vector<std::uint32_t> entered_aoi;  // newly visible (send CS_ENTER_ACK)
    std::vector<std::uint32_t> left_aoi;     // no longer visible (send CS_LEAVE_ACK)
    std::vector<std::uint32_t> common_aoi;   // still visible (send CS_MOVE_ACK)
};

class IMapState
{
public:
    virtual ~IMapState() = default;

    // Register player at their starting position.
    // Returns char_ids of players already in the entering player's AOI
    // (caller sends CS_ENTER_ACK for each). The entering player's
    // PlayerPresence is stored for future AOI queries.
    virtual std::vector<std::uint32_t>
        EnterMap(std::uint32_t char_id,
                 const PlayerPresence& presence) = 0;

    // Unregister player on disconnect or channel change.
    // Returns char_ids in the leaving player's AOI (caller sends
    // CS_LEAVE_ACK to each of them about the departing player).
    virtual std::vector<std::uint32_t>
        LeaveMap(std::uint32_t char_id) = 0;

    // Update position after CS_MOVE_REQ. Returns AOI delta so the
    // handler can send CS_ENTER_ACK / CS_MOVE_ACK / CS_LEAVE_ACK.
    // Also updates the stored presence (pos/dir/action updated).
    virtual MoveDelta
        OnMove(std::uint32_t char_id,
               float new_x, float new_z) = 0;

    // Overwrite the full presence record (used after EnterMap to
    // propagate level/HP etc. from snapshot).
    virtual void
        UpdatePresence(std::uint32_t char_id, PlayerPresence p) = 0;

    // Look up a player's current presence. Returns nullptr if not
    // registered.
    virtual const PlayerPresence*
        GetPresence(std::uint32_t char_id) const = 0;

    // Get all char_ids visible from pos (3×3 cell neighbourhood).
    virtual std::vector<std::uint32_t>
        GetNeighborIds(float pos_x, float pos_z) const = 0;

    // True iff char_id is currently registered.
    virtual bool Contains(std::uint32_t char_id) const = 0;

    virtual std::size_t PlayerCount() const = 0;
};

// ---------------------------------------------------------------------------
// LocalMapState — single-threaded, in-memory.
// ---------------------------------------------------------------------------
//
// Cell coordinate formula (legacy NetCode.h:CELL_SIZE = 64):
//   cell_x = uint16_t(floor(pos_x) / 64)
//   cell_z = uint16_t(floor(pos_z) / 64)
//   cell_key = (uint32_t(cell_z) << 16) | uint32_t(cell_x)
//
// Neighbour scan: 3×3 around (cell_x, cell_z) — indices
//   [cell_x-1 .. cell_x+1] × [cell_z-1 .. cell_z+1]
// clamped to [0, UINT16_MAX]. Matches legacy GetNeighbor() exactly.

class LocalMapState : public IMapState
{
public:
    static constexpr int CELL_SIZE = 64;    // world units per cell

    using CellKey = std::uint32_t;

    static CellKey MakeCellKey(float pos_x, float pos_z)
    {
        const auto cx = static_cast<std::uint16_t>(
            static_cast<int>(pos_x) / CELL_SIZE);
        const auto cz = static_cast<std::uint16_t>(
            static_cast<int>(pos_z) / CELL_SIZE);
        return (static_cast<std::uint32_t>(cz) << 16) | cx;
    }

    static std::vector<CellKey> NeighborKeys(float pos_x, float pos_z)
    {
        const int cx = static_cast<int>(pos_x) / CELL_SIZE;
        const int cz = static_cast<int>(pos_z) / CELL_SIZE;
        std::vector<CellKey> result;
        result.reserve(9);
        for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
        {
            const int nx = cx + dx, nz = cz + dz;
            if (nx >= 0 && nz >= 0)
                result.push_back(
                    (static_cast<std::uint32_t>(nz) << 16) |
                     static_cast<std::uint16_t>(nx));
        }
        return result;
    }

    // IMapState
    std::vector<std::uint32_t>
    EnterMap(std::uint32_t char_id, const PlayerPresence& presence) override
    {
        m_presences[char_id] = presence;
        const CellKey key = MakeCellKey(presence.pos_x, presence.pos_z);
        m_cells[key].insert(char_id);
        m_char_cell[char_id] = key;

        std::vector<std::uint32_t> result;
        for (CellKey nk : NeighborKeys(presence.pos_x, presence.pos_z))
        {
            auto it = m_cells.find(nk);
            if (it == m_cells.end()) continue;
            for (std::uint32_t id : it->second)
                if (id != char_id) result.push_back(id);
        }
        return result;
    }

    std::vector<std::uint32_t>
    LeaveMap(std::uint32_t char_id) override
    {
        auto cell_it = m_char_cell.find(char_id);
        if (cell_it == m_char_cell.end()) return {};

        // Snapshot position before erasing presence
        float px = 0.0f, pz = 0.0f;
        {
            auto pit = m_presences.find(char_id);
            if (pit != m_presences.end())
            {
                px = pit->second.pos_x;
                pz = pit->second.pos_z;
            }
        }

        // Remove from cell
        CellKey key = cell_it->second;
        m_char_cell.erase(cell_it);
        auto& cell = m_cells[key];
        cell.erase(char_id);
        if (cell.empty()) m_cells.erase(key);
        m_presences.erase(char_id);

        // Notify all AOI neighbours
        std::vector<std::uint32_t> result;
        for (CellKey nk : NeighborKeys(px, pz))
        {
            auto it = m_cells.find(nk);
            if (it == m_cells.end()) continue;
            for (std::uint32_t id : it->second)
                if (id != char_id) result.push_back(id);
        }
        return result;
    }

    MoveDelta
    OnMove(std::uint32_t char_id, float new_x, float new_z) override
    {
        MoveDelta delta;
        auto cell_it = m_char_cell.find(char_id);
        if (cell_it == m_char_cell.end()) return delta;

        const CellKey old_key = cell_it->second;
        const CellKey new_key = MakeCellKey(new_x, new_z);

        // Update position in presence
        auto pit = m_presences.find(char_id);
        float old_x = 0.0f, old_z = 0.0f;
        if (pit != m_presences.end())
        {
            old_x = pit->second.pos_x;
            old_z = pit->second.pos_z;
            pit->second.pos_x = new_x;
            pit->second.pos_z = new_z;
        }

        if (old_key == new_key)
        {
            // Same cell — all AOI neighbours are common.
            for (CellKey nk : NeighborKeys(new_x, new_z))
            {
                auto it = m_cells.find(nk);
                if (it == m_cells.end()) continue;
                for (std::uint32_t id : it->second)
                    if (id != char_id) delta.common_aoi.push_back(id);
            }
            return delta;
        }

        // Cell transition — compute old/new key sets
        std::unordered_set<CellKey> old_set, new_set;
        for (CellKey k : NeighborKeys(old_x, old_z)) old_set.insert(k);
        for (CellKey k : NeighborKeys(new_x, new_z)) new_set.insert(k);

        // Move in cell maps
        {
            auto& oc = m_cells[old_key];
            oc.erase(char_id);
            if (oc.empty()) m_cells.erase(old_key);
        }
        m_cells[new_key].insert(char_id);
        cell_it->second = new_key;

        // Classify each occupied neighbour cell
        for (const auto& [k, cell] : m_cells)
        {
            const bool in_old = old_set.count(k) > 0;
            const bool in_new = new_set.count(k) > 0;
            for (std::uint32_t id : cell)
            {
                if (id == char_id) continue;
                if (in_new && !in_old)      delta.entered_aoi.push_back(id);
                else if (in_old && !in_new) delta.left_aoi.push_back(id);
                else if (in_old && in_new)  delta.common_aoi.push_back(id);
            }
        }
        return delta;
    }

    void UpdatePresence(std::uint32_t char_id, PlayerPresence p) override
    {
        m_presences[char_id] = std::move(p);
    }

    const PlayerPresence*
    GetPresence(std::uint32_t char_id) const override
    {
        auto it = m_presences.find(char_id);
        return it != m_presences.end() ? &it->second : nullptr;
    }

    std::vector<std::uint32_t>
    GetNeighborIds(float pos_x, float pos_z) const override
    {
        std::vector<std::uint32_t> result;
        for (CellKey nk : NeighborKeys(pos_x, pos_z))
        {
            auto it = m_cells.find(nk);
            if (it == m_cells.end()) continue;
            for (std::uint32_t id : it->second) result.push_back(id);
        }
        return result;
    }

    bool Contains(std::uint32_t char_id) const override
    {
        return m_char_cell.count(char_id) > 0;
    }

    std::size_t PlayerCount() const override { return m_presences.size(); }

private:
    // Cell storage: cell_key → set of char_ids in that cell
    std::unordered_map<CellKey, std::unordered_set<std::uint32_t>> m_cells;
    // Reverse: char_id → current cell key (for O(1) move)
    std::unordered_map<std::uint32_t, CellKey>                     m_char_cell;
    // Full presence record per player (for AOI broadcasts)
    std::unordered_map<std::uint32_t, PlayerPresence>              m_presences;
};

} // namespace tmapsvr
