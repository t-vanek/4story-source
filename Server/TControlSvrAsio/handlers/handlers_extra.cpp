// Round-2 audit fix-ups for handlers missing from F1–F5:
//
//   Operator-side:
//     CT_ITEMFIND_REQ        — broadcast to every WorldSvr in group.
//     CT_ITEMSTATE_REQ       — broadcast to every WorldSvr in group.
//     CT_MONACTION_REQ       — broadcast to every MapSvr in group.
//     CT_PLATFORM_REQ        — fan-out of PDH counters to operators
//                              with MANAGER_SERVICE; wire kept, body
//                              forwarded with zeroed metrics on
//                              non-Windows (PDH replaced by /metrics).
//     CT_SERVICEDATACLEAR_REQ — reset per-peer counters + broadcast
//                              CT_SERVICEDATACLEAR_ACK to every peer.
//
//   Peer-side:
//     CT_SERVICECHANGE_REQ    — peer reports state transition; we
//                              fan SERVICECHANGE_ACK to operators +
//                              fire alerter on non-zero SMS type.
//     CT_ITEMFIND_ACK         — route back to operator (strips the
//                              middle manager_id).
//     CT_ITEMSTATE_ACK        — route back (strips head manager_id).
//     CT_MONSPAWNFIND_ACK     — route back (strips head manager_id).
//     CT_EVENTQUARTERLIST_ACK — route back.
//     CT_EVENTQUARTERUPDATE_ACK — route back.
//     CT_TOURNAMENTEVENT_ACK  — route back.
//     CT_RPSGAMEDATA_ACK      — route back.
//     CT_CMGIFT_ACK           — route back.
//     CT_CMGIFTLIST_ACK       — route back.
//     CT_CASHITEMSALE_ACK     — observer (no-op, just verifies index).

#include "handlers.h"

#include "../peer_session.h"
#include "../senders.h"
#include "../wire_codec.h"
#include "../services/authority_gate.h"
#include "../services/svr_type.h"

#include "fourstory/db/co_offload.h"
#include "MessageId.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstring>

