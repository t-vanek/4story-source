#pragma once

// Inventory service — reads TINVENTABLE for a character and returns
// the list of inventory slot rows (bInvenID, wItemID, dEndTime, bELD).
// One row per slot: main bag tabs, equipped gear pseudo-slot, etc.
//
// Used by the F9 extension of OnDMLoadCharReq's success body: the
// snapshot from F8 is followed by a 16-bit row count and one record
// per inventory slot, in TINVENTABLE order (matches legacy SSHandler
// .cpp:3540).

#include "domain/inventory.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace tmapsvr {

class IInventoryService
{
public:
    virtual ~IInventoryService() = default;

    // Returns the inventory rows for `char_id` in TINVENTABLE row
    // order. Empty vector when the char has no rows or the DB query
    // fails (caller can't easily distinguish — both end up as
    // "client gets empty inventory", which is what the legacy
    // `m_bLoadCharError` branch in SSHandler.cpp:3567 did).
    virtual std::vector<InventoryRow>
        LoadInventory(std::uint32_t char_id) = 0;

    // Acquire an item into the char's bag (the loot / pickup path).
    // Allocates the lowest free bag slot (FindBlankSlot over the char's
    // current TINVENTABLE rows) and persists a row, returning the assigned
    // slot — or nullopt when the bag is full or the write fails (caller
    // replies MIT_FULLINVEN). Faithful to CTPlayer::PushTItem's
    // GetBlankPos + insert; only the 5 persisted columns are written
    // (dwCharID, bInvenID, wItemID, dEndTime, bELD), so durability /
    // refine / magic are template-derived on the next load, and the
    // looted item is treated as permanent (dEndTime NULL) — timed / cash
    // items are a follow-up. `it.wItemID` / `it.bELD` are the inputs.
    virtual std::optional<std::uint8_t>
        AddItem(std::uint32_t char_id, const ItemInstance& it) = 0;
};

} // namespace tmapsvr
