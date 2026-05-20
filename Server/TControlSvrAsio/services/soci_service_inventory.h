#pragma once

// SOCI-backed IServiceInventory. Loads TMACHINE / TGROUP / TSVRTYPE /
// TSERVER / TIPADDR (TGLOBAL_RAGEZONE) into in-memory POD vectors at
// startup. The legacy loader runs in `CTControlSvrModule::LoadData`
// at service start; the modern equivalent runs at the same point
// (in `main` before the accept loop starts).
//
// Filters from the legacy queries:
//   - TSVRTYPE WHERE bControl = 1
//   - TSERVER  WHERE bType <> 6       (skip "MSGSVR")
//   - TIPADDR  WHERE bActive = 1
//
// Synthetic service_id = (group<<16) | (type<<8) | server — same
// composition used by legacy CTControlSvrModule::FindService(DWORD).
//
// Reload() is provided so the RegistryRefresher in
// FourStoryCommon::ops can refresh the snapshot on a timer when
// operators change the topology without restarting the daemon.

#include "service_inventory.h"

#include "fourstory/db/session_pool.h"

#include <mutex>

namespace tcontrolsvr {

class SociServiceInventory final : public IServiceInventory
{
public:
    explicit SociServiceInventory(fourstory::db::SessionPool& pool);

    // Read everything fresh from the DB. Throws on connect failure
    // — callers (main) treat that as fatal at startup; refresh
    // failures are logged and the previous snapshot stays in place.
    void Reload();

    const std::vector<Group>&            Groups()   const override;
    const std::vector<Machine>&          Machines() const override;
    const std::vector<ServerType>&       Types()    const override;
    const std::vector<ServiceInstance>&  Services() const override;

private:
    fourstory::db::SessionPool& m_pool;

    // Snapshots are copied wholesale by Reload; readers iterate the
    // current vectors. The lock guards the pointer swap; iteration
    // itself is lock-free against the snapshot it captured. F1/F2
    // run single-threaded so the lock is largely belt-and-suspenders.
    mutable std::mutex            m_mtx;
    std::vector<Group>            m_groups;
    std::vector<Machine>          m_machines;
    std::vector<ServerType>       m_types;
    std::vector<ServiceInstance>  m_services;
};

} // namespace tcontrolsvr
