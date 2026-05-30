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

// ItemInstance — a live item the server holds and serializes to the
// client. Faithful to the fields CTItem::WrapPacketClient emits
// (TItem.cpp:502) plus the identity (dlID) + placement (bInvenID/bItemID)
// the runtime inventory store + the loot take path need. A freshly-rolled
// monster drop fills wItemID/bCount/bGLevel and leaves the rest at their
// defaults (durability / refine / magic come from the item-template +
// magic charts, which are deferred — see services/loot.h).
struct ItemInstance
{
    std::uint64_t  dlID         = 0;   // unique instance id (legacy GenItemID)
    std::uint8_t   bItemID      = 0;   // slot within its inventory tab
    std::uint8_t   bInvenID     = 0;   // which inventory (INVEN_DEFAULT = 0, …)
    std::uint16_t  wItemID      = 0;   // item template id
    std::uint8_t   bLevel       = 0;   // item level
    std::uint8_t   bGem         = 0;   // gem socket count
    std::uint16_t  wMoggItemID  = 0;   // appearance ("mogg") item
    std::uint16_t  wCompanion   = 0;   // IEV_COMPANION binding
    std::uint8_t   bCount       = 1;   // stack count
    std::uint32_t  dwDuraMax    = 0;   // durability max (0 = no durability)
    std::uint32_t  dwDuraCur    = 0;
    std::uint8_t   bRefineMax   = 0;   // refine cap (from item template — deferred)
    std::uint8_t   bRefineCur   = 0;
    std::uint8_t   bGLevel      = 0;   // grade level
    std::int64_t   dEndTime     = 0;   // expiry (0 = permanent)
    std::uint8_t   bGradeEffect = 0;
    std::uint8_t   bELD         = 0;   // IEV_ELD
    std::uint8_t   bWrap        = 0;   // IEV_WRAP (gift wrapping)
    std::uint16_t  wColor       = 0;   // IEV_COLOR dye
    std::uint16_t  wCustomTex   = 0;   // IEV_CUSTOMTEX
    std::uint32_t  dwGuildBound  = 0;  // IEV_GUILD — bRegGuild set when == viewer
    // Magic options (m_mapTMAGIC) are deferred — descriptor writes count 0.
};

} // namespace tmapsvr
