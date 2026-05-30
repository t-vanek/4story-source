#pragma once

// Monster corpse loot — the money + items a killed monster left behind,
// held until looted (or the corpse is reaped). Keyed by the dead monster's
// runtime instance id — the same id the client passes in
// CS_MONITEMLIST_REQ / CS_MONMONEYTAKE_REQ / CS_MONITEMTAKE_REQ.
//
// Faithful to the legacy model: CTMonster::AddItem fills the corpse's
// INVEN_DEFAULT on death (TMonster.cpp:1080) and the player loots via the
// corpse window — 4Story has no ground-item entity. Server-side only (not
// persisted), thread-safe like the monster registry (the combat coroutine
// populates it; the loot handlers drain it). A reaper/expiry timer is a
// follow-up — a corpse currently lingers until emptied or its monster
// instance id is reused.

#include "domain/inventory.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace tmapsvr {

struct Corpse
{
    std::uint32_t             dwMoney = 0;   // cooper held on the corpse
    std::vector<ItemInstance> items;         // each item's bItemID = corpse slot
};

class ICorpseRegistry
{
public:
    virtual ~ICorpseRegistry() = default;

    // Create / replace the corpse for a dead monster instance.
    virtual void Put(std::uint32_t mon_id, Corpse corpse) = 0;

    // Snapshot (money + items) for the loot list, or nullopt if no corpse.
    virtual std::optional<Corpse> Find(std::uint32_t mon_id) const = 0;

    // Take all money: returns the cooper held and zeroes it (0 if none /
    // no corpse). Faithful to MonMoneyTake.
    virtual std::uint32_t TakeMoney(std::uint32_t mon_id) = 0;

    // Peek one corpse item by its slot (bItemID) without removing it — the
    // take handler reads it, tries to place it in the player's bag, and
    // only then RemoveItem()s it (so a full bag leaves the corpse intact).
    virtual std::optional<ItemInstance>
        ItemAt(std::uint32_t mon_id, std::uint8_t slot) const = 0;

    // Remove one item by slot (after it's been placed in a bag).
    virtual void RemoveItem(std::uint32_t mon_id, std::uint8_t slot) = 0;

    // True when the corpse holds no money and no items (the take handlers
    // Remove() it once empty so the id can be reused).
    virtual bool IsEmpty(std::uint32_t mon_id) const = 0;

    // Drop the whole corpse.
    virtual void Remove(std::uint32_t mon_id) = 0;

    virtual std::size_t Size() const = 0;
};

class InMemoryCorpseRegistry final : public ICorpseRegistry
{
public:
    void Put(std::uint32_t mon_id, Corpse corpse) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_rows[mon_id] = std::move(corpse);
    }

    std::optional<Corpse> Find(std::uint32_t mon_id) const override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        const auto it = m_rows.find(mon_id);
        if (it == m_rows.end()) return std::nullopt;
        return it->second;
    }

    std::uint32_t TakeMoney(std::uint32_t mon_id) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        const auto it = m_rows.find(mon_id);
        if (it == m_rows.end()) return 0;
        const std::uint32_t money = it->second.dwMoney;
        it->second.dwMoney = 0;
        return money;
    }

    std::optional<ItemInstance>
        ItemAt(std::uint32_t mon_id, std::uint8_t slot) const override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        const auto it = m_rows.find(mon_id);
        if (it == m_rows.end()) return std::nullopt;
        for (const auto& item : it->second.items)
            if (item.bItemID == slot) return item;
        return std::nullopt;
    }

    void RemoveItem(std::uint32_t mon_id, std::uint8_t slot) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        const auto it = m_rows.find(mon_id);
        if (it == m_rows.end()) return;
        auto& items = it->second.items;
        for (auto i = items.begin(); i != items.end(); ++i)
            if (i->bItemID == slot) { items.erase(i); break; }
    }

    bool IsEmpty(std::uint32_t mon_id) const override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        const auto it = m_rows.find(mon_id);
        if (it == m_rows.end()) return true;
        return it->second.dwMoney == 0 && it->second.items.empty();
    }

    void Remove(std::uint32_t mon_id) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_rows.erase(mon_id);
    }

    std::size_t Size() const override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        return m_rows.size();
    }

private:
    mutable std::mutex                            m_mtx;
    std::unordered_map<std::uint32_t, Corpse>     m_rows;
};

// Build a corpse from a money total (cooper) + the item template ids a
// drop roll produced (services/loot.h RollItemDrops). Each item takes the
// next corpse slot (bItemID = index), count 1; grade / durability / magic
// stay at ItemInstance defaults (template-derived, deferred).
inline Corpse MakeCorpse(std::uint32_t money,
                         const std::vector<std::uint16_t>& item_ids)
{
    Corpse c;
    c.dwMoney = money;
    std::uint8_t slot = 0;
    for (const auto id : item_ids)
    {
        ItemInstance it;
        it.bItemID = slot++;
        it.wItemID = id;
        it.bCount  = 1;
        c.items.push_back(it);
    }
    return c;
}

} // namespace tmapsvr
