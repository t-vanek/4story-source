#pragma once

// MonsterState — live monster instance and IMonsterRegistry.
//
// Each running monster has one `MonsterState`. They are stored in
// `LocalMonsterRegistry` which is queried:
//   * On player EnterMap → send CS_ADDMON_ACK for nearby monsters
//   * On CS_ACTION_REQ / CS_SKILLUSE_REQ → apply damage
//   * On tick → AI command execution (F4b)
//
// Instance IDs: server-local DWORD, unique per channel. The legacy
// code used `CTMapSvrModule::m_dwNextMonID` as a monotonic counter.
// LocalMonsterRegistry exposes NextInstanceId() for the same purpose.
//
// Cell-grid integration: monsters don't live in LocalMapState (which
// is player-only). Proximity queries use the same CELL_SIZE=64 math
// but go through IMonsterRegistry::GetNeighborIds separately.
//
// Source: Server/TMapSvr/TMonster.h (CTMonster fields)

#include "legacy_port/types.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tmapsvr {

// ---------------------------------------------------------------------------
// MonsterState — running instance
// ---------------------------------------------------------------------------

struct MonsterState
{
    std::uint32_t instance_id  = 0;   // unique per-channel DWORD
    std::uint16_t template_id  = 0;   // which TMONSTERCHART row
    std::uint8_t  level        = 0;
    std::uint32_t max_hp       = 0;
    std::uint32_t hp           = 0;
    std::uint32_t max_mp       = 0;
    std::uint32_t mp           = 0;
    float         pos_x        = 0.0f;
    float         pos_y        = 0.0f;
    float         pos_z        = 0.0f;
    std::uint16_t dir          = 0;
    std::uint8_t  action       = 0;   // STAND / WALK / RUN / ATTACK …
    std::uint8_t  mode         = 0;   // 0=peaceful 1=combat
    std::uint8_t  country      = 0;   // faction
    std::uint16_t spawn_id     = 0;   // which MonsterSpawn created this
    std::uint8_t  channel      = 0;

    bool IsDead() const { return hp == 0; }
    bool IsAlive() const { return hp > 0; }
};

// ---------------------------------------------------------------------------
// IMonsterRegistry
// ---------------------------------------------------------------------------

class IMonsterRegistry
{
public:
    virtual ~IMonsterRegistry() = default;

    virtual void Add(MonsterState mon) = 0;

    // Returns false if instance_id not found.
    virtual bool Remove(std::uint32_t instance_id) = 0;

    // Returns nullptr if not found.
    virtual const MonsterState* Get(std::uint32_t instance_id) const = 0;
    virtual       MonsterState* GetMutable(std::uint32_t instance_id) = 0;

    // Apply `delta` HP change (positive = heal, negative = damage).
    // Clamps to [0, max_hp]. Returns new HP. Returns 0 if not found.
    virtual std::uint32_t ApplyHpDelta(std::uint32_t instance_id,
                                       std::int64_t  delta) = 0;

    // All monster instance_ids whose position falls within 3×3 cells
    // around (pos_x, pos_z). CELL_SIZE=64, same grid as LocalMapState.
    virtual std::vector<std::uint32_t>
        GetNeighborIds(float pos_x, float pos_z) const = 0;

    // Monotonic counter for issuing new instance IDs.
    virtual std::uint32_t NextInstanceId() = 0;

    virtual std::size_t Count() const = 0;
};

// ---------------------------------------------------------------------------
// LocalMonsterRegistry — single-threaded, in-memory
// ---------------------------------------------------------------------------

class LocalMonsterRegistry : public IMonsterRegistry
{
public:
    static constexpr int CELL_SIZE = 64;

    using CellKey = std::uint32_t;

    static CellKey MakeCellKey(float px, float pz)
    {
        const auto cx = static_cast<std::uint16_t>(static_cast<int>(px) / CELL_SIZE);
        const auto cz = static_cast<std::uint16_t>(static_cast<int>(pz) / CELL_SIZE);
        return (static_cast<std::uint32_t>(cz) << 16) | cx;
    }

    void Add(MonsterState mon) override
    {
        const auto id  = mon.instance_id;
        const auto key = MakeCellKey(mon.pos_x, mon.pos_z);
        m_cell_index[id] = key;
        m_cells[key].push_back(id);
        m_monsters[id] = std::move(mon);
    }

    bool Remove(std::uint32_t instance_id) override
    {
        auto mit = m_monsters.find(instance_id);
        if (mit == m_monsters.end()) return false;

        const auto key = MakeCellKey(mit->second.pos_x, mit->second.pos_z);
        auto& cell = m_cells[key];
        cell.erase(std::remove(cell.begin(), cell.end(), instance_id), cell.end());
        if (cell.empty()) m_cells.erase(key);

        m_cell_index.erase(instance_id);
        m_monsters.erase(mit);
        return true;
    }

    const MonsterState* Get(std::uint32_t id) const override
    {
        auto it = m_monsters.find(id);
        return it != m_monsters.end() ? &it->second : nullptr;
    }

    MonsterState* GetMutable(std::uint32_t id) override
    {
        auto it = m_monsters.find(id);
        return it != m_monsters.end() ? &it->second : nullptr;
    }

    std::uint32_t ApplyHpDelta(std::uint32_t id, std::int64_t delta) override
    {
        auto* mon = GetMutable(id);
        if (!mon) return 0;
        const std::int64_t new_hp =
            std::clamp(static_cast<std::int64_t>(mon->hp) + delta,
                       std::int64_t{0},
                       static_cast<std::int64_t>(mon->max_hp));
        mon->hp = static_cast<std::uint32_t>(new_hp);
        return mon->hp;
    }

    std::vector<std::uint32_t>
    GetNeighborIds(float pos_x, float pos_z) const override
    {
        const int cx = static_cast<int>(pos_x) / CELL_SIZE;
        const int cz = static_cast<int>(pos_z) / CELL_SIZE;
        std::vector<std::uint32_t> result;
        for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
        {
            const int nx = cx + dx, nz = cz + dz;
            if (nx < 0 || nz < 0) continue;
            const CellKey k =
                (static_cast<std::uint32_t>(nz) << 16) |
                 static_cast<std::uint16_t>(nx);
            auto it = m_cells.find(k);
            if (it == m_cells.end()) continue;
            result.insert(result.end(), it->second.begin(), it->second.end());
        }
        return result;
    }

    std::uint32_t NextInstanceId() override { return ++m_next_id; }

    std::size_t Count() const override { return m_monsters.size(); }

private:
    std::unordered_map<std::uint32_t, MonsterState>              m_monsters;
    std::unordered_map<std::uint32_t, CellKey>                   m_cell_index;
    std::unordered_map<CellKey, std::vector<std::uint32_t>>      m_cells;
    std::uint32_t                                                 m_next_id = 0;
};

} // namespace tmapsvr
