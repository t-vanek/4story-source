#pragma once

// Item handlers and wire helpers.
//
// F5 Part 1 scope:
//   * SerializeItem      — CTItem::WrapPacketClient equivalent
//   * SendMoveItemAck    — CS_MOVEITEM_ACK (1-byte result code)
//   * SendAddItemAck     — CS_ADDITEM_ACK  (inven_id + item data)
//   * SendDelItemAck     — CS_DELITEM_ACK  (inven_id + slot)
//   * SendEquipAck       — CS_EQUIP_ACK    (broadcast on equip change)
//   * SendItemUseAck     — CS_ITEMUSE_ACK  (use result)
//   * OnMoveItemReq      — CS_MOVEITEM_REQ handler
//   * OnItemUseReq       — CS_ITEMUSE_REQ handler
//
// Wire format references:
//   CS_MOVEITEM_ACK  — CSSender.cpp:1420
//   CS_ADDITEM_ACK   — CSSender.cpp:1443
//   CS_DELITEM_ACK   — CSSender.cpp:1456
//   CS_EQUIP_ACK     — CSSender.cpp:1468
//   CS_ITEMUSE_ACK   — CSSender.cpp:3798
//   CTItem::WrapPacketClient — TItem.cpp:502

#include "asio_session.h"
#include "inventory_service.h"

#include <boost/asio/awaitable.hpp>
#include <cstdint>
#include <memory>
#include <vector>

namespace tmapsvr {

// Move-item result codes (legacy MI_* enum)
namespace MoveItemResult {
    constexpr std::uint8_t Success   = 0;
    constexpr std::uint8_t Dealing   = 1;
    constexpr std::uint8_t InvenFull = 2;
    constexpr std::uint8_t NoSrcItem = 3;
}

// Item-use result codes (legacy IU_* enum)
namespace ItemUseResult {
    constexpr std::uint8_t Success    = 0;
    constexpr std::uint8_t Dealing    = 1;
    constexpr std::uint8_t NotFound   = 2;
    constexpr std::uint8_t NeedLevel  = 3;
}

// Serialize one ItemInstance into `out` using the CTItem::WrapPacketClient
// wire layout. If `add_item_id` is true, the slot-id byte is prepended.
// Source: TItem.cpp:502 — WrapPacketClient
void SerializeItem(std::vector<std::byte>&  out,
                   const ItemInstance&      item,
                   bool                     add_item_id = true);

// Send CS_MOVEITEM_ACK — single result byte.
boost::asio::awaitable<void> SendMoveItemAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::uint8_t                          result);

// Send CS_ADDITEM_ACK — item added to player's inventory.
boost::asio::awaitable<void> SendAddItemAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::uint8_t                          inven_id,
    const ItemInstance&                  item);

// Send CS_DELITEM_ACK — item removed from player's inventory.
boost::asio::awaitable<void> SendDelItemAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::uint8_t                          inven_type,
    std::uint8_t                          inven_id);

// Send CS_EQUIP_ACK — broadcast full equipment to one receiver.
// Sends all items of type InvenType::Equip for the given char_id.
boost::asio::awaitable<void> SendEquipAck(
    std::shared_ptr<tnetlib::AsioSession>      sess,
    std::uint32_t                               char_id,
    const std::vector<const ItemInstance*>&    equip_items);

// Send CS_ITEMUSE_ACK — result + cooldown info.
boost::asio::awaitable<void> SendItemUseAck(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::uint8_t                          result,
    std::uint16_t                         delay_group_id = 0,
    std::uint8_t                          item_kind      = 0,
    std::uint32_t                         expire_tick    = 0);

// F5 Part 2: pick up item from monster loot.
// Source: CSHandler.cpp:6874.
boost::asio::awaitable<void> OnMonItemTakeReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    MapSessionState&                     state,
    const tnetlib::DecodedPacket&        packet,
    const HandlerContext&                ctx);

} // namespace tmapsvr
