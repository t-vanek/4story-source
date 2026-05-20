#include "handlers.h"

#include "../peer_dialer.h"
#include "../senders.h"
#include "../wire_codec.h"
#include "MessageId.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <spdlog/spdlog.h>

#include <chrono>

namespace tcontrolsvr::handlers {

namespace {

constexpr std::uint8_t ACK_SUCCESS = 1;
constexpr std::uint8_t ACK_FAILED  = 0;
constexpr std::uint8_t ACK_PENDING = 2;

// Map RuntimeStatus rows from the registry into the wire-shape rows
// the legacy CT_SERVICESTAT_ACK uses (one record per registered
// service). The status DWORD carries the raw ServiceStatus enum so
// the GUI's existing "STOPPED / RUNNING / …" mapping table works
// unchanged.
std::vector<senders::ServiceStatRow> BuildStatSnapshot(const PeerRegistry& reg)
{
    std::vector<senders::ServiceStatRow> rows;
    rows.reserve(reg.Services().size());
    for (const auto& svc : reg.Services())
    {
        const auto* st = reg.Status(svc.service_id);
        const std::uint32_t status = st
            ? static_cast<std::uint32_t>(st->status)
            : static_cast<std::uint32_t>(ServiceStatus::Unknown);
        rows.push_back({
            svc.group_id,
            svc.type_id,
            svc.server_id,
            svc.name,
            svc.machine_id,
            status,
        });
    }
    return rows;
}

// Resolve the synthetic service id the GUI sends in
// CT_SERVICECONTROL_REQ / CT_NEWCONNECT_REQ. Returns the service
// instance + status pair, or nullptrs if the id isn't registered.
struct ResolvedService
{
    const ServiceInstance* svc    = nullptr;
    RuntimeStatus*         status = nullptr;
};

ResolvedService ResolveById(PeerRegistry& reg, std::uint32_t service_id)
{
    ResolvedService r{};
    r.svc    = reg.FindService(service_id);
    r.status = reg.Status(service_id);
    return r;
}

// Broadcast SERVICECHANGE to every logged-in operator. Caller has
// already mutated the runtime status; this just fans out the
// notification.
boost::asio::awaitable<void>
BroadcastServiceChange(const HandlerContext& ctx,
                       std::uint32_t service_id,
                       std::uint32_t status)
{
    if (!ctx.operators) co_return;
    for (const auto& op : ctx.operators->SnapshotLoggedIn())
    {
        if (!op || !op->Wire() || !op->Wire()->IsOpen()) continue;
        co_await senders::SendServiceChangeAck(op->Wire(), service_id, status);
    }
}

} // namespace

boost::asio::awaitable<void>
OnServiceStatReq(std::shared_ptr<OperatorSession> op,
                 std::vector<std::byte> /*body*/,
                 const HandlerContext& ctx)
{
    if (!op->LoggedIn() || !ctx.peers)
    {
        co_await senders::SendServiceStatAck(op->Wire(), {});
        co_return;
    }
    co_await senders::SendServiceStatAck(op->Wire(),
                                         BuildStatSnapshot(*ctx.peers));
}

boost::asio::awaitable<void>
OnServiceControlReq(std::shared_ptr<OperatorSession> op,
                    std::vector<std::byte> body,
                    const HandlerContext& ctx)
{
    if (!op->LoggedIn())
    {
        co_await senders::SendAuthorityAck(op->Wire());
        co_return;
    }
    // Legacy: CheckAuthority(MANAGER_SERVICE) on this handler. F3
    // wires the full role-gate enum; for F2 we only allow
    // MANAGER_SERVICE / MANAGER_ALL.
    const auto role = op->Role();
    if (role != OperatorRole::All && role != OperatorRole::Service)
    {
        co_await senders::SendAuthorityAck(op->Wire());
        co_return;
    }

    wire::Reader r(body);
    std::uint32_t service_id = 0;
    std::uint8_t  want_start = 0;
    if (!r.Read(service_id) || !r.Read(want_start))
    {
        co_await senders::SendServiceControlAck(op->Wire(), ACK_FAILED);
        co_return;
    }

    if (!ctx.peers)
    {
        co_await senders::SendServiceControlAck(op->Wire(), ACK_FAILED);
        co_return;
    }
    auto resolved = ResolveById(*ctx.peers, service_id);
    if (!resolved.svc || !resolved.status)
    {
        co_await senders::SendServiceControlAck(op->Wire(), ACK_FAILED);
        co_return;
    }

    // Cluster topology guard from legacy: stopping a WorldSvr
    // implicitly disables auto-restart for every other service in
    // the same group. F2 mirrors the flag flip; the actual stop
    // happens via the IServiceController in the same step.
    constexpr std::uint8_t SVRGRP_WORLDSVR = 5;  // legacy bType enum
    auto status_before = resolved.status->status;

    std::uint8_t ack = ACK_FAILED;
    if (ctx.controller)
    {
        if (want_start && status_before == ServiceStatus::Stopped)
        {
            const auto rc = co_await ctx.controller->Start(*resolved.svc);
            if (rc == ControlResult::Ok)
            {
                resolved.status->status = ServiceStatus::StartPending;
                ack = ACK_SUCCESS;
                spdlog::info("CT_SERVICECONTROL: start svc_id={:08x} "
                             "name='{}'", service_id, resolved.svc->name);
                co_await BroadcastServiceChange(ctx, service_id,
                    static_cast<std::uint32_t>(resolved.status->status));
            }
            else if (rc == ControlResult::NotSupported)
            {
                // DisabledServiceController default — match legacy
                // "no-op" semantics (no state change, ACK_FAILED).
                ack = ACK_FAILED;
                spdlog::info("CT_SERVICECONTROL: start svc_id={:08x} — "
                             "controller disabled", service_id);
            }
        }
        else if (!want_start && status_before == ServiceStatus::Running)
        {
            const auto rc = co_await ctx.controller->Stop(*resolved.svc);
            if (rc == ControlResult::Ok)
            {
                resolved.status->manager_control = 0;
                resolved.status->status = ServiceStatus::StopPending;
                ack = ACK_SUCCESS;
                spdlog::info("CT_SERVICECONTROL: stop svc_id={:08x} "
                             "name='{}'", service_id, resolved.svc->name);

                if (resolved.svc->type_id == SVRGRP_WORLDSVR && ctx.peers)
                {
                    // Cascade: every other service in the same group
                    // loses its manager_control flag (auto-restart
                    // disabled until the operator re-enables).
                    for (const auto& sibling : ctx.peers->Services())
                    {
                        if (sibling.group_id != resolved.svc->group_id) continue;
                        if (auto* sib_st = ctx.peers->Status(sibling.service_id))
                        {
                            sib_st->manager_control = 0;
                            spdlog::info("CT_SERVICECONTROL cascade: "
                                "cleared manager_control for svc_id={:08x}",
                                sibling.service_id);
                        }
                    }
                }
                co_await BroadcastServiceChange(ctx, service_id,
                    static_cast<std::uint32_t>(resolved.status->status));
            }
            else if (rc == ControlResult::NotSupported)
            {
                ack = ACK_FAILED;
                spdlog::info("CT_SERVICECONTROL: stop svc_id={:08x} — "
                             "controller disabled", service_id);
            }
        }
    }

    co_await senders::SendServiceControlAck(op->Wire(), ack);
}

boost::asio::awaitable<void>
OnNewConnectReq(std::shared_ptr<OperatorSession> op,
                std::vector<std::byte> body,
                const HandlerContext& ctx)
{
    if (!op->LoggedIn()) co_return;
    wire::Reader r(body);
    std::uint32_t service_id = 0;
    if (!r.Read(service_id)) co_return;

    if (!ctx.peers || !ctx.dialer)
    {
        spdlog::warn("CT_NEWCONNECT_REQ svc_id={:08x} — peer infra missing",
            service_id);
        co_return;
    }

    if (ctx.peers->Connection(service_id))
    {
        // Legacy: silently ignore — already connected. The GUI sees
        // the existing status update via the 1Hz monitor sweep.
        co_return;
    }

    const auto* svc = ctx.peers->FindService(service_id);
    if (!svc)
    {
        spdlog::warn("CT_NEWCONNECT_REQ svc_id={:08x} not in inventory",
            service_id);
        co_return;
    }

    auto result = co_await ctx.dialer->Dial(*svc);
    if (!result.session)
    {
        spdlog::warn("CT_NEWCONNECT_REQ svc_id={:08x} dial failed: {}",
            service_id, result.failure_reason);
        co_return;
    }

    // Handshake — say "hello, I am control" so the peer knows the
    // socket should be treated as a TControlSvr peer rather than a
    // patching client (legacy CTServer::SendCT_CTRLSVR_REQ).
    co_await senders::SendCtrlSvrReq(result.session->Wire());

    // Spawn the read loop so the peer's CT_SERVICEMONITOR_REQ +
    // admin-ack forwarders (F3) can land back here. The loop exits
    // when the peer closes or framing breaks.
    if (ctx.io)
    {
        boost::asio::co_spawn(*ctx.io,
            RunPeerLoop(result.session, ctx),
            boost::asio::detached);
    }
}

boost::asio::awaitable<void>
OnReconnectReq(std::shared_ptr<OperatorSession> op,
               std::vector<std::byte> body,
               const HandlerContext& ctx)
{
    // Legacy CT_RECONNECT_REQ closes the existing CTServer connection
    // and re-enters the OnCT_NEWCONNECT_REQ dispatch path. We do the
    // same — close any current connection, then delegate.
    wire::Reader r(body);
    std::uint32_t service_id = 0;
    if (!r.Read(service_id)) co_return;
    if (!ctx.peers) co_return;

    if (auto existing = ctx.peers->Connection(service_id))
    {
        if (existing->Wire()) existing->Wire()->Close();
        ctx.peers->ClearConnection(service_id);
    }
    // Re-dispatch with the same id — repack the body so OnNewConnectReq
    // can parse it identically.
    std::vector<std::byte> repack;
    wire::WritePOD<std::uint32_t>(repack, service_id);
    co_await OnNewConnectReq(std::move(op), std::move(repack), ctx);
}

boost::asio::awaitable<void>
OnServiceMonitorReq(std::shared_ptr<PeerSession> peer,
                    std::vector<std::byte> body,
                    const HandlerContext& ctx)
{
    if (!peer || !ctx.peers) co_return;
    wire::Reader r(body);
    std::uint32_t tick = 0, sessions = 0, users = 0, active_users = 0;
    if (!r.Read(tick) || !r.Read(sessions) ||
        !r.Read(users) || !r.Read(active_users))
    {
        spdlog::warn("CT_SERVICEMONITOR_REQ malformed body");
        co_return;
    }

    const auto svc_id = peer->ServiceId();
    auto* st = ctx.peers->Status(svc_id);
    if (!st) co_return;

    // Echo the tick back so the peer can RTT-measure (legacy
    // CTServer::SendCT_SERVICEMONITOR_ACK).
    co_await senders::SendServiceMonitorAck(peer->Wire(), tick);

    const auto now_ms = static_cast<std::uint32_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    const std::uint32_t ping_ms = now_ms - tick;

    st->status         = ServiceStatus::Running;
    st->cur_users      = users;
    st->last_recv_tick = now_ms;
    if (users > st->max_users)
    {
        st->max_users      = users;
        st->peak_time_unix = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    // Fan out to every logged-in operator (legacy walks m_mapMANAGER
    // and sends to all — F2 keeps the same behavior; the
    // authority-based filtering for SERVICEDATA lands with the
    // admin-handler family in F3).
    if (ctx.operators)
    {
        for (const auto& op : ctx.operators->SnapshotLoggedIn())
        {
            if (!op || !op->Wire() || !op->Wire()->IsOpen()) continue;
            co_await senders::SendServiceDataAck(op->Wire(),
                svc_id, sessions, users, st->max_users, ping_ms,
                st->peak_time_unix, st->stop_count, st->latest_stop_unix,
                active_users);
        }
    }
}

boost::asio::awaitable<void>
RunPeerLoop(std::shared_ptr<PeerSession> peer, HandlerContext ctx)
{
    using tnetlib::protocol::MessageId;
    using tnetlib::protocol::ToMessageId;

    co_await peer->Wire()->Run(
        [peer, ctx](std::shared_ptr<ControlSession> /*s*/,
                    DecodedPacket pkt) -> boost::asio::awaitable<void>
        {
            const auto id = ToMessageId(pkt.wId);
            switch (id)
            {
            case MessageId::CT_SERVICEMONITOR_REQ:
                co_await OnServiceMonitorReq(peer, std::move(pkt.body), ctx);
                break;
            default:
                spdlog::debug("peer_loop: unhandled CT_* id=0x{:04X} from "
                              "svc_id={:08x}", pkt.wId, peer->ServiceId());
                break;
            }
        });

    // Connection closed — clear from registry so the next
    // CT_NEWCONNECT_REQ can dial again.
    spdlog::info("peer_loop: svc_id={:08x} disconnected",
        peer->ServiceId());
    if (ctx.peers)
    {
        if (auto* st = ctx.peers->Status(peer->ServiceId()))
        {
            st->status = ServiceStatus::Stopped;
            st->stop_count += 1;
            st->latest_stop_unix = std::chrono::duration_cast<
                std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
        }
        ctx.peers->ClearConnection(peer->ServiceId());
    }
}

} // namespace tcontrolsvr::handlers
