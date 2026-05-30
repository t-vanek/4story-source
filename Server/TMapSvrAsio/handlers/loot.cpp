// Monster corpse loot handlers — the client-driven loot window over a
// killed monster's corpse (services/corpse_registry.h, populated on death
// in handlers/combat.cpp). Faithful to the legacy CS_MONITEM* family
// (CSHandler.cpp:6771-6910 → TMapSvr::MonMoneyTake / MonItemTake):
//
//   CS_MONITEMLIST_REQ    open the loot window  → CS_MONITEMLIST_ACK
//   CS_MONMONEYTAKE_REQ   take the corpse money → CS_MONEY_ACK + refresh
//   CS_MONITEMTAKE_REQ    take one item → bag   → CS_GETITEM_ACK + refresh
//   CS_MONITEMTAKEALL_REQ take money + every item that fits
//
// Bounded, each documented at its site: the keeper / inven-lock / party
// (PT_HUNTER) access rules, the party item lottery, the client-chosen
// destination slot (we auto-allocate via IInventoryService::AddItem), and
// the in-session CS_ADDITEM_ACK placement packet (the looted item is
// persisted + surfaced via CS_GETITEM_ACK and shows in the bag on reload)
// are follow-ups. Acks go to the looting client only (private state).

#include "handlers.h"

#include "domain/character.h"
#include "domain/inventory.h"
#include "services/char_state_store.h"
#include "services/client_senders.h"
#include "services/corpse_registry.h"
#include "services/inventory_service.h"
#include "services/loot_take.h"
#include "services/money.h"
#include "services/session_registry.h"
#include "wire_codec.h"

#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace tmapsvr {

namespace {

// Resolve the looting char from the session, or 0 if unbound.
std::uint32_t LooterCharId(const HandlerContext& ctx,
                           tnetlib::AsioSession* sess)
{
    if (ctx.session_reg)
        if (const auto cid = ctx.session_reg->FindCharIdBySession(sess))
            return *cid;
    return 0;
}

// Send the current loot-window state for a corpse: MIL_SUCCESS + money +
// items when it exists, else MIL_CANTACCESS. Shared by the open request +
// the post-take refresh.
boost::asio::awaitable<void> SendCorpseList(
    std::shared_ptr<tnetlib::AsioSession> sess, const HandlerContext& ctx,
    std::uint32_t char_id, std::uint32_t mon_id)
{
    using tnetlib::protocol::MessageId;

    const auto corpse =
        ctx.corpse_registry ? ctx.corpse_registry->Find(mon_id) : std::nullopt;

    std::vector<std::byte> ack;
    if (!corpse)
    {
        ack = EncodeMonItemListAck(MIL_CANTACCESS, 0, mon_id, 0, 0, 0, {},
                                   char_id);
    }
    else
    {
        std::uint32_t g = 0, s = 0, c = 0;
        SplitMoney(corpse->dwMoney, g, s, c);
        ack = EncodeMonItemListAck(MIL_SUCCESS, 0, mon_id, g, s, c,
                                   corpse->items, char_id);
    }
    co_await sess->SendPacket(
        static_cast<std::uint16_t>(MessageId::CS_MONITEMLIST_ACK), ack);
}

// Move all of a corpse's money into the char's purse + tell the client.
// Shared by CS_MONMONEYTAKE and CS_MONITEMTAKEALL.
boost::asio::awaitable<void> TakeMoneyToPurse(
    std::shared_ptr<tnetlib::AsioSession> sess, const HandlerContext& ctx,
    std::uint32_t char_id, std::uint32_t mon_id)
{
    using tnetlib::protocol::MessageId;

    if (!ctx.corpse_registry)
        co_return;
    const auto money = ctx.corpse_registry->TakeMoney(mon_id);
    if (!money || !ctx.char_state)
        co_return;

    ctx.char_state->Update(char_id,
        [&](CharSnapshot& s) { AddMoneyToChar(s, money); });
    if (const auto s = ctx.char_state->Get(char_id))
    {
        const auto m = EncodeMoneyAck(s->dwGold, s->dwSilver, s->dwCooper);
        co_await sess->SendPacket(
            static_cast<std::uint16_t>(MessageId::CS_MONEY_ACK), m);
    }
    spdlog::info("loot: char={} took {} cooper off corpse mon={}",
        char_id, money, mon_id);
}

} // namespace

// CS_MONITEMLIST_REQ — open (or close) the loot window for a corpse.
boost::asio::awaitable<void> OnMonItemListReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx)
{
    // wire (CSHandler.cpp:6781): BYTE bWant, DWORD dwMonID
    wire::Reader r(body.data(), body.size());
    std::uint8_t  want = 0;
    std::uint32_t mon_id = 0;
    if (!r.Read(want) || !r.Read(mon_id))
    {
        spdlog::warn("CS_MONITEMLIST_REQ: short body ({} bytes)", body.size());
        co_return;
    }

    const auto cid = LooterCharId(ctx, sess.get());
    if (!cid)
        co_return;

    // bWant == 0 closes the window (legacy unlocks the corpse); nothing to
    // send. The keeper / inven-lock gating is a follow-up.
    if (!want)
        co_return;

    co_await SendCorpseList(std::move(sess), ctx, cid, mon_id);
}

