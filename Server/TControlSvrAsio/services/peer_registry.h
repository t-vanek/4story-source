#pragma once

// PeerRegistry — runtime mapping of synthetic service_id → PeerSession.
// Mirrors legacy `m_mapTSVRTEMP` lookups in the dispatcher (which
// resolves OnCT_SERVICEMONITOR_REQ / OnCT_NEWCONNECT_REQ /
// OnCT_TIMER_REQ via FindService(dwID)).
//
// A second layer of state tracks the **service status** (Stopped /
// Running / StartPending / …) and the **live counters** (current
// session count, max session count, peak time, stop count). These
// fields live on `RuntimeStatus` here rather than on PeerSession
// because they must survive the peer reconnecting — legacy keeps
// them on TSVRTEMP for the same reason.

#include "registry_event_bus.h"
#include "service_inventory.h"
#include "service_controller.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tcontrolsvr {

class PeerSession;
class IRegistryPersistence;

struct RuntimeStatus
{
    ServiceStatus status            = ServiceStatus::Unknown;
    std::uint32_t cur_users         = 0;
    std::uint32_t max_users         = 0;
    std::int64_t  peak_time_unix    = 0;   // legacy m_nPickTime
    std::uint32_t stop_count        = 0;
    std::int64_t  latest_stop_unix  = 0;
    std::uint32_t last_recv_tick    = 0;   // legacy m_dwRecvTick (ms)
    std::uint8_t  manager_control   = 0;   // legacy m_bManagerControl
};

// Dynamic registration record — what the peer reports about itself
// when it dials TControl and sends CT_PEER_REGISTER_REQ. Distinct from
// `ServiceInstance` (static, DB-backed inventory) and from
// `RuntimeStatus` (cached from periodic SERVICEMONITOR pings). Lives
// only as long as the peer keeps its lease alive via heartbeats; the
// lease-expiry sweep drops entries that miss ~3 heartbeat intervals.
struct RegistryEntry
{
    std::uint32_t  service_id      = 0;
    std::string    reported_name;          // peer's self-reported name
    std::string    reported_addr;          // peer's self-reported IPv4
    std::uint16_t  reported_port    = 0;
    std::string    version;                // build / git rev string
    std::uint32_t  pid              = 0;
    std::int64_t   start_unix       = 0;   // peer's own boot time
    std::uint32_t  cur_users        = 0;   // latest heartbeat
    std::uint32_t  max_users        = 0;
    std::uint64_t  lease_epoch      = 0;   // bumps on re-register
    // In-RAM steady-clock view used by the expiry sweep + admin
    // shell age display. When the registry is hydrated from
    // persistence at boot, these are synthesized from stored unix
    // timestamps so "now - last_heartbeat_at" still yields the
    // peer's actual heartbeat age.
    std::chrono::steady_clock::time_point registered_at{};
    std::chrono::steady_clock::time_point last_heartbeat_at{};
};

class PeerRegistry
{
public:
    explicit PeerRegistry(const IServiceInventory& inventory);

    // Allow callers to refresh from a new inventory snapshot when the
    // RegistryRefresher rotates it. Preserves runtime status for
    // services that survive the swap; drops entries for services
    // that no longer exist.
    void Rebind(const IServiceInventory& inventory);

    const ServiceInstance* FindService(std::uint32_t service_id) const;
    const std::vector<ServiceInstance>& Services() const { return m_services; }

    // Status table — the legacy m_mapTSVRTEMP runtime fields.
    RuntimeStatus*       Status(std::uint32_t service_id);
    const RuntimeStatus* Status(std::uint32_t service_id) const;

    // Active outbound connection to the peer. nullptr until
    // CT_NEWCONNECT_REQ + dial succeeds; cleared on disconnect.
    std::shared_ptr<PeerSession> Connection(std::uint32_t service_id) const;
    void                          SetConnection(std::uint32_t service_id,
                                                std::shared_ptr<PeerSession> conn);
    void                          ClearConnection(std::uint32_t service_id);

    // Lookup peers by (group, type) — needed for the admin-handler
    // forwarders (KICK, ANNOUNCEMENT, …) that broadcast to all
    // MapSvrs in a world.
    std::vector<std::shared_ptr<PeerSession>>
        FindByType(std::uint8_t group_id, std::uint8_t type_id) const;

    // --- Dynamic registry (peer self-registration, CT_PEER_*) -------
    //
    // Register / Heartbeat / Deregister return the lease epoch the
    // caller should echo on subsequent heartbeats. Register bumps the
    // epoch (re-registration is fine — equivalent to a peer restart).
    // Heartbeat refuses stale epochs and returns 0 so the peer knows
    // it must re-register.

    std::uint64_t Register(const RegistryEntry& proposed);
    std::uint64_t Heartbeat(std::uint32_t service_id,
                            std::uint64_t lease_epoch,
                            std::uint32_t cur_users,
                            std::uint32_t max_users);
    bool          Deregister(std::uint32_t service_id,
                             std::uint64_t lease_epoch);

    // Drop entries whose last heartbeat is older than `max_age`. Mirrors
    // the legacy stale-peer sweep but driven by lease semantics instead
    // of socket liveness — covers cases where a peer's network is
    // partitioned but its TCP socket still appears open.
    std::size_t   ExpireStale(std::chrono::steady_clock::duration max_age);

    // Snapshot of the live registry, copied by value so callers
    // (admin shell `registry` command, monitoring) can iterate
    // lock-free.
    std::vector<RegistryEntry> Registry() const;

    const RegistryEntry* FindRegistration(std::uint32_t service_id) const;

    // Live event stream — subscribers see every Register/Heartbeat/
    // Deregister/Expire transition without polling. Used by the
    // admin shell `subscribe registry` command.
    RegistryEventBus& Events()       { return m_events; }
    const RegistryEventBus& Events() const { return m_events; }

    // Optional persistence sink. When wired (default Noop instance),
    // every Register/Heartbeat/Deregister/Expire propagates to
    // durable storage so TControl restart can resume the registry
    // from a snapshot rather than a 90s blank window.
    void SetPersistence(IRegistryPersistence* persistence)
    { m_persistence = persistence; }

    // Boot reload — populates m_registry from the persistence
    // snapshot, advances the lease-epoch counter past every stored
    // epoch (so future Register calls don't collide with resurrected
    // entries), and publishes one Registered event per loaded row
    // so streaming subscribers see the hydrated state. Does NOT run
    // ExpireStale — main.cpp does that immediately after Hydrate so
    // peers that were already stale at restart get cleaned up.
    void Hydrate(const std::vector<RegistryEntry>& entries);

private:
    std::vector<ServiceInstance> m_services;
    std::unordered_map<std::uint32_t, std::size_t>          m_idx;
    std::unordered_map<std::uint32_t, RuntimeStatus>        m_status;
    std::unordered_map<std::uint32_t, std::shared_ptr<PeerSession>> m_conn;
    std::unordered_map<std::uint32_t, RegistryEntry>        m_registry;
    std::uint64_t                                           m_next_epoch = 1;
    RegistryEventBus                                        m_events;
    IRegistryPersistence*                                   m_persistence = nullptr;
};

} // namespace tcontrolsvr
