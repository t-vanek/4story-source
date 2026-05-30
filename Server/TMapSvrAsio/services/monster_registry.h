#pragma once

// Live monster state — server-side only, not persisted. The
// SpawnManager (F13.x consolidation) inserts MonsterInstance rows
// when a spawn point fires, and removes them on death. Client-facing
// CS_MONSTER_/MW_ broadcasts walk the registry filtered by channel /
// map.
//
// F13 ships the data structure and a few simple operations. The AI
// tick (move / chase / attack) that actually mutates this state lives
// in the consolidation pass — pulling it forward would couple F13 to
// the combat / damage layer that's still TODO from F11.

#include "domain/monster.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace tmapsvr {

class IMonsterRegistry
{
public:
    virtual ~IMonsterRegistry() = default;

    // Insert / replace by instance id. The SpawnManager assigns ids
    // from a counter; the registry doesn't generate them itself
    // (keeps the assignment policy out of the storage).
    virtual void Insert(MonsterInstance m) = 0;

    virtual void Remove(std::uint32_t instance_id) = 0;

    virtual std::optional<MonsterInstance>
        Find(std::uint32_t instance_id) const = 0;

    // Subtract `dmg` from the monster's HP (clamped at 0) and return the
    // updated instance — dwHP == 0 means it just died and the caller
    // should Remove it + award the kill. Returns nullopt when the
    // instance isn't in the registry (already gone / never existed). The
    // mutation is atomic under the registry lock.
    virtual std::optional<MonsterInstance>
        ApplyDamage(std::uint32_t instance_id, std::uint32_t dmg) = 0;

    // Snapshot of all monsters on (channel, map_id). Used by the
    // CS_CONREADY enter-map broadcast (once that lands) to spam the
    // joining client with one CS_MONSPAWN_REQ per monster in view.
    virtual std::vector<MonsterInstance>
        ListInMap(std::uint8_t channel, std::uint16_t map_id) const = 0;

    virtual std::size_t Size() const = 0;
};

class InMemoryMonsterRegistry final : public IMonsterRegistry
{
public:
    void Insert(MonsterInstance m) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_rows[m.dwInstanceID] = m;
    }

    void Remove(std::uint32_t instance_id) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_rows.erase(instance_id);
    }

    std::optional<MonsterInstance>
        Find(std::uint32_t instance_id) const override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        const auto it = m_rows.find(instance_id);
        if (it == m_rows.end()) return std::nullopt;
        return it->second;
    }

    std::optional<MonsterInstance>
        ApplyDamage(std::uint32_t instance_id, std::uint32_t dmg) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        const auto it = m_rows.find(instance_id);
        if (it == m_rows.end()) return std::nullopt;
        auto& m = it->second;
        m.dwHP = (dmg >= m.dwHP) ? 0u : (m.dwHP - dmg);
        return m;
    }

    std::vector<MonsterInstance>
        ListInMap(std::uint8_t channel, std::uint16_t map_id) const override
    {
        std::vector<MonsterInstance> out;
        std::lock_guard<std::mutex> lk(m_mtx);
        for (const auto& [_, m] : m_rows)
        {
            if (m.bChannel == channel && m.wMapID == map_id)
                out.push_back(m);
        }
        return out;
    }

    std::size_t Size() const override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        return m_rows.size();
    }

private:
    mutable std::mutex                                   m_mtx;
    std::unordered_map<std::uint32_t, MonsterInstance>   m_rows;
};

} // namespace tmapsvr
