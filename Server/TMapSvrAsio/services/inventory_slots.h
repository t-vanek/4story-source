#pragma once

// Inventory slot allocation — the faithful port of CTInven::GetBlankPos
// (TInven.cpp:47): the lowest free slot index in [0, capacity) not already
// occupied. The legacy capacity is the equipped bag item's
// m_pTITEM->m_bSlotCount; until the item-template (TITEM) chart lands we
// use a fixed, deliberately conservative default so the allocator
// FAILS SAFE — it declines a pickup (caller → MIT_FULLINVEN) rather than
// placing an item in a slot beyond the player's real bag (which the client
// wouldn't render). Real per-char capacity is a follow-up gated on TITEM.
//
// TINVENTABLE.bInvenID is a flat per-char slot (its PK with dwCharID); bag
// items occupy the low range, equipped gear / tab markers sit at 254 / 255
// (NetCode.h INVEN_DEFAULT/INVALID_SLOT == 0xFF), naturally outside the
// scanned range so they never collide with a freshly-allocated bag slot.

#include "domain/inventory.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace tmapsvr {

// Conservative default bag capacity (legacy base bag) — see the file note.
inline constexpr std::uint16_t kDefaultBagSlots = 24;

// Lowest free bag slot in [0, capacity) not present among `occupied`'s
// bInvenID values, or nullopt when the bag is full. Faithful to
// CTInven::GetBlankPos's "first gap" scan.
inline std::optional<std::uint8_t> FindBlankSlot(
    const std::vector<InventoryRow>& occupied,
    std::uint16_t capacity = kDefaultBagSlots)
{
    for (std::uint16_t slot = 0; slot < capacity; ++slot)
    {
        bool used = false;
        for (const auto& r : occupied)
            if (r.bInvenID == slot) { used = true; break; }
        if (!used)
            return static_cast<std::uint8_t>(slot);
    }
    return std::nullopt;
}

} // namespace tmapsvr
