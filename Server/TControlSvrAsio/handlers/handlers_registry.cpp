// Peer self-registration handlers — F1 of the modern cluster control
// plane.
//
// Legacy 4Story has TControl dial out to each peer over CT_NEWCONNECT_REQ
// using addresses operators type in via the GUI. That model is fine when
// the cluster topology is static but breaks the moment a peer's address
// is dynamic (containers, autoscaling) or the operator isn't around to
// click "connect" after a peer restart. Modern peers instead dial
// TControl on startup and announce themselves with CT_PEER_REGISTER_REQ;
// TControl assigns a lease epoch, the peer keeps it alive with
// CT_PEER_HEARTBEAT_REQ on a fixed tick, and the lease-expiry sweep
// in ControlServer reaps anything that misses its heartbeat window.
//
// Wire shape (REGISTER_REQ body):
//   DWORD   service_id        // (group<<16)|(type<<8)|server
//   CString reported_name     // operator-readable service name
//   CString reported_addr     // peer's self-reported IPv4 (dotted)
//   WORD    reported_port     // peer's own listener port
//   CString version           // build / git rev, free-form
//   DWORD   pid
//   QWORD   start_unix        // peer's own boot wall-clock (seconds)
//
// Wire shape (HEARTBEAT_REQ body):
//   DWORD service_id, QWORD lease_epoch, DWORD cur_users, DWORD max_users
//
// Wire shape (DEREGISTER_REQ body):
//   DWORD service_id, QWORD lease_epoch
//
// Cadence policy: heartbeat_interval_sec is fixed at 30s in the ACK so
// peers don't need their own config knob for it. Sweep drops entries
// older than ~3 * interval (90s) — covers two missed heartbeats before
// the third comes in late. Reconfigurable via ControlServerConfig.

#include "handlers.h"

#include "../senders.h"
#include "../wire_codec.h"
#include "../peer_session.h"
#include "../services/peer_registry.h"
#include "../services/peer_repository.h"
#include "MessageId.h"

#include "fourstory/db/co_offload.h"
#include "fourstory/security/hostname_match.h"


#include <spdlog/spdlog.h>