namespace tcontrolsvr::handlers {

namespace {

using tnetlib::protocol::MessageId;
using tnetlib::protocol::ToUint16;

} // namespace

boost::asio::awaitable<void>
OnItemFindReq(std::shared_ptr<OperatorSession> op,
              std::vector<std::byte> body,
              const HandlerContext& ctx)
{
    if (!op->LoggedIn() || !HasAuthority(*op, OperatorRole::All))
    {
        if (ctx.audit)
            ctx.audit->LogAuthorityDenied(op->UserId(),
                op->AuthorityRaw(), "item_find");
        co_await senders::SendAuthorityAck(op->Wire());
        co_return;
    }
    wire::Reader r(body);
    std::uint16_t item_id = 0;
    std::string   user_name;
    std::uint8_t  world = 0;
    if (!r.Read(item_id) || !r.ReadString(user_name) || !r.Read(world))
    {
        spdlog::warn("CT_ITEMFIND_REQ malformed body");
        co_return;
    }
    if (ctx.audit)
        ctx.audit->LogAdminAction(op->UserId(), "item_find", user_name);
    if (!ctx.peers) co_return;
    for (const auto& peer : ctx.peers->FindByType(world, svr_type::kWorldSvr))
        co_await senders::SendItemFindReq(peer->Wire(),
            op->ManagerSeq(), item_id, user_name);
}

boost::asio::awaitable<void>
OnItemStateReq(std::shared_ptr<OperatorSession> op,
               std::vector<std::byte> body,
               const HandlerContext& ctx)
{
    if (!op->LoggedIn() || !HasAuthority(*op, OperatorRole::All))
    {
        if (ctx.audit)
            ctx.audit->LogAuthorityDenied(op->UserId(),
                op->AuthorityRaw(), "item_state");
        co_await senders::SendAuthorityAck(op->Wire());
        co_return;
    }
    wire::Reader r(body);
    std::uint8_t  world = 0;
    std::uint32_t manager_id = 0;
    std::uint16_t count = 0;
    if (!r.Read(world) || !r.Read(manager_id) || !r.Read(count))
    {
        spdlog::warn("CT_ITEMSTATE_REQ malformed body");
        co_return;
    }

    // Re-build the peer-side body: { DWORD manager_id, WORD count,
    // [WORD item_id, BYTE state] * count }. Strip the world filter
    // (the first byte) and repack — that matches the legacy
    // OnCT_ITEMSTATE_REQ pMSG construction in Handler.cpp:1350.
    std::vector<std::byte> fwd;
    wire::WritePOD<std::uint32_t>(fwd, manager_id);
    wire::WritePOD<std::uint16_t>(fwd, count);
    for (std::uint16_t i = 0; i < count; ++i)
    {
        std::uint16_t item_id = 0;
        std::uint8_t  state = 0;
        if (!r.Read(item_id) || !r.Read(state))
        {
            spdlog::warn("CT_ITEMSTATE_REQ truncated at item {}", i);
            co_return;
        }
        wire::WritePOD<std::uint16_t>(fwd, item_id);
        wire::WritePOD<std::uint8_t >(fwd, state);
    }
    if (ctx.audit)
        ctx.audit->LogAdminAction(op->UserId(), "item_state",
            "world=" + std::to_string(world));
    if (!ctx.peers) co_return;
    for (const auto& peer : ctx.peers->FindByType(world, svr_type::kWorldSvr))
        co_await senders::SendItemStateReq(peer->Wire(), fwd);
}

boost::asio::awaitable<void>
OnMonActionReq(std::shared_ptr<OperatorSession> op,
               std::vector<std::byte> body,
               const HandlerContext& ctx)
{
    // Legacy MONACTION_REQ has no authority gate — match the legacy
    // permissive behavior. Auditing still records the action.
    if (!op->LoggedIn()) co_return;

    wire::Reader r(body);
    std::uint8_t  group = 0, channel = 0, action = 0, rh_type = 0;
    std::uint16_t map_id = 0, spawn_id = 0;
    std::uint32_t mon_id = 0, trigger_id = 0, host_id = 0, rh_id = 0;
    if (!r.Read(group)      || !r.Read(channel)    ||
        !r.Read(map_id)     || !r.Read(mon_id)     ||
        !r.Read(action)     || !r.Read(trigger_id) ||
        !r.Read(host_id)    || !r.Read(rh_id)      ||
        !r.Read(rh_type)    || !r.Read(spawn_id))
    {
        spdlog::warn("CT_MONACTION_REQ malformed body");
        co_return;
    }
    if (ctx.audit)
        ctx.audit->LogAdminAction(op->UserId(), "mon_action",
            "map=" + std::to_string(map_id));
    if (!ctx.peers) co_return;
    for (const auto& peer : ctx.peers->FindByType(group, svr_type::kMapSvr))
        co_await senders::SendMonActionAck(peer->Wire(),
            channel, map_id, mon_id, action, trigger_id, host_id,
            rh_id, rh_type, spawn_id);
}

boost::asio::awaitable<void>
OnPlatformReq(std::shared_ptr<OperatorSession> /*op*/,
              std::vector<std::byte> body,
              const HandlerContext& ctx)
{
    // Legacy fan-out: PDH counter sample from a peer machine,
    // broadcast to every MANAGER_SERVICE operator. PDH itself is
    // intentionally not re-implemented on the Asio side (see
    // _rewrite/docs/CONTROL_SERVER_PORT_PLAN.md §2.3) — the wire
    // shape is preserved so the legacy GUI's tile renders, but the
    // body forwards whatever the peer claimed (zeros from the
    // disabled platform monitor on Linux).
    wire::Reader r(body);
    std::uint8_t  machine_id = 0;
    std::uint32_t cpu = 0, mem = 0;
    float         net = 0;
    if (!r.Read(machine_id) || !r.Read(cpu) ||
        !r.Read(mem)        || !r.Read(net))
    {
        spdlog::debug("CT_PLATFORM_REQ malformed body — dropping");
        co_return;
    }
    if (!ctx.operators) co_return;
    for (const auto& op : ctx.operators->SnapshotLoggedIn())
    {
        if (!op || !op->Wire() || !op->Wire()->IsOpen()) continue;
        if (!HasAuthority(*op, OperatorRole::Service)) continue;
        co_await senders::SendPlatformAck(op->Wire(),
            machine_id, cpu, mem, net);
    }
}

boost::asio::awaitable<void>
OnServiceDataClearReq(std::shared_ptr<OperatorSession> op,
                      std::vector<std::byte> /*body*/,
                      const HandlerContext& ctx)
{
    if (!op->LoggedIn()) co_return;

    if (ctx.peers)
    {
        // Reset every per-peer runtime counter, mirror legacy
        // OnCT_SERVICEDATACLEAR_REQ. Stamp the "latest stop" + peak
        // time with now so the GUI tiles don't read as never-seen.
        const auto now_unix = std::chrono::duration_cast<
            std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        for (const auto& svc : ctx.peers->Services())
        {
            auto* st = ctx.peers->Status(svc.service_id);
            if (!st) continue;
            st->max_users        = 0;
            st->stop_count       = 0;
            st->cur_users        = 0;
            st->latest_stop_unix = 0;
            st->peak_time_unix   = 0;
            auto conn = ctx.peers->Connection(svc.service_id);
            if (conn && conn->Wire() && conn->Wire()->IsOpen())
            {
                st->latest_stop_unix = now_unix;
                st->peak_time_unix   = now_unix;
                co_await senders::SendServiceDataClearAck(conn->Wire());
            }
        }
    }
    if (ctx.audit)
        ctx.audit->LogAdminAction(op->UserId(),
            "service_data_clear", "");
}

boost::asio::awaitable<void>
OnPeerServiceChangeReq(std::shared_ptr<PeerSession> /*peer*/,
                       std::vector<std::byte> body,
                       const HandlerContext& ctx)
{
    wire::Reader r(body);
    std::uint32_t service_id = 0, status = 0;
    std::uint8_t  sms_type = 0, svr_type_val = 0;
    if (!r.Read(service_id) || !r.Read(status) ||
        !r.Read(sms_type)   || !r.Read(svr_type_val))
    {
        spdlog::warn("peer CT_SERVICECHANGE_REQ malformed body");
        co_return;
    }
    if (ctx.peers)
    {
        if (auto* st = ctx.peers->Status(service_id))
            st->status = static_cast<ServiceStatus>(status);
    }
    if (ctx.operators)
    {
        for (const auto& op : ctx.operators->SnapshotLoggedIn())
        {
            if (!op || !op->Wire() || !op->Wire()->IsOpen()) continue;
            co_await senders::SendServiceChangeAck(op->Wire(),
                service_id, status);
        }
    }
    if (sms_type != 0 && ctx.alerter)
    {
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool,
            [al = ctx.alerter, svr_type_val, service_id, sms_type] {
                al->Notify(svr_type_val, service_id, sms_type);
            });
    }
}

