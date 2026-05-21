#pragma once

// Inventory row — one slot from TINVENTABLE. Used by the F9
// inventory service + DM_LOADCHAR_ACK encoder.

#include <cstdint>

namespace tmapsvr {

struct InventoryRow
{
    std::uint8_t   bInvenID  = 0;  // slot id (1..N for tabs, 254 = EQUIP, 255 = tab marker)
    std::uint16_t  wItemID   = 0;  // item template id
    std::int64_t   dEndTime  = 0;  // expiry tick (0 = permanent)
    std::uint8_t   bELD      = 0;  // legacy "ease lord drop" flag
};

} // namespace tmapsvr