// CS_MONMONEYTAKE_REQ — take the money off a corpse.
boost::asio::awaitable<void> OnMonMoneyTakeReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx)
{
    // wire (CSHandler.cpp:6866): DWORD dwMonID
    wire::Reader r(body.data(), body.size());
    std::uint32_t mon_id = 0;
    if (!r.Read(mon_id))
    {
        spdlog::warn("CS_MONMONEYTAKE_REQ: short body ({} bytes)", body.size());
        co_return;
    }

    const auto cid = LooterCharId(ctx, sess.get());
    if (!cid)
        co_return;

    co_await TakeMoneyToPurse(sess, ctx, cid, mon_id);

    // Reap an emptied corpse, then refresh the loot window.
    if (ctx.corpse_registry && ctx.corpse_registry->IsEmpty(mon_id))
        ctx.corpse_registry->Remove(mon_id);
    co_await SendCorpseList(std::move(sess), ctx, cid, mon_id);
}

// CS_MONITEMTAKE_REQ — take one item off a corpse into the bag.
boost::asio::awaitable<void> OnMonItemTakeReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;

    // wire (CSHandler.cpp:6885): DWORD dwMonID, BYTE bItemID (corpse slot),
    // BYTE bInvenID, BYTE bSlotID (client-chosen dest — we auto-allocate).
    wire::Reader r(body.data(), body.size());
    std::uint32_t mon_id = 0;
    std::uint8_t  item_slot = 0, dest_inven = 0, dest_slot = 0;
    if (!r.Read(mon_id) || !r.Read(item_slot) || !r.Read(dest_inven) ||
        !r.Read(dest_slot))
    {
        spdlog::warn("CS_MONITEMTAKE_REQ: short body ({} bytes)", body.size());
        co_return;
    }

    const auto cid = LooterCharId(ctx, sess.get());
    if (!cid || !ctx.corpse_registry || !ctx.inventory_service)
        co_return;

    const auto out = TakeCorpseItem(*ctx.corpse_registry,
        *ctx.inventory_service, cid, mon_id, item_slot);

    co_await sess->SendPacket(
        static_cast<std::uint16_t>(MessageId::CS_MONITEMTAKE_ACK),
        EncodeMonItemTakeAck(out.result));

    if (out.result == MIT_SUCCESS && out.taken)
    {
        // CS_GETITEM_ACK — the loot notification (descriptor, no slot),
        // exactly as legacy MonItemTake sends after the item is placed.
        const auto get = EncodeItemDescriptor(*out.taken, cid,
                                              /*add_item_id=*/false);
        co_await sess->SendPacket(
            static_cast<std::uint16_t>(MessageId::CS_GETITEM_ACK), get);
        spdlog::info("loot: char={} took item={} (corpse slot {}) off mon={} "
            "→ bag slot {}", cid, out.taken->wItemID, item_slot, mon_id,
            out.taken->bItemID);

        if (ctx.corpse_registry->IsEmpty(mon_id))
            ctx.corpse_registry->Remove(mon_id);
        co_await SendCorpseList(std::move(sess), ctx, cid, mon_id);
    }
}

// CS_MONITEMTAKEALL_REQ — take the money + every item that fits.
boost::asio::awaitable<void> OnMonItemTakeAllReq(
    std::shared_ptr<tnetlib::AsioSession> sess,
    std::vector<std::byte>                body,
    const HandlerContext&                 ctx)
{
    using tnetlib::protocol::MessageId;

    // wire (CSHandler.cpp:6903): DWORD dwMonID
    wire::Reader r(body.data(), body.size());
    std::uint32_t mon_id = 0;
    if (!r.Read(mon_id))
    {
        spdlog::warn("CS_MONITEMTAKEALL_REQ: short body ({} bytes)",
            body.size());
        co_return;
    }

    const auto cid = LooterCharId(ctx, sess.get());
    if (!cid)
        co_return;

    co_await TakeMoneyToPurse(sess, ctx, cid, mon_id);

    // Take items until the corpse empties or the bag fills (a failed take
    // leaves the remaining items on the corpse — no loss). TakeCorpseItem
    // removes each item it places, so re-Find shrinks the list each pass.
    if (ctx.corpse_registry && ctx.inventory_service)
    {
        while (true)
        {
            const auto corpse = ctx.corpse_registry->Find(mon_id);
            if (!corpse || corpse->items.empty())
                break;
            const std::uint8_t slot = corpse->items.front().bItemID;
            const auto out = TakeCorpseItem(*ctx.corpse_registry,
                *ctx.inventory_service, cid, mon_id, slot);
            if (out.result != MIT_SUCCESS || !out.taken)
                break;   // bag full / gone — stop, rest stays on the corpse
            const auto get = EncodeItemDescriptor(*out.taken, cid,
                                                  /*add_item_id=*/false);
            co_await sess->SendPacket(
                static_cast<std::uint16_t>(MessageId::CS_GETITEM_ACK), get);
        }
    }

    if (ctx.corpse_registry && ctx.corpse_registry->IsEmpty(mon_id))
        ctx.corpse_registry->Remove(mon_id);
    co_await SendCorpseList(std::move(sess), ctx, cid, mon_id);
}

} // namespace tmapsvr
