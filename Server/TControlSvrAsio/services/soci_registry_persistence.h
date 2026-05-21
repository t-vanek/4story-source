#pragma once

// SOCI-backed IRegistryPersistence — writes to TPEER_REGISTRY in
// TGLOBAL. See schema/tcontrol-peer-registry.sql for the DDL.
//
// Write path: Upsert / Touch / Remove are sync-looking but post the
// actual SOCI call onto a worker pool to keep the io_context thread
// off the DB latency hot path. The PeerRegistry mutator returns
// immediately; the row is written ~milliseconds later. A failed
// write logs at warn and is dropped — the next heartbeat from the
// same peer will retry the row, and the in-RAM registry stays
// authoritative (the worst-case outcome of a persistent DB outage
// is the same as not having persistence at all).
//
// Read path: LoadAll() is called once from main() before the listener
// starts accepting CT_PEER_REGISTER_REQ. It runs synchronously on
// the boot thread — blocking is acceptable there, and we need the
// data before continuing.

#include "registry_persistence.h"

#include "fourstory/db/session_pool.h"

#include <string>

namespace boost::asio { class thread_pool; }

namespace tcontrolsvr {

class SociRegistryPersistence final : public IRegistryPersistence
{
public:
    struct Options
    {
        // Defaults to "TPEER_REGISTRY"; operators can override when
        // their DB layout uses a different name (rare, but the
        // existing target_table pattern in TLogSvrAsio already
        // supports it so we mirror).
        std::string  table_name   = "TPEER_REGISTRY";

        // Worker pool used to offload the blocking SOCI calls.
        // nullptr → execute inline (blocks io_context). Production
        // wiring shares the same pool as the rest of TControl's SOCI
        // call sites.
        boost::asio::thread_pool* worker_pool = nullptr;
    };

    SociRegistryPersistence(fourstory::db::SessionPool& pool, Options opts);

    void Upsert(const RegistryEntry& entry) override;
    void Touch(std::uint32_t  service_id,
               std::uint64_t  lease_epoch,
               std::uint32_t  cur_users,
               std::uint32_t  max_users,
               std::int64_t   heartbeat_unix) override;
    void Remove(std::uint32_t service_id) override;
    std::vector<RegistryEntry> LoadAll() override;

private:
    fourstory::db::SessionPool& m_pool;
    Options                     m_opts;
};

} // namespace tcontrolsvr
