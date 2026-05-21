#pragma once

// IPeerRepository — DB operations for the dynamic peer state tables:
//   TPEER_REGISTRY    (live registrations — upsert / heartbeat / delete)
//   TPEER_STATUS_LOG  (status-change history)
//   TPEER_METRICS     (sampled monitoring counters, 1/min per peer)
//
// All methods are synchronous blocking. Callers on the io_context
// strand must offload via fourstory::db::CoOffloadIf.

#include "peer_registry.h"
#include "service_controller.h"

#include <cstdint>
#include <string>
#include <vector>

namespace tcontrolsvr {

struct PeerMetricsSample
{
    std::uint8_t  group_id    = 0;
    std::uint8_t  server_id   = 0;
    std::uint8_t  type_id     = 0;
    std::uint32_t sessions    = 0;
    std::uint32_t users       = 0;
    std::uint32_t active      = 0;
};

class IPeerRepository
{
public:
    virtual ~IPeerRepository() = default;

    // Upsert a registration row — called on CT_PEER_REGISTER_REQ.
    // The row key is (group_id, server_id) decomposed from entry.service_id.
    virtual void Upsert(const RegistryEntry& entry) = 0;

    // Update heartbeat timestamp + user counters in the existing row.
    // No-ops when the row is absent (peer re-registered elsewhere).
    virtual void UpdateHeartbeat(std::uint32_t service_id,
                                 std::uint64_t lease_epoch,
                                 std::uint32_t cur_users,
                                 std::uint32_t max_users) = 0;

    // Delete a registration row on deregister or lease expiry.
    virtual void Delete(std::uint32_t service_id) = 0;

    // Append a status-change record to TPEER_STATUS_LOG.
    virtual void InsertStatusLog(std::uint8_t  group_id,
                                 std::uint8_t  server_id,
                                 std::uint8_t  type_id,
                                 ServiceStatus old_status,
                                 ServiceStatus new_status,
                                 const std::string& reason) = 0;

    // Append a monitoring snapshot to TPEER_METRICS.
    // Implementation throttles to one INSERT per peer per minute.
    virtual void InsertMetrics(const PeerMetricsSample& sample) = 0;

    // Load all rows from TPEER_REGISTRY at startup. The caller
    // re-registers them with PeerRegistry so the in-memory state is
    // warm after a TControlSvr restart. Entries with no heartbeat
    // within the lease window will expire naturally.
    virtual std::vector<RegistryEntry> LoadAll() = 0;

    // Purge historical rows. Called once at startup to enforce retention.
    virtual void PurgeOldRows(int status_log_days, int metrics_days) = 0;
};

} // namespace tcontrolsvr