boost::asio::awaitable<void>
OnPeerAckRouteBack(std::shared_ptr<PeerSession> /*peer*/,
                   std::uint16_t out_id,
                   std::vector<std::byte> body,
                   const HandlerContext& ctx)
{
    // Legacy: peer's body starts with DWORD manager_id; strip it
    // and forward the residual body to the addressed operator.
    if (body.size() < 4)
    {
        spdlog::warn("peer ack 0x{:04X} body too small", out_id);
        co_return;
    }
    std::uint32_t manager_id = 0;
    std::memcpy(&manager_id, body.data(), 4);
    if (!ctx.operators) co_return;
    auto op = ctx.operators->FindBySeq(manager_id);
    if (!op || !op->Wire() || !op->Wire()->IsOpen()) co_return;

    std::vector<std::byte> tail(body.begin() + 4, body.end());
    co_await senders::SendRawForward(op->Wire(), out_id, tail);
}

boost::asio::awaitable<void>
OnPeerItemFindAck(std::shared_ptr<PeerSession> /*peer*/,
                  std::vector<std::byte> body,
                  const HandlerContext& ctx)
{
    // Legacy OnCT_ITEMFIND_ACK reads `wCount, dwManagerID` then
    // reassembles `wCount, [items]` for the operator. The
    // manager_id sits in the *middle* (after the count), not at
    // the head — repack accordingly.
    wire::Reader r(body);
    std::uint16_t count = 0;
    std::uint32_t manager_id = 0;
    if (!r.Read(count) || !r.Read(manager_id))
    {
        spdlog::warn("peer CT_ITEMFIND_ACK malformed header");
        co_return;
    }
    if (!ctx.operators) co_return;
    auto op = ctx.operators->FindBySeq(manager_id);
    if (!op || !op->Wire() || !op->Wire()->IsOpen()) co_return;

    std::vector<std::byte> out;
    wire::WritePOD<std::uint16_t>(out, count);
    // Append the remaining tail (the [items] array) as-is.
    const std::size_t header_bytes = 2 /*count*/ + 4 /*manager*/;
    if (body.size() > header_bytes)
        out.insert(out.end(),
            body.begin() + header_bytes, body.end());
    co_await senders::SendRawForward(op->Wire(),
        ToUint16(MessageId::CT_ITEMFIND_ACK), out);
}

