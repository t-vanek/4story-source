// Item handlers — F5 Part 1.
//
// SerializeItem and the item ACK helpers implement the wire-faithful
// CTItem::WrapPacketClient format so item data sent to clients renders
// correctly. Handlers (OnMoveItemReq, OnItemUseReq) manipulate the
// in-memory IInventoryService and broadcast results to AOI.
//
// Legacy references:
//   TItem.cpp:502     — CTItem::WrapPacketClient
//   CSSender.cpp:1420 — SendCS_MOVEITEM_ACK
//   CSSender.cpp:1443 — SendCS_ADDITEM_ACK
//   CSSender.cpp:1456 — SendCS_DELITEM_ACK
//   CSSender.cpp:1468 — SendCS_EQUIP_ACK
//   CSSender.cpp:3798 — SendCS_ITEMUSE_ACK
//   CSHandler.cpp:1782 — OnCS_MOVEITEM_REQ
//   CSHandler.cpp:9092 — OnCS_ITEMUSE_REQ

#include "handlers_items.h"
#include "handlers.h"
#include "handlers_combat.h"  // SendHpMpAck
#include "loot_registry.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

namespace tmapsvr {

using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

// ---------------------------------------------------------------------------
// SerializeItem (CTItem::WrapPacketClient)
// ---------------------------------------------------------------------------
//
// Simplified F5 version — omits companion / routing fields that require
// additional chart tables. Each stub carries a PENDING comment with the
// legacy field name so F5 Part 2 can fill it in.

void SerializeItem(std::vector<std::byte>& out,
                   const ItemInstance&     item,
                   bool                    add_item_id)
{
    if (add_item_id)
        wire::WritePOD<std::uint8_t>(out, item.inven_id);

    wire::WritePOD<std::uint16_t>(out, item.item_id);
    wire::WritePOD<std::uint8_t> (out, item.level);
    wire::WritePOD<std::uint8_t> (out, item.gem);
    wire::WritePOD<std::uint16_t>(out, 0u);  // m_wMoggItemID    (companion, F5b)
    wire::WritePOD<std::uint16_t>(out, 0u);  // companion_value  (F5b)
    wire::WritePOD<std::uint8_t> (out, item.count);
    wire::WritePOD<std::uint32_t>(out, item.dura_max);
    wire::WritePOD<std::uint32_t>(out, item.dura_cur);
    wire::WritePOD<std::uint8_t> (out, 0u);  // m_bRefineMax (from template, F5b)
    wire::WritePOD<std::uint8_t> (out, item.refine_cur);
    wire::WritePOD<std::uint8_t> (out, item.g_level);
    // F5 sends __time64_t (non-cash items — most items)
    wire::WritePOD<std::int64_t> (out, item.end_time);
    wire::WritePOD<std::uint8_t> (out, item.g_effect);
    wire::WritePOD<std::uint8_t> (out, item.eld);
    wire::WritePOD<std::uint8_t> (out, 0u);  // wrap status (F5b)
    wire::WritePOD<std::uint16_t>(out, 0u);  // color (F5b)
    wire::WritePOD<std::uint16_t>(out, 0u);  // custom_tex (F5b)
    wire::WritePOD<std::uint8_t> (out, 0u);  // reg_guild (F5b)

    // Magic attribute list
    const auto magic_count =
        static_cast<std::uint8_t>(item.magic.size());
    wire::WritePOD<std::uint8_t>(out, magic_count);
    for (const auto& m : item.magic)
    {
        wire::WritePOD<std::uint8_t> (out, m.id);
        wire::WritePOD<std::uint16_t>(out, m.value);
    }
}

// ---------------------------------------------------------------------------
// CS_MOVEITEM_ACK
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
SendMoveItemAck(std::shared_ptr<tnetlib::AsioSession> sess,
                std::uint8_t                          result)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t>(body, result);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_MOVEITEM_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_ADDITEM_ACK
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
SendAddItemAck(std::shared_ptr<tnetlib::AsioSession> sess,
               std::uint8_t                          inven_id,
               const ItemInstance&                  item)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t>(body, inven_id);
    SerializeItem(body, item, true);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_ADDITEM_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_DELITEM_ACK
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
SendDelItemAck(std::shared_ptr<tnetlib::AsioSession> sess,
               std::uint8_t                          inven_type,
               std::uint8_t                          inven_id)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t>(body, inven_type);
    wire::WritePOD<std::uint8_t>(body, inven_id);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_DELITEM_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_EQUIP_ACK — broadcast equipped items (char_id + count + items)
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
SendEquipAck(std::shared_ptr<tnetlib::AsioSession>      sess,
             std::uint32_t                               char_id,
             const std::vector<const ItemInstance*>&    equip_items)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, char_id);
    const auto count = static_cast<std::uint8_t>(equip_items.size());
    wire::WritePOD<std::uint8_t>(body, count);
    for (const auto* item : equip_items)
        if (item) SerializeItem(body, *item, true);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_EQUIP_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_ITEMUSE_ACK
