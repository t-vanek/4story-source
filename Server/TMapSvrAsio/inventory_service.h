#pragma once

// IInventoryService — item CRUD for live player sessions.
//
// ItemInstance is the in-memory representation of one DB row from
// TCHARITEMTABLE (or similar). It is richer than the snapshot `InvenItem`
// used in CS_CHARINFO_ACK — it carries enchant level, magic attributes,
// durability, and expiry.
//
// Lifecycle:
//   CharSnapshot::inventory (InvenItem[]) is the login snapshot.
//   IInventoryService is the live store used by OnMoveItemReq,
//   OnItemUseReq, etc.  Items survive across handler calls.
//
// Source:
//   TItem.h / TInven.h — CTItem / CTInven class fields
//   CSSender.cpp:502    — CTItem::WrapPacketClient serialisation

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace tmapsvr {

// Inventory-type constants (legacy INVEN_* enum)
namespace InvenType {
    constexpr std::uint8_t Main  = 0;   // INVEN_INVEN
    constexpr std::uint8_t Equip = 1;   // INVEN_EQUIP
    constexpr std::uint8_t Cash  = 2;   // INVEN_CASH
}

// Magic attribute on an item
struct ItemMagic
{
    std::uint8_t  id    = 0;
    std::uint16_t value = 0;
};

// Live item instance — one row in TCHARITEMTABLE.
// Has std::vector → Layer 3.
struct ItemInstance
{
    std::uint64_t dl_id      = 0;   // unique DB instance ID (m_dlID)
    std::uint16_t item_id    = 0;   // template ID
    std::uint8_t  inven_id   = 0;   // inventory slot index (m_bItemID)
    std::uint8_t  inven_type = 0;   // InvenType::Main / Equip / Cash

    std::uint8_t  level      = 0;   // m_bLevel  (enchant level)
    std::uint8_t  gem        = 0;   // m_bGem
    std::uint8_t  count      = 1;   // m_bCount  (stack size)
    std::uint8_t  g_level    = 0;   // m_bGLevel (grade)
    std::uint8_t  refine_cur = 0;   // m_bRefineCur
    std::uint32_t dura_max   = 0;
    std::uint32_t dura_cur   = 0;
    std::int64_t  end_time   = 0;   // m_dEndTime  (0 = permanent)
    std::uint8_t  eld        = 0;   // ELD enhancement (m_dwExtValue[IEV_ELD])
    std::uint8_t  g_effect   = 0;   // m_bGradeEffect

    std::vector<ItemMagic> magic;   // m_mapTMAGIC entries

    bool IsExpired(std::int64_t now_unix) const
    {
        return end_time > 0 && now_unix >= end_time;
    }
};

// ---------------------------------------------------------------------------
// IInventoryService
// ---------------------------------------------------------------------------

class IInventoryService
{
public:
    virtual ~IInventoryService() = default;

    // Load all items for char_id into the live store (called at login).
    virtual void LoadCharItems(std::uint32_t                char_id,
                               std::vector<ItemInstance>   items) = 0;

    // Release all items for char_id (called on disconnect).
    virtual void UnloadCharItems(std::uint32_t char_id) = 0;

    // Get all items in one inventory type for a char.
    virtual std::vector<const ItemInstance*>
        GetItems(std::uint32_t char_id, std::uint8_t inven_type) const = 0;

    // Find one item by (inven_type, inven_id) slot.
    virtual const ItemInstance*
        FindItem(std::uint32_t char_id,
                 std::uint8_t  inven_type,
                 std::uint8_t  inven_id) const = 0;

    // Move item within the same char's inventory (also used for equip).
    // Returns false on conflict / item-not-found.
    virtual bool MoveItem(std::uint32_t char_id,
                          std::uint8_t  src_type, std::uint8_t src_slot,
                          std::uint8_t  dst_type, std::uint8_t dst_slot,
                          std::uint8_t  count) = 0;

    // Remove one item by slot (consume, drop).
    // Returns the removed item or nullopt if not found.
    virtual std::optional<ItemInstance>
        RemoveItem(std::uint32_t char_id,
                   std::uint8_t  inven_type,
                   std::uint8_t  inven_id) = 0;

    // Add an item to inventory (pickup, reward).
    virtual bool AddItem(std::uint32_t char_id, ItemInstance item) = 0;
};

// ---------------------------------------------------------------------------
// FakeInventoryService — in-memory, for unit tests
// ---------------------------------------------------------------------------

class FakeInventoryService : public IInventoryService
{
public:
    void LoadCharItems(std::uint32_t              char_id,
                       std::vector<ItemInstance>  items) override
    {
        m_store[char_id] = std::move(items);
    }

    void UnloadCharItems(std::uint32_t char_id) override
    {
        m_store.erase(char_id);
    }

    std::vector<const ItemInstance*>
    GetItems(std::uint32_t char_id, std::uint8_t inven_type) const override
    {
        std::vector<const ItemInstance*> result;
        auto it = m_store.find(char_id);
        if (it == m_store.end()) return result;
        for (const auto& item : it->second)
            if (item.inven_type == inven_type)
                result.push_back(&item);
        return result;
    }

    const ItemInstance*
    FindItem(std::uint32_t char_id,
             std::uint8_t  inven_type,
             std::uint8_t  inven_id) const override
    {
        auto it = m_store.find(char_id);
        if (it == m_store.end()) return nullptr;
        for (const auto& item : it->second)
            if (item.inven_type == inven_type && item.inven_id == inven_id)
                return &item;
        return nullptr;
    }

    bool MoveItem(std::uint32_t char_id,
                  std::uint8_t  src_type, std::uint8_t src_slot,
                  std::uint8_t  dst_type, std::uint8_t dst_slot,
                  std::uint8_t  /*count*/) override
    {
        auto it = m_store.find(char_id);
        if (it == m_store.end()) return false;
        for (auto& item : it->second)
        {
            if (item.inven_type == src_type && item.inven_id == src_slot)
            {
                item.inven_type = dst_type;
                item.inven_id   = dst_slot;
                return true;
            }
        }
        return false;
    }

    std::optional<ItemInstance>
    RemoveItem(std::uint32_t char_id,
               std::uint8_t  inven_type,
               std::uint8_t  inven_id) override
    {
        auto it = m_store.find(char_id);
        if (it == m_store.end()) return std::nullopt;
        auto& vec = it->second;
        for (auto vit = vec.begin(); vit != vec.end(); ++vit)
        {
            if (vit->inven_type == inven_type && vit->inven_id == inven_id)
            {
                ItemInstance removed = std::move(*vit);
                vec.erase(vit);
                return removed;
            }
        }
        return std::nullopt;
    }

    bool AddItem(std::uint32_t char_id, ItemInstance item) override
    {
        m_store[char_id].push_back(std::move(item));
        return true;
    }

private:
    std::unordered_map<std::uint32_t, std::vector<ItemInstance>> m_store;
};

} // namespace tmapsvr
