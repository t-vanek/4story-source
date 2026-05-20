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

#include "service_inventory.h"
#include "service_controller.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace tcontrolsvr {

class PeerSession;

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

private:
    std::vector<ServiceInstance> m_services;
    std::unordered_map<std::uint32_t, std::size_t>          m_idx;
    std::unordered_map<std::uint32_t, RuntimeStatus>        m_status;
    std::unordered_map<std::uint32_t, std::shared_ptr<PeerSession>> m_conn;
};

} // namespace tcontrolsvr
