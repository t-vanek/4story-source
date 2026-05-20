#pragma once

// SOCI-backed IInventoryService. Reads TITEMTABLE for all items
// belonging to a character (WHERE dwOwnerID = char_id AND bOwnerType = 1).
//
// Column mapping follows DBAccess.h::CTBLItem SELECT list (35 cols).
// Magic attributes (bMagic[6] + wValue[6]) populate ItemInstance::magic.
// ELD is the first extended value (dwExtValue[0]).
//
// Write-back (StoreItem, DeleteItem) is PENDING F5 Part 3.

#include "inventory_service.h"

namespace fourstory::db { class SessionPool; }

namespace tmapsvr {

class SociInventoryService final : public IInventoryService
{
public:
    explicit SociInventoryService(fourstory::db::SessionPool& pool);

    // Loads items from TITEMTABLE into the in-memory store for char_id.
    // Must be called before any other method for this char_id.
    void LoadCharItems(std::uint32_t              char_id,
                       std::vector<ItemInstance>  items) override;

    // Overload: loads directly from DB (ignores items argument).
    void LoadCharItemsFromDb(std::uint32_t char_id);

    void UnloadCharItems(std::uint32_t char_id) override;

    std::vector<const ItemInstance*>
        GetItems(std::uint32_t char_id, std::uint8_t inven_type) const override;

    const ItemInstance*
        FindItem(std::uint32_t char_id,
                 std::uint8_t  inven_type,
                 std::uint8_t  inven_id) const override;

    bool MoveItem(std::uint32_t char_id,
                  std::uint8_t  src_type, std::uint8_t src_slot,
                  std::uint8_t  dst_type, std::uint8_t dst_slot,
                  std::uint8_t  count) override;

    std::optional<ItemInstance>
        RemoveItem(std::uint32_t char_id,
                   std::uint8_t  inven_type,
                   std::uint8_t  inven_id) override;

    bool AddItem(std::uint32_t char_id, ItemInstance item) override;

private:
    fourstory::db::SessionPool&  m_pool;
    FakeInventoryService         m_cache;  // in-memory write-through cache
};

} // namespace tmapsvr
