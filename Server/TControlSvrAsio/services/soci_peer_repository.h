#pragma once

#include "peer_repository.h"

#include "fourstory/db/session_pool.h"

#include <chrono>
#include <cstdint>
#include <unordered_map>

namespace tcontrolsvr {

class SociPeerRepository final : public IPeerRepository
{
public:
    explicit SociPeerRepository(fourstory::db::SessionPool& pool);

    void Upsert(const RegistryEntry& entry) override;
    void UpdateHeartbeat(std::uint32_t service_id,
                         std::uint64_t lease_epoch,
                         std::uint32_t cur_users,
                         std::uint32_t max_users) override;
    void Delete(std::uint32_t service_id) override;
    void InsertStatusLog(std::uint8_t  group_id,
                         std::uint8_t  server_id,
                         std::uint8_t  type_id,
                         ServiceStatus old_status,
                         ServiceStatus new_status,
                         const std::string& reason) override;
    void InsertMetrics(const PeerMetricsSample& sample) override;
    std::vector<RegistryEntry> LoadAll() override;
    void PurgeOldRows(int status_log_days, int metrics_days) override;

private:
    fourstory::db::SessionPool& m_pool;

    // Per-peer last metrics insert time — throttle to 1/min.
    std::unordered_map<std::uint32_t,
        std::chrono::steady_clock::time_point> m_metrics_last;
};

} // namespace tcontrolsvr
