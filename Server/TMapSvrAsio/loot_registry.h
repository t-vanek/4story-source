#pragma once

// ILootRegistry — per-monster loot after death.
//
// When a monster is killed (OnActionReq), GenerateLoot populates its
// loot table. Clients receive CS_MONITEMLIST_ACK on corpse interaction
// and CS_MONITEMTAKE_ACK when they pick up an item.
//
// LocalLootRegistry is the in-memory implementation; it is cleared
// when LocalSpawnManager removes the monster on respawn.

#include "inventory_service.h"  // ItemInstance

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace tmapsvr {

struct MonsterLoot
{
    std::uint32_t          gold   = 0;
    std::uint32_t          silver = 0;
    std::uint32_t          copper = 0;
    std::vector<ItemInstance> items;
};

class ILootRegistry
{
public:
    virtual ~ILootRegistry() = default;

    virtual void             SetLoot(std::uint32_t mon_id, MonsterLoot loot) = 0;
    virtual const MonsterLoot* GetLoot(std::uint32_t mon_id) const = 0;

    // Remove one item from the loot table by its inven_id (slot in loot list).
    // Returns the item or nullopt.
    virtual std::optional<ItemInstance>
        TakeItem(std::uint32_t mon_id, std::uint8_t loot_slot) = 0;

    virtual void ClearLoot(std::uint32_t mon_id) = 0;
    virtual bool HasLoot(std::uint32_t mon_id)  const = 0;
};

class LocalLootRegistry : public ILootRegistry
{
public:
    void SetLoot(std::uint32_t mon_id, MonsterLoot loot) override
    {
        m_loot[mon_id] = std::move(loot);
    }

    const MonsterLoot* GetLoot(std::uint32_t mon_id) const override
    {
        auto it = m_loot.find(mon_id);
        return it != m_loot.end() ? &it->second : nullptr;
    }

    std::optional<ItemInstance>
    TakeItem(std::uint32_t mon_id, std::uint8_t loot_slot) override
    {
        auto it = m_loot.find(mon_id);
        if (it == m_loot.end()) return std::nullopt;

        auto& items = it->second.items;
        for (auto vit = items.begin(); vit != items.end(); ++vit)
        {
            if (vit->inven_id == loot_slot)
            {
                ItemInstance taken = std::move(*vit);
                items.erase(vit);
                return taken;
            }
        }
        return std::nullopt;
    }

    void ClearLoot(std::uint32_t mon_id) override
    {
        m_loot.erase(mon_id);
    }

    bool HasLoot(std::uint32_t mon_id) const override
    {
        return m_loot.count(mon_id) > 0;
    }

private:
    std::unordered_map<std::uint32_t, MonsterLoot> m_loot;
};

// ---------------------------------------------------------------------------
// Stub loot generator — called when monster dies.
// Real loot tables need TITEMDROPCHART which is F5 Part 3.
// Stub: 50% chance to drop a small HP potion (item_id=1) per kill.
// ---------------------------------------------------------------------------

inline MonsterLoot GenerateStubLoot(std::uint8_t monster_level)
{
    MonsterLoot loot{};
    // Coin drop: level × 3 copper, level silver at every 5 levels
    loot.copper = static_cast<std::uint32_t>(monster_level) * 3;
    loot.silver = static_cast<std::uint32_t>(monster_level) / 5;

    // 50% item drop (deterministic based on level parity for tests)
    if (monster_level % 2 == 0)
    {
        ItemInstance item{};
        item.item_id    = 1;    // stub HP potion
        item.inven_id   = 0;    // loot slot 0
        item.inven_type = InvenType::Main;
        item.count      = 1;
        loot.items.push_back(std::move(item));
    }
    return loot;
}

} // namespace tmapsvr
