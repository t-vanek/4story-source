// F4 — event manager CRUD + push-once forwarders.
//
// Core CRUD:
//   CT_EVENTCHANGE_REQ — add / update / delete with overlap check;
//                        persists via IEventRepository.
//   CT_EVENTDEL_REQ    — wraps the EK_DEL branch of the above.
//   CT_EVENTLIST_REQ   — snapshots EventRegistry.
//   CT_EVENTUPDATE_REQ — push current state of one event to peers
//                        (operator-initiated; the scheduler does
//                        the same on state transitions).
//   CT_EVENTMSG_REQ    — push start / end announcement to peers.
//   CT_CASHITEMSALE_REQ— operator-initiated cash-shop sale push.
//   CT_CASHSHOPSTOP_REQ— operator-initiated cash-shop stop.
//   CT_CASHITEMLIST_REQ— reads TCASHSHOPITEMCHART.

#include "handlers.h"

#include "../event_codec.h"
#include "../senders.h"
#include "../wire_codec.h"
#include "../services/authority_gate.h"
#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <chrono>

namespace tcontrolsvr::handlers {

namespace {

// Build the "value blob" (legacy szValue) for the SP call. F4
// preserves the legacy three-field shape just enough to satisfy
// the SP — full string encoding for cash items / mon-regens /
// lotteries is intentionally lossy (the GM editor re-supplies
// the structured data on every update).
std::string BuildValueBlob(const EventInfo& ev)
{
    std::string out;
    if (ev.kind == event_kind::kCashSale)
    {
        for (const auto& s : ev.cash_items)
        {
            out += std::to_string(s.item_id);
            out += "-";
            out += std::to_string(static_cast<int>(s.sale_value));
            out += ";";
        }
    }
    return out;
}

// Peers targeted by an event's (group, type, server_id) selectors.
std::vector<std::shared_ptr<PeerSession>>
PeersForEvent(const EventInfo& ev, PeerRegistry& peers)
{
    std::vector<std::shared_ptr<PeerSession>> out;
    for (const auto& svc : peers.Services())
    {
        if (svc.type_id != ev.server_type) continue;
        if (ev.group_id  != 0 && svc.group_id  != ev.group_id)  continue;
        if (ev.server_id != 0 && svc.server_id != ev.server_id) continue;
        auto conn = peers.Connection(svc.service_id);
        if (conn && conn->Wire() && conn->Wire()->IsOpen())
            out.push_back(std::move(conn));
    }
    return out;
}

} // namespace

boost::asio::awaitable<void>
OnEventListReq(std::shared_ptr<OperatorSession> op,
               std::vector<std::byte> /*body*/,
               const HandlerContext& ctx)
{
    if (!op->LoggedIn() || !HasAuthority(*op, OperatorRole::User))
    {
        if (ctx.audit)
            ctx.audit->LogAuthorityDenied(op->UserId(),
                op->AuthorityRaw(), "event_list");
        co_await senders::SendAuthorityAck(op->Wire());
        co_return;
    }
    if (!ctx.events)
    {
        co_await senders::SendEventListAck(op->Wire(), {});
        co_return;
    }
    co_await senders::SendEventListAck(op->Wire(), ctx.events->Snapshot());
}

boost::asio::awaitable<void>
OnEventChangeReq(std::shared_ptr<OperatorSession> op,
                 std::vector<std::byte> body,
                 const HandlerContext& ctx)
{
    if (!op->LoggedIn() || !HasAuthority(*op, OperatorRole::GMLevel3))
    {
        if (ctx.audit)
            ctx.audit->LogAuthorityDenied(op->UserId(),
                op->AuthorityRaw(), "event_change");
        co_await senders::SendAuthorityAck(op->Wire());
        co_return;
    }

    wire::Reader r(body);
    std::uint8_t op_byte = 0;
    if (!r.Read(op_byte))
    {
        spdlog::warn("CT_EVENTCHANGE_REQ malformed (no op byte)");
        co_return;
    }
    EventInfo ev{};
    if (!event_codec::Read(r, ev))
    {
        spdlog::warn("CT_EVENTCHANGE_REQ malformed (EventInfo body)");
        co_return;
    }

    if (!ctx.events) co_return;

    // EK_DEL on a missing index is success-as-noop (legacy).
    if (op_byte == event_op::kDel && !ctx.events->Find(ev.index))
    {
        co_await senders::SendEventChangeAck(op->Wire(),
            event_result::kSuccess, op_byte, ev);
        co_return;
    }

    // Time-range invariants for non-delete ops.
    if (op_byte != event_op::kDel)
    {
        const auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (ev.end_unix <= now || ev.end_unix <= ev.start_unix)
        {
            co_await senders::SendEventChangeAck(op->Wire(),
                event_result::kInvalidTime, op_byte, ev);
            co_return;
        }
        const std::uint32_t skip_idx =
            (op_byte == event_op::kUpdate) ? ev.index : 0;
        if (ctx.events->OverlapsExisting(ev, skip_idx))
        {
            co_await senders::SendEventChangeAck(op->Wire(),
                event_result::kInvalidTime, op_byte, ev);
            co_return;
        }
    }

    if (op_byte == event_op::kAdd && ev.index == 0)
        ev.index = ctx.events->NextIndex();

    std::uint8_t rc = event_result::kFail;
    if (ctx.event_repo)
        rc = ctx.event_repo->Persist(ev, op_byte, BuildValueBlob(ev));
    else
        rc = event_result::kSuccess;  // no-repo deploys still mutate memory

    co_await senders::SendEventChangeAck(op->Wire(), rc, op_byte, ev);
    if (rc != event_result::kSuccess) co_return;

    // Legacy: if the event is currently running and the SP succeeded,
    // re-push the new state to peers immediately (skipping lottery /
    // gifttime which fire-and-delete).
    const auto* existing = ctx.events->Find(ev.index);
    const bool was_running = existing && existing->state == 1;
    if (was_running &&
        ev.kind != event_kind::kLottery &&
        ev.kind != event_kind::kGiftTime &&
        ctx.peers)
    {
        for (const auto& peer : PeersForEvent(ev, *ctx.peers))
        {
            if (ev.kind == event_kind::kCashSale)
                co_await senders::SendCashItemSaleReq(peer->Wire(),
                    ev.index, 0, ev.cash_items);
            else
                co_await senders::SendEventUpdateReq(peer->Wire(),
                    ev.kind, 0, ev);
        }
    }

    if (op_byte == event_op::kDel) ctx.events->Erase(ev.index);
    else                           ctx.events->Upsert(ev);
}

boost::asio::awaitable<void>
OnEventDelReq(std::shared_ptr<OperatorSession> op,
              std::vector<std::byte> body,
              const HandlerContext& ctx)
{
    if (!op->LoggedIn() || !HasAuthority(*op, OperatorRole::GMLevel3))
    {
        if (ctx.audit)
            ctx.audit->LogAuthorityDenied(op->UserId(),
                op->AuthorityRaw(), "event_del");
        co_await senders::SendAuthorityAck(op->Wire());
        co_return;
    }

    wire::Reader r(body);
    std::uint32_t index = 0;
    if (!r.Read(index)) co_return;
    if (!ctx.events) co_return;
    auto* ev = ctx.events->Find(index);
    if (!ev) co_return;

    EventInfo del = *ev;  // copy for repo call (Persist needs the row)
    std::uint8_t rc = event_result::kFail;
    if (ctx.event_repo)
        rc = ctx.event_repo->Persist(del, event_op::kDel, "");
    else
        rc = event_result::kSuccess;
    if (rc == event_result::kSuccess) ctx.events->Erase(index);
}

boost::asio::awaitable<void>
OnEventUpdateReq(std::shared_ptr<OperatorSession> op,
                 std::vector<std::byte> body,
                 const HandlerContext& ctx)
{
    if (!op->LoggedIn()) co_return;
    wire::Reader r(body);
    std::uint32_t index = 0;
    std::uint16_t value = 0;
    if (!r.Read(index) || !r.Read(value)) co_return;
    if (!ctx.events || !ctx.peers) co_return;
    const auto* ev = ctx.events->Find(index);
    if (!ev) co_return;
    for (const auto& peer : PeersForEvent(*ev, *ctx.peers))
        co_await senders::SendEventUpdateReq(peer->Wire(),
            ev->kind, value, *ev);
}

boost::asio::awaitable<void>
OnEventMsgReq(std::shared_ptr<OperatorSession> op,
              std::vector<std::byte> body,
              const HandlerContext& ctx)
{
    if (!op->LoggedIn()) co_return;
    wire::Reader r(body);
    std::uint32_t index = 0;
    std::uint8_t  msg_type = 0;
    if (!r.Read(index) || !r.Read(msg_type)) co_return;
    if (!ctx.events || !ctx.peers) co_return;
    const auto* ev = ctx.events->Find(index);
    if (!ev) co_return;
    const std::string& msg =
        (msg_type == 0) ? ev->start_msg : ev->end_msg;
    for (const auto& peer : PeersForEvent(*ev, *ctx.peers))
        co_await senders::SendEventMsgReq(peer->Wire(),
            ev->kind, msg_type, msg);
}

boost::asio::awaitable<void>
OnCashItemSaleReq(std::shared_ptr<OperatorSession> op,
                  std::vector<std::byte> body,
                  const HandlerContext& ctx)
{
    if (!op->LoggedIn()) co_return;
    wire::Reader r(body);
    std::uint32_t index = 0;
    std::uint16_t value = 0;
    if (!r.Read(index) || !r.Read(value)) co_return;
    if (value > 100) value = 100;
    if (!ctx.events || !ctx.peers) co_return;
    const auto* ev = ctx.events->Find(index);
    if (!ev) co_return;
    for (const auto& peer : PeersForEvent(*ev, *ctx.peers))
        co_await senders::SendCashItemSaleReq(peer->Wire(),
            index, value, ev->cash_items);
}

boost::asio::awaitable<void>
OnCashShopStopReq(std::shared_ptr<OperatorSession> op,
                  std::vector<std::byte> body,
                  const HandlerContext& ctx)
{
    if (!op->LoggedIn()) co_return;
    wire::Reader r(body);
    std::uint32_t index = 0;
    if (!r.Read(index)) co_return;
    if (!ctx.events || !ctx.peers) co_return;
    const auto* ev = ctx.events->Find(index);
    if (!ev) co_return;
    for (const auto& peer : PeersForEvent(*ev, *ctx.peers))
        co_await senders::SendCashShopStopReq(peer->Wire(), 1);
}

boost::asio::awaitable<void>
OnCashItemListReq(std::shared_ptr<OperatorSession> op,
                  std::vector<std::byte> /*body*/,
                  const HandlerContext& ctx)
{
    if (!op->LoggedIn()) co_return;
    std::vector<CashItem> items;
    if (ctx.event_repo) items = ctx.event_repo->ListCashItems();
    co_await senders::SendCashItemListAck(op->Wire(), items);
}

boost::asio::awaitable<void>
ForwardRawToType(std::shared_ptr<OperatorSession> op,
                 std::uint16_t wId,
                 std::vector<std::byte> body,
                 std::uint8_t target_type,
                 bool single_target,
                 const HandlerContext& ctx)
{
    if (!op->LoggedIn() || !ctx.peers) co_return;
    for (const auto& svc : ctx.peers->Services())
    {
        if (svc.type_id != target_type) continue;
        auto conn = ctx.peers->Connection(svc.service_id);
        if (!conn || !conn->Wire() || !conn->Wire()->IsOpen()) continue;
        co_await senders::SendRawForward(conn->Wire(), wId, body);
        if (single_target) break;
    }
}

} // namespace tcontrolsvr::handlers