boost::asio::awaitable<void>
OnPeerMonSpawnFindAck(std::shared_ptr<PeerSession> /*peer*/,
                      std::vector<std::byte> body,
                      const HandlerContext& ctx)
{
    // Legacy OnCT_MONSPAWNFIND_ACK reads `dwManagerID, wMapID,
    // wSpawnID, bMonCnt, [items]` then reassembles `wMapID,
    // wSpawnID, bMonCnt, [items]` for the operator (strips the
    // manager_id head).
    co_await OnPeerAckRouteBack(/*peer=*/nullptr,
        ToUint16(MessageId::CT_MONSPAWNFIND_ACK),
        std::move(body), ctx);
}

boost::asio::awaitable<void>
OnPeerCashItemSaleAck(std::shared_ptr<PeerSession> /*peer*/,
                      std::vector<std::byte> body,
                      const HandlerContext& ctx)
{
    // Legacy is observer-only: reads dwIndex + wValue, returns if
    // wValue is set, otherwise looks up the event for diagnostics.
    // We log + noop.
    wire::Reader r(body);
    std::uint32_t index = 0;
    std::uint16_t value = 0;
    if (r.Read(index) && r.Read(value))
        spdlog::debug("peer CT_CASHITEMSALE_ACK idx={} value={}",
            index, value);
    (void)ctx;
    co_return;
}

// CT_SERVICEUPLOAD* — disabled-feature stub. Always returns failure
// code 2 ("machine not found", reused as "feature unavailable")
// for UPLOADSTART; UPLOAD chunks are silently swallowed because
// the operator GUI stops sending after UPLOADSTART rejects.
// UPLOADEND replies with the same code so a stuck GUI still
// receives a terminating ack.
boost::asio::awaitable<void>
OnServiceUploadStartReq(std::shared_ptr<OperatorSession> op,
                        std::vector<std::byte> /*body*/,
                        const HandlerContext& ctx)
{
    if (!op->LoggedIn()) co_return;
    spdlog::info("CT_SERVICEUPLOADSTART_REQ from op='{}' — feature "
                 "disabled (UNC-share upload is intentionally not "
                 "implemented; use the CI/CD pipeline instead)",
        op->UserId());
    if (ctx.audit)
        ctx.audit->LogAdminAction(op->UserId(),
            "service_upload_rejected", "disabled");
    co_await senders::SendServiceUploadStartAck(op->Wire(), 2);
}

boost::asio::awaitable<void>
OnServiceUploadReq(std::shared_ptr<OperatorSession> op,
                   std::vector<std::byte> /*body*/,
                   const HandlerContext& /*ctx*/)
{
    // The GUI should not reach this branch — UPLOADSTART always
    // fails. If it does (out-of-spec client), drop the chunk
    // silently. Logging at debug so a curious operator can spot it.
    spdlog::debug("CT_SERVICEUPLOAD_REQ chunk from op='{}' — dropped "
                  "(feature disabled)", op->UserId());
    co_return;
}

boost::asio::awaitable<void>
OnServiceUploadEndReq(std::shared_ptr<OperatorSession> op,
                      std::vector<std::byte> /*body*/,
                      const HandlerContext& /*ctx*/)
{
    if (!op->LoggedIn()) co_return;
    co_await senders::SendServiceUploadEndAck(op->Wire(), 2);
}

} // namespace tcontrolsvr::handlers