namespace tcontrolsvr::handlers {

namespace {

// Reject codes for CT_PEER_REGISTER_ACK.reason_code. Bumped together
// with the wire format if the registry contract changes.
constexpr std::uint32_t kRejectUnknownService = 1;
constexpr std::uint32_t kRejectMalformed      = 2;
constexpr std::uint32_t kRejectCnMismatch     = 3;
constexpr std::uint32_t kRejectInternal       = 99;

// Fixed cadence the control side asks every peer to use. 30s matches
// the legacy WorkTickProc reconnect tick — peers that were tolerant
// of that interval before will be tolerant of this one.
constexpr std::uint32_t kHeartbeatIntervalSec = 30;

} // namespace

boost::asio::awaitable<void>
OnPeerRegisterReq(std::shared_ptr<OperatorSession> op,
                  std::vector<std::byte> body,
                  const HandlerContext& ctx)
{
    if (!ctx.peers)
    {
        spdlog::error("CT_PEER_REGISTER_REQ: peers registry unavailable");
        co_await senders::SendPeerRegisterAck(op->Wire(), 0,
            kRejectInternal, 0, kHeartbeatIntervalSec);
        co_return;
    }

    wire::Reader r(body);
    std::uint32_t service_id = 0;
    std::string   name, addr, version;
    std::uint16_t port = 0;
    std::uint32_t pid  = 0;
    std::uint64_t start_unix = 0;
    if (!r.Read(service_id)            ||
        !r.ReadString(name)            ||
        !r.ReadString(addr)            ||
        !r.Read(port)                  ||
        !r.ReadString(version)         ||
        !r.Read(pid)                   ||
        !r.Read(start_unix))
    {
        spdlog::warn("CT_PEER_REGISTER_REQ: malformed body ({} bytes)",
            body.size());
        co_await senders::SendPeerRegisterAck(op->Wire(), 0,
            kRejectMalformed, 0, kHeartbeatIntervalSec);
        co_return;
    }

    // Validate against the static inventory — peers can't register a
    // service_id the operator hasn't provisioned. Prevents typos /
    // misconfigured peers from polluting the live registry.
    if (ctx.peers->FindService(service_id) == nullptr)
    {
        spdlog::warn("CT_PEER_REGISTER_REQ: unknown service_id={:#x} "
                     "(reported_name='{}')", service_id, name);
        co_await senders::SendPeerRegisterAck(op->Wire(), 0,
            kRejectUnknownService, 0, kHeartbeatIntervalSec);
        co_return;
    }

    // Post-handshake identity validation. When the peer connected
    // over TLS, the cert's Common Name AND any DNS-type SAN entries
    // were captured into ControlSession at construction. We accept
    // a name match against either CN or any SAN — RFC 5280 §4.2.1.6
    // deprecates CN-as-identity in favor of SAN, and modern PKI
    // tooling (cert-manager, step-ca, ACME) frequently leaves CN
    // empty and only populates SAN.
    //
    // Policy (otherwise unchanged from PR #47):
    //   * No TLS identity at all (plain-TCP session): skip.
    //   * Trust map has no opinion for this slot: skip.
    //   * Trust map has an opinion AND neither CN nor any SAN
    //     matches: REJECT with kRejectCnMismatch.
    const auto& peer_cn   = op->Wire()->PeerCommonName();
    const auto& peer_sans = op->Wire()->PeerSubjectAltNames();
    const bool has_tls_identity = !peer_cn.empty() || !peer_sans.empty();

    if (has_tls_identity && ctx.security != nullptr)
    {
        const std::uint8_t group_id  =
            static_cast<std::uint8_t>((service_id >> 16) & 0xFF);
        const std::uint8_t server_id =
            static_cast<std::uint8_t>(service_id & 0xFF);
        const auto expected =
            ctx.security->LookupPeerName(group_id, server_id);
        if (expected.has_value())
        {
            // Identity match policy: literal compare against CN +
            // every SAN entry, plus RFC 6125 wildcard expansion on
            // the SAN side (operators sometimes issue one
            // "*.cluster.local" cert per machine instead of one
            // cert per peer). CN gets literal-only comparison
            // because CN-as-identity is deprecated and wildcards
            // there are easy to misuse.
            const bool cn_matches =
                fourstory::security::detail::EqualIgnoreCase(
                    peer_cn, *expected);
            bool san_matches = false;
            for (const auto& san : peer_sans)
            {
                if (fourstory::security::HostnameMatch(san, *expected))
                {
                    san_matches = true;
                    break;
                }
            }
            if (!cn_matches && !san_matches)
            {
                spdlog::warn("CT_PEER_REGISTER_REQ: identity mismatch — "
                             "cert CN='{}' SANs={} expected='{}' "
                             "service_id={:#x}",
                    peer_cn, peer_sans.size(), *expected, service_id);
                co_await senders::SendPeerRegisterAck(op->Wire(), 0,
                    kRejectCnMismatch, 0, kHeartbeatIntervalSec);
                co_return;
            }
        }
    }

    RegistryEntry entry{};
    entry.service_id    = service_id;
    entry.reported_name = std::move(name);
    entry.reported_addr = std::move(addr);
    entry.reported_port = port;
    entry.version       = std::move(version);
    entry.pid           = pid;
    entry.start_unix    = static_cast<std::int64_t>(start_unix);
    const auto epoch = ctx.peers->Register(entry);

    // Persist registration to TPEER_REGISTRY + log status transition
    // so TControlSvr can recover after a restart.
    if (ctx.peer_repo)
    {
        const RegistryEntry entry_snap = entry;
        const auto* svc_snap = ctx.peers->FindService(service_id);
        const std::uint8_t type_id = svc_snap
            ? static_cast<std::uint8_t>((service_id >> 8) & 0xFF) : 0;
        const std::uint8_t group_id  = static_cast<std::uint8_t>((service_id >> 16) & 0xFF);
        const std::uint8_t server_id = static_cast<std::uint8_t>(service_id & 0xFF);
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool, [&] {
            ctx.peer_repo->Upsert(entry_snap);
            ctx.peer_repo->InsertStatusLog(
                group_id, server_id, type_id,
                ServiceStatus::Unknown, ServiceStatus::Running,
                "peer_register");
        });
    }


    // Attach the inbound socket to the connection table so admin
    // forwarders (kick / announcement) can find the peer immediately
    // after registration. The PeerSession reuses the OperatorSession's
    // wire — same underlying ControlSession.
    const auto* svc = ctx.peers->FindService(service_id);
    if (svc)
    {
        auto peer = std::make_shared<PeerSession>(op->Wire(), *svc);
        ctx.peers->SetConnection(service_id, peer);
    }

