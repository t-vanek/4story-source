// NPC handlers — F6 Part 2.
//
// OnNpcTalkReq: simplest handler — echo NPCTALK_ACK with quest_id=0.
// OnNpcItemListReq: builds CS_NPCITEMLIST_ACK from INpcService data.
// OnItemBuyReq: validates gold, deducts, adds item, sends ACK.
//
// Legacy references:
//   CSHandler.cpp:3506 — OnCS_NPCTALK_REQ
//   CSHandler.cpp:6192 — OnCS_NPCITEMLIST_REQ
//   CSHandler.cpp:6247 — OnCS_ITEMBUY_REQ
//   CSSender.cpp:3824  — SendCS_NPCTALK_ACK
//   CSSender.cpp:2756  — SendCS_NPCITEMLIST_ACK
//   CSSender.cpp:2942  — SendCS_ITEMBUY_ACK

#include "handlers_npc.h"
#include "handlers.h"
#include "handlers_items.h"   // SendAddItemAck
#include "handlers_quest.h"   // SendQuestListPossibleAck
#include "npc_service.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

namespace tmapsvr {

using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

// ---------------------------------------------------------------------------
// CS_NPCTALK_REQ
// ---------------------------------------------------------------------------
//
// Wire body: WORD wNpcID
// ACK:       DWORD dwQuestID, WORD wNpcID
// Source: CSHandler.cpp:3506 / CSSender.cpp:3824

boost::asio::awaitable<void>
OnNpcTalkReq(std::shared_ptr<tnetlib::AsioSession> sess,
             MapSessionState&                     state,
             const tnetlib::DecodedPacket&        packet,
             const HandlerContext&                ctx)
{
    if (!state.connected || !state.snapshot) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint16_t npc_id = 0;
    if (!r.Read(npc_id))
    {
        spdlog::warn("CS_NPCTALK_REQ malformed uid={}", state.user_id);
        co_return;
    }

    spdlog::debug("CS_NPCTALK_REQ uid={} npc_id={}", state.user_id, npc_id);

    // ACK: dwQuestID=0 (quest matching is F7), wNpcID
    std::vector<std::byte> body;
    wire::WritePOD<std::uint32_t>(body, 0u);       // dwQuestID
    wire::WritePOD<std::uint16_t>(body, npc_id);

    co_await sess->SendPacket(
        ToUint16(MessageId::CS_NPCTALK_ACK),
        std::span<const std::byte>(body.data(), body.size()));

    // F7 Part 2: progress TALK quest terms
    if (ctx.quest_engine && state.snapshot)
    {
        const auto ev = ctx.quest_engine->OnNpcTalked(
            state.char_id, static_cast<std::uint32_t>(npc_id));
        co_await DispatchQuestEvents(sess, state, ctx, ev);
    }

    // F7: send available quests for this NPC
    if (ctx.quest_engine && ctx.npc_svc)
    {
        const auto* npc = ctx.npc_svc->GetNpc(npc_id);
        const std::uint8_t npc_country = npc ? npc->discount_rate : 0u;
        const auto available =
            ctx.quest_engine->Chart()->GetNpcQuests(npc_id);
        if (!available.empty())
            co_await SendQuestListPossibleAck(sess, npc_id,
                npc_country, available);
    }
}

// ---------------------------------------------------------------------------
// CS_NPCITEMLIST_REQ
// ---------------------------------------------------------------------------
//
// Wire body: WORD wNpcID
// ACK: WORD wNpcID, BYTE bType, BYTE bDiscountRate,
//      BYTE bCount, [WORD wItemID + DWORD dwPrice] × count
// Source: CSHandler.cpp:6192 / CSSender.cpp:2756

boost::asio::awaitable<void>
OnNpcItemListReq(std::shared_ptr<tnetlib::AsioSession> sess,
                 MapSessionState&                     state,
                 const tnetlib::DecodedPacket&        packet,
                 const HandlerContext&                ctx)
{
    if (!state.connected) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint16_t npc_id = 0;
    if (!r.Read(npc_id))
    {
        spdlog::warn("CS_NPCITEMLIST_REQ malformed uid={}", state.user_id);
        co_return;
    }

    std::vector<std::byte> body;
    wire::WritePOD<std::uint16_t>(body, npc_id);

    if (!ctx.npc_svc)
    {
        // No NPC service: empty shop
        wire::WritePOD<std::uint8_t>(body, NpcType::Item);
        wire::WritePOD<std::uint8_t>(body, 0u);  // discount
        wire::WritePOD<std::uint8_t>(body, 0u);  // count=0
        co_await sess->SendPacket(
            ToUint16(MessageId::CS_NPCITEMLIST_ACK),
            std::span<const std::byte>(body.data(), body.size()));
        co_return;
    }

    const auto* npc = ctx.npc_svc->GetNpc(npc_id);
    if (!npc)
    {
        spdlog::debug("CS_NPCITEMLIST_REQ: unknown npc_id={}", npc_id);
        wire::WritePOD<std::uint8_t>(body, NpcType::Item);
        wire::WritePOD<std::uint8_t>(body, 0u);
        wire::WritePOD<std::uint8_t>(body, 0u);
        co_await sess->SendPacket(
            ToUint16(MessageId::CS_NPCITEMLIST_ACK),
            std::span<const std::byte>(body.data(), body.size()));
        co_return;
    }

    wire::WritePOD<std::uint8_t>(body, npc->type);
    wire::WritePOD<std::uint8_t>(body, npc->discount_rate);

    const auto count = static_cast<std::uint8_t>(npc->shop_items.size());
    wire::WritePOD<std::uint8_t>(body, count);
    for (const auto& si : npc->shop_items)
    {
        wire::WritePOD<std::uint16_t>(body, si.item_id);
        wire::WritePOD<std::uint32_t>(body, si.price);
    }

    spdlog::debug("CS_NPCITEMLIST_REQ uid={} npc={} ({} items)",
        state.user_id, npc_id, count);

    co_await sess->SendPacket(
        ToUint16(MessageId::CS_NPCITEMLIST_ACK),
        std::span<const std::byte>(body.data(), body.size()));
}

// ---------------------------------------------------------------------------
// CS_ITEMBUY_REQ
// ---------------------------------------------------------------------------
//
// Wire body: WORD wNpcID, DWORD dwQuestID, WORD wItemID, BYTE bCount,
//            BYTE bNpcInvenID, BYTE bNpcItemID
// ACK: BYTE bRet, WORD wItemID, DWORD gold, DWORD silver, DWORD copper
// Source: CSHandler.cpp:6247 / CSSender.cpp:2942

boost::asio::awaitable<void>
OnItemBuyReq(std::shared_ptr<tnetlib::AsioSession> sess,
             MapSessionState&                     state,
             const tnetlib::DecodedPacket&        packet,
             const HandlerContext&                ctx)
{
    if (!state.connected || !state.snapshot) co_return;

    wire::Reader r(packet.body.data(), packet.body.size());
    std::uint16_t npc_id = 0;
    std::uint32_t quest_id = 0;
    std::uint16_t item_id = 0;
    std::uint8_t  count = 0, npc_inven_id = 0, npc_item_id = 0;

    if (!r.Read(npc_id) || !r.Read(quest_id) || !r.Read(item_id) ||
        !r.Read(count)  || !r.Read(npc_inven_id) || !r.Read(npc_item_id))
    {
        spdlog::warn("CS_ITEMBUY_REQ malformed uid={}", state.user_id);
        co_return;
    }

    // Helper lambda to send buy result ACK
    auto& snap = *state.snapshot;
    auto SendAck = [&](std::uint8_t result) -> boost::asio::awaitable<void> {
        std::vector<std::byte> b;
        wire::WritePOD<std::uint8_t> (b, result);
        wire::WritePOD<std::uint16_t>(b, item_id);
        wire::WritePOD<std::uint32_t>(b, snap.gold);
        wire::WritePOD<std::uint32_t>(b, snap.silver);
        wire::WritePOD<std::uint32_t>(b, snap.copper);
        co_await sess->SendPacket(
            ToUint16(MessageId::CS_ITEMBUY_ACK),
            std::span<const std::byte>(b.data(), b.size()));
    };

    if (!ctx.npc_svc)
    {
        co_await SendAck(ItemBuyResult::NpcCallError);
        co_return;
    }

    const auto shop_item = ctx.npc_svc->FindShopItem(npc_id, item_id);
    if (!shop_item)
    {
        co_await SendAck(ItemBuyResult::NotFound);
        co_return;
    }

    const std::uint64_t total_cost =
        static_cast<std::uint64_t>(shop_item->price) * count;
    const std::uint64_t total_gold =
        static_cast<std::uint64_t>(snap.gold) * 10000 +
        static_cast<std::uint64_t>(snap.silver) * 100 +
        static_cast<std::uint64_t>(snap.copper);

    if (total_gold < total_cost)
    {
        co_await SendAck(ItemBuyResult::NeedMoney);
        co_return;
    }

    // Deduct cost (all in copper)
    std::int64_t remainder = static_cast<std::int64_t>(total_gold) -
                             static_cast<std::int64_t>(total_cost);
    snap.gold   = static_cast<std::uint32_t>(remainder / 10000);
    snap.silver = static_cast<std::uint32_t>((remainder % 10000) / 100);
    snap.copper = static_cast<std::uint32_t>(remainder % 100);

    // Add item to inventory
    if (ctx.inventory_svc)
    {
        ItemInstance item{};
        item.item_id    = item_id;
        item.count      = count;
        item.inven_type = InvenType::Main;
        item.inven_id   = 0;  // let service assign slot
        ctx.inventory_svc->AddItem(state.char_id, item);
        co_await SendAddItemAck(sess, InvenType::Main, item);
    }

    co_await SendAck(ItemBuyResult::Success);

    spdlog::info("CS_ITEMBUY_REQ uid={} npc={} item={} ×{} cost={}",
        state.user_id, npc_id, item_id, count, total_cost);
}

} // namespace tmapsvr