// ---------------------------------------------------------------------------

boost::asio::awaitable<void>
SendItemUseAck(std::shared_ptr<tnetlib::AsioSession> sess,
               std::uint8_t                          result,
               std::uint16_t                         delay_group_id,
               std::uint8_t                          item_kind,
               std::uint32_t                         expire_tick)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t> (body, result);
    wire::WritePOD<std::uint16_t>(body, delay_group_id);
    wire::WritePOD<std::uint8_t> (body, item_kind);
    wire::WritePOD<std::uint32_t>(body, expire_tick);
    co_await sess->SendPacket(
        ToUint16(MessageId::CS_ITEMUSE_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_MOVEITEM_REQ handler
// ---------------------------------------------------------------------------
//
// Wire: BYTE bInvenSRC, BYTE bSRCItemID, BYTE bInvenDEST, BYTE bDESTItemID,
//       BYTE bCount
// Source: CSHandler.cpp:1782
//
// F5 Part 1:
//   §1 malformed body → drop
//   §2 not connected / dead → drop
//   §3 item not found → CS_MOVEITEM_ACK(NoSrcItem)
//   §4 equip (dst_type==INVEN_EQUIP) → move + CS_EQUIP_ACK to AOI
//   §5 inventory move → CS_MOVEITEM_ACK + CS_DELITEM_ACK + CS_ADDITEM_ACK

boost::asio::awaitable<void>
OnMoveItemReq(std::shared_ptr<tnetlib::AsioSession> sess,
              MapSessionState&                     state,
              const tnetlib::DecodedPacket&        packet,
              const HandlerContext&                ctx)
{
    if (!state.connected || !state.snapshot || state.is_dead) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint8_t src_type = 0, src_slot = 0, dst_type = 0, dst_slot = 0, count = 0;
    if (!r.Read(src_type) || !r.Read(src_slot) ||
        !r.Read(dst_type) || !r.Read(dst_slot) || !r.Read(count))
    {
        spdlog::warn("CS_MOVEITEM_REQ malformed uid={}", state.user_id);
        co_return;
    }

    if (!ctx.inventory_svc)
    {
        co_await SendMoveItemAck(sess, MoveItemResult::NoSrcItem);
        co_return;
    }

    // §3 item not found
    const auto* src_item =
        ctx.inventory_svc->FindItem(state.char_id, src_type, src_slot);
    if (!src_item)
    {
        co_await SendMoveItemAck(sess, MoveItemResult::NoSrcItem);
        co_return;
    }

    // Snapshot before move (for ACK data)
    const ItemInstance moved_item = *src_item;

    // Perform the move
    const bool ok = ctx.inventory_svc->MoveItem(
        state.char_id, src_type, src_slot, dst_type, dst_slot, count);
    if (!ok)
    {
        co_await SendMoveItemAck(sess, MoveItemResult::Dealing);
        co_return;
    }

    co_await SendMoveItemAck(sess, MoveItemResult::Success);
    co_await SendDelItemAck(sess, src_type, src_slot);

    // Re-fetch to get updated inven_id
    ItemInstance sent = moved_item;
    sent.inven_type = dst_type;
    sent.inven_id   = dst_slot;
    co_await SendAddItemAck(sess, dst_type, sent);

    // §4 Equip change: broadcast CS_EQUIP_ACK to AOI
    if (dst_type == InvenType::Equip || src_type == InvenType::Equip)
    {
        if (ctx.map_state && ctx.session_registry)
        {
            const auto equip_ptrs = ctx.inventory_svc->GetItems(
                state.char_id, InvenType::Equip);
            const auto aoi = ctx.map_state->GetNeighborIds(
                state.snapshot->position.pos_x,
                state.snapshot->position.pos_z);
            for (std::uint32_t pid : aoi)
            {
                auto nbr = ctx.session_registry->Get(pid);
                if (nbr)
                    co_await SendEquipAck(nbr, state.char_id, equip_ptrs);
            }
        }
    }

    spdlog::debug("CS_MOVEITEM_REQ uid={} item={} {}:{} → {}:{}",
        state.user_id, moved_item.item_id,
        src_type, src_slot, dst_type, dst_slot);
}

// ---------------------------------------------------------------------------
// CS_ITEMUSE_REQ handler
// ---------------------------------------------------------------------------
//
// Wire: WORD wTempItem, BYTE bInven, BYTE bItem, WORD wDelayGroupID,
//       BYTE bCount, [DWORD+BYTE per target]
// Source: CSHandler.cpp:9092
//
// F5 Part 1:
//   §1 malformed body → drop
//   §2 not connected / dead → drop
//   §3 item not found → CS_ITEMUSE_ACK(NotFound)
//   §4 HP potion → heal HP + CS_HPMP_ACK broadcast + CS_ITEMUSE_ACK
//   §5 other items → CS_ITEMUSE_ACK(Success) + consume item
//
// Potion detection: item_id ranges 1-99 are HP/MP potions in the
// legacy TITEMCHART. F5 Part 1 uses use_value field (from CS_CHARINFO_ACK
// we only have item_id — actual classification requires TITEMCHART lookup).
// Stub: all used items are treated as HP potions healing level×50.

boost::asio::awaitable<void>
OnItemUseReq(std::shared_ptr<tnetlib::AsioSession> sess,
             MapSessionState&                     state,
             const tnetlib::DecodedPacket&        packet,
             const HandlerContext&                ctx)
{
    if (!state.connected || !state.snapshot || state.is_dead) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint16_t temp_item = 0;
    std::uint8_t  inven = 0, item_slot = 0;
    std::uint16_t delay_group_id = 0;
    std::uint8_t  count = 0;
    if (!r.Read(temp_item) || !r.Read(inven) || !r.Read(item_slot) ||
        !r.Read(delay_group_id) || !r.Read(count))
    {
        spdlog::warn("CS_ITEMUSE_REQ malformed uid={}", state.user_id);
        co_return;
    }

    // §3 item not found
    if (!ctx.inventory_svc)
    {
        co_await SendItemUseAck(sess, ItemUseResult::NotFound);
        co_return;
    }

    const auto* src = ctx.inventory_svc->FindItem(
        state.char_id, inven, item_slot);
    if (!src)
    {
        co_await SendItemUseAck(sess, ItemUseResult::NotFound);
        co_return;
    }

    spdlog::debug("CS_ITEMUSE_REQ uid={} item_id={} slot={}:{}",
        state.user_id, src->item_id, inven, item_slot);

    // §4 HP heal stub (all consumables heal HP)
    const std::uint32_t heal_amount =
        static_cast<std::uint32_t>(state.snapshot->level) * 50 + 200;

    if (ctx.player_hp)
    {
        const std::uint32_t new_hp =
            ctx.player_hp->ApplyHpDelta(state.char_id,
                static_cast<std::int64_t>(heal_amount));
        const auto* v = ctx.player_hp->Get(state.char_id);
        if (v)
        {
            constexpr std::uint8_t OT_PC = 1;
            co_await SendHpMpAck(sess, state.char_id, OT_PC,
                v->max_hp, new_hp, v->max_mp, v->mp);

            // Broadcast to AOI
            if (ctx.map_state && ctx.session_registry && state.snapshot)
            {
                const auto aoi = ctx.map_state->GetNeighborIds(
                    state.snapshot->position.pos_x,
                    state.snapshot->position.pos_z);
                for (std::uint32_t pid : aoi)
                {
                    auto nbr = ctx.session_registry->Get(pid);
                    if (nbr)
                        co_await SendHpMpAck(nbr, state.char_id, OT_PC,
                            v->max_hp, new_hp, v->max_mp, v->mp);
                }
            }
        }
    }

    // Consume item (remove 1 from stack, or remove if count becomes 0)
    ctx.inventory_svc->RemoveItem(state.char_id, inven, item_slot);
    co_await SendDelItemAck(sess, inven, item_slot);

    co_await SendItemUseAck(sess, ItemUseResult::Success,
        delay_group_id, 0, 0);
}

// ---------------------------------------------------------------------------
// SendMonItemListAck — show loot list to client
// ---------------------------------------------------------------------------
//
// Wire: BYTE bRet, BYTE bUpdate, DWORD dwMonID,
//       DWORD gold, DWORD silver, DWORD copper,
//       BYTE bItemCount, [SerializeItem × count]
// Source: CSSender.cpp:2991

static boost::asio::awaitable<void>
SendMonItemListAck(std::shared_ptr<tnetlib::AsioSession> sess,
                   std::uint32_t                         mon_id,
                   const MonsterLoot&                   loot)
{
    std::vector<std::byte> body;
    wire::WritePOD<std::uint8_t> (body, 0u);   // bRet = 0 (success)
    wire::WritePOD<std::uint8_t> (body, 1u);   // bUpdate
    wire::WritePOD<std::uint32_t>(body, mon_id);
    wire::WritePOD<std::uint32_t>(body, loot.gold);
    wire::WritePOD<std::uint32_t>(body, loot.silver);
    wire::WritePOD<std::uint32_t>(body, loot.copper);
    const auto cnt = static_cast<std::uint8_t>(loot.items.size());
    wire::WritePOD<std::uint8_t>(body, cnt);
    for (const auto& item : loot.items)
        SerializeItem(body, item, true);

    co_await sess->SendPacket(
        ToUint16(MessageId::CS_MONITEMLIST_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// OnMonItemTakeReq — pick up one item from monster loot
// ---------------------------------------------------------------------------
//
// Wire: DWORD dwMonID, BYTE bItemID, BYTE bInvenID, BYTE bSlotID
// Source: CSHandler.cpp:6874

boost::asio::awaitable<void>
OnMonItemTakeReq(std::shared_ptr<tnetlib::AsioSession> sess,
                 MapSessionState&                     state,
                 const tnetlib::DecodedPacket&        packet,
                 const HandlerContext&                ctx)
{
    if (!state.connected || !state.snapshot || state.is_dead) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint32_t mon_id  = 0;
    std::uint8_t  item_id = 0, inven_id = 0, slot_id = 0;
    if (!r.Read(mon_id) || !r.Read(item_id) ||
        !r.Read(inven_id) || !r.Read(slot_id))
    {
        spdlog::warn("CS_MONITEMTAKE_REQ malformed uid={}", state.user_id);
        co_return;
    }

    // Result: 1 byte success/fail
    // CS_MONITEMTAKE_ACK wire: BYTE bResult
    auto SendTakeAck = [&](std::uint8_t result) -> boost::asio::awaitable<void> {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t>(b, result);
        co_await sess->SendPacket(
            ToUint16(MessageId::CS_MONITEMTAKE_ACK),
            std::span<const std::byte>(b.data(), b.size()));
    };

    if (!ctx.loot_registry)
    {
        co_await SendTakeAck(1u);  // fail
        co_return;
    }

    auto taken = ctx.loot_registry->TakeItem(mon_id, item_id);
    if (!taken)
    {
        co_await SendTakeAck(1u);  // item already taken
        co_return;
    }

    // Add item to player inventory
    if (ctx.inventory_svc)
    {
        taken->inven_type = inven_id;
        taken->inven_id   = slot_id;
        ctx.inventory_svc->AddItem(state.char_id, *taken);
        co_await SendAddItemAck(sess, inven_id, *taken);
    }

    co_await SendTakeAck(0u);  // success
    spdlog::debug("OnMonItemTakeReq: uid={} took item_id={} from mon={}",
        state.user_id, taken->item_id, mon_id);
}

} // namespace tmapsvr