    spdlog::info("peer registered: service_id={:#x} name='{}' addr={}:{} "
                 "version='{}' pid={} lease={}",
        service_id, entry.reported_name, entry.reported_addr,
        entry.reported_port, entry.version, entry.pid, epoch);

    co_await senders::SendPeerRegisterAck(op->Wire(), 1, 0, epoch,
        kHeartbeatIntervalSec);
}

boost::asio::awaitable<void>
OnPeerHeartbeatReq(std::shared_ptr<OperatorSession> op,
                   std::vector<std::byte> body,
                   const HandlerContext& ctx)
{
    if (!ctx.peers)
    {
        co_await senders::SendPeerHeartbeatAck(op->Wire(), 0, 0);
        co_return;
    }
    wire::Reader r(body);
    std::uint32_t service_id = 0;
    std::uint64_t lease_epoch = 0;
    std::uint32_t cur_users = 0, max_users = 0;
    if (!r.Read(service_id)   ||
        !r.Read(lease_epoch)  ||
        !r.Read(cur_users)    ||
        !r.Read(max_users))
    {
        spdlog::warn("CT_PEER_HEARTBEAT_REQ: malformed body");
        co_await senders::SendPeerHeartbeatAck(op->Wire(), 0, 0);
        co_return;
    }
    const auto current = ctx.peers->Heartbeat(service_id, lease_epoch,
        cur_users, max_users);
    if (current == 0)
    {
        // Stale epoch or expired lease — instruct the peer to
        // re-register by replying accepted=0. Common case: TControl
        // restarted between the peer's registration and this
        // heartbeat, so the in-memory lease is gone.
        spdlog::info("heartbeat rejected: service_id={:#x} "
                     "lease_epoch={} (stale / unregistered)",
            service_id, lease_epoch);
        co_await senders::SendPeerHeartbeatAck(op->Wire(), 0, 0);
        co_return;
    }
    // Persist heartbeat to TPEER_REGISTRY (fire-and-forget offload).
    if (ctx.peer_repo)
    {
        const std::uint32_t sid = service_id;
        const std::uint64_t ep  = current;
        const std::uint32_t cu  = cur_users;
        const std::uint32_t mu  = max_users;
        co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool, [&] {
            ctx.peer_repo->UpdateHeartbeat(sid, ep, cu, mu);
        });
    }
    // Mirror the heartbeat into RuntimeStatus so SERVICEMONITOR
    // consumers (operator GUI, admin shell `peers`) see the latest
    // user counts even before a CT_SERVICEMONITOR_REQ arrives.
    if (auto* st = ctx.peers->Status(service_id))
    {
        st->cur_users = cur_users;
        st->max_users = max_users;
    }
    co_await senders::SendPeerHeartbeatAck(op->Wire(), 1, current);
}

boost::asio::awaitable<void>
OnPeerDeregisterReq(std::shared_ptr<OperatorSession> /*op*/,
                    std::vector<std::byte> body,
                    const HandlerContext& ctx)
{
    if (!ctx.peers) co_return;
    wire::Reader r(body);
    std::uint32_t service_id = 0;
    std::uint64_t lease_epoch = 0;
    if (!r.Read(service_id) || !r.Read(lease_epoch))
    {
        spdlog::warn("CT_PEER_DEREGISTER_REQ: malformed body");
        co_return;
    }
    const bool ok = ctx.peers->Deregister(service_id, lease_epoch);
    if (ok)
    {
        ctx.peers->ClearConnection(service_id);
        spdlog::info("peer deregistered: service_id={:#x} lease={}",
            service_id, lease_epoch);
        if (ctx.peer_repo)
        {
            const std::uint32_t sid = service_id;
            const std::uint8_t group_id  = static_cast<std::uint8_t>((sid >> 16) & 0xFF);
            const std::uint8_t type_id   = static_cast<std::uint8_t>((sid >>  8) & 0xFF);
            const std::uint8_t server_id = static_cast<std::uint8_t>( sid        & 0xFF);
            co_await fourstory::db::CoOffloadVoidIf(ctx.db_pool, [&] {
                ctx.peer_repo->Delete(sid);
                ctx.peer_repo->InsertStatusLog(
                    group_id, server_id, type_id,
                    ServiceStatus::Running, ServiceStatus::Stopped,
                    "peer_deregister");
            });
        }
    }
    else
    {
        spdlog::info("CT_PEER_DEREGISTER_REQ: stale/unknown "
                     "service_id={:#x} lease={}", service_id, lease_epoch);
    }
    co_return;
}

} // namespace tcontrolsvr::handlers
