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
};

} // namespace tmapsvr
