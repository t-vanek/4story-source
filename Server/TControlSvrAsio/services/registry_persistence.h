#pragma once

// IRegistryPersistence — snapshot the dynamic peer registry to durable
// storage so TControl restarts don't blank the cluster's known state.
// Without it: TControl restart wipes m_registry, every peer's next
// heartbeat fails with "stale lease", every peer re-registers within
// ~30s. With it: TControl restart loads the snapshot at boot, peers
// continue heartbeating against the same lease, no 90s "all peers
// missing" window in the operator GUI.
//
// Interface contract:
//
//   Upsert(entry)        — called from PeerRegistry::Register. Stores
//                          (or replaces) the full entry. Persistence
//                          impls treat service_id as the primary key
//                          and overwrite on conflict — re-registration
//                          is normal (peer restart).
//
//   Touch(sid, lease,    — called from PeerRegistry::Heartbeat. Only
//         cur, max,        updates the heartbeat timestamp + user
//         heartbeat_unix)  counts. Ignores the call if (sid,lease)
//                          doesn't match the stored row — same epoch
//                          check the in-RAM Heartbeat already does.
//
//   Remove(sid)          — called from PeerRegistry::Deregister and
//                          from ExpireStale. Deletes the row.
//
//   LoadAll()            — called once at boot. Returns every stored
//                          row as a RegistryEntry. PeerRegistry::
//                          Hydrate() then walks the vector + populates
//                          its in-RAM map.
//
// Threading: PeerRegistry calls Upsert/Touch/Remove from the
// io_context thread (same thread that runs the CT_PEER_* handlers
// and the lease-expiry sweep). The SOCI impl posts the actual DB
// write onto a worker pool so the reactor isn't blocked on
// network/disk latency. Sync impls (Noop, in-memory test fakes) can
// just execute inline.

#include "peer_registry.h"

#include <cstdint>
#include <vector>

namespace tcontrolsvr {

class IRegistryPersistence
{
public:
    virtual ~IRegistryPersistence() = default;

    virtual void Upsert(const RegistryEntry& entry) = 0;

    virtual void Touch(std::uint32_t  service_id,
                       std::uint64_t  lease_epoch,
                       std::uint32_t  cur_users,
                       std::uint32_t  max_users,
                       std::int64_t   heartbeat_unix) = 0;

    virtual void Remove(std::uint32_t service_id) = 0;

    virtual std::vector<RegistryEntry> LoadAll() = 0;
};

// Default implementation used when persistence is disabled in the
// config (or when no DB pool is wired). Every mutator is a no-op;
// LoadAll() returns an empty vector. Lets PeerRegistry hold a
// non-null pointer unconditionally so the call sites don't need
// null guards on every mutation.
class NoopRegistryPersistence final : public IRegistryPersistence
{
public:
    void Upsert(const RegistryEntry&) override {}
    void Touch(std::uint32_t, std::uint64_t, std::uint32_t,
               std::uint32_t, std::int64_t) override {}
    void Remove(std::uint32_t) override {}
    std::vector<RegistryEntry> LoadAll() override { return {}; }
};

} // namespace tcontrolsvr
