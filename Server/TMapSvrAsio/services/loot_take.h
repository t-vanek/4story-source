#pragma once

// The corpse-item take decision — the pure orchestration the
// CS_MONITEMTAKE handler runs, factored out of the coroutine so its
// invariants are unit-testable without a socket. Moves one corpse item
// into the player's bag: peek it, try to add (slot-allocate + persist),
// and only on success remove it from the corpse — a full bag leaves the
// corpse intact (no item loss), faithful to TMapSvr::MonItemTake's
// CanPush / PushTItem guard.

#include "corpse_registry.h"
#include "inventory_service.h"
#include "domain/inventory.h"

#include <cstdint>
#include <optional>

namespace tmapsvr {

// MONITEMTAKE_RESULT (NetCode.h:510). Only the first three arise in this
// bounded path; AUTHORITY/DEALING/LOTTERY ride the keeper/trade/party waves.
enum MonItemTakeResult : std::uint8_t
{
    MIT_SUCCESS   = 0,
    MIT_FULLINVEN = 1,
    MIT_NOTFOUND  = 2,
    MIT_AUTHORITY = 3,
    MIT_DEALING   = 4,
    MIT_LOTTERY   = 5,
};

// MONITEMLIST_RESULT (NetCode.h:520).
enum MonItemListResult : std::uint8_t
{
    MIL_SUCCESS   = 0,
    MIL_CANTACCESS = 1,
};

struct TakeItemOutcome
{
    std::uint8_t                result = MIT_NOTFOUND;
    std::optional<ItemInstance> taken;   // set on MIT_SUCCESS (for the ack)
};

// Take the corpse item at `slot` into `char_id`'s bag. On success the
// returned `taken` carries the item with its assigned bag slot (bItemID).
inline TakeItemOutcome TakeCorpseItem(
    ICorpseRegistry& corpses, IInventoryService& inv,
    std::uint32_t char_id, std::uint32_t mon_id, std::uint8_t slot)
{
    const auto item = corpses.ItemAt(mon_id, slot);
    if (!item)
        return { MIT_NOTFOUND, std::nullopt };

    const auto dest = inv.AddItem(char_id, *item);
    if (!dest)
        return { MIT_FULLINVEN, std::nullopt };   // bag full → corpse intact

    ItemInstance taken = *item;
    taken.bItemID = *dest;                         // assigned bag slot
    corpses.RemoveItem(mon_id, slot);
    return { MIT_SUCCESS, taken };
}

} // namespace tmapsvr
