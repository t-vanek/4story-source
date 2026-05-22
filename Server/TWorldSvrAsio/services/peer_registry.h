#pragma once

// PeerRegistry — by-wID index of live PeerSession objects. Used by
// guild / party / war handlers to route a reply back to the map
// server that "owns" a character (legacy FindMapSvr(bMainID)).
//
// Cardinality is small (one entry per map server in the cluster —
// O(10..50) in production), so a flat hash map under a single
// shared_mutex is fine; no sharding needed. Membership churn is
// rare (map servers come and go on operator action, not per-frame).
//
// Lifecycle:
//   - PeerSession constructed at TCP accept (wID=0, not registered).
//   - OnRelaysvrReq sets the wID and calls Register(peer).
//   - WorldServer::HandleConnection calls Unregister on read-loop
//     exit — guarantees the registry never points at a closed wire.

#include "../peer_session.h"

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace tworldsvr {

class PeerRegistry
{
public:
    PeerRegistry() = default;
    PeerRegistry(const PeerRegistry&) = delete;
    PeerRegistry& operator=(const PeerRegistry&) = delete;

    // Register a peer under its wID. Returns false if a peer with
    // the same wID is already registered (duplicate connect from
    // the same map server). Caller may decide to close the older
    // session in that case; W3a-2 logs and keeps the original
    // entry, matching the legacy "first one wins" behavior.
    bool Register(std::shared_ptr<PeerSession> peer);

    // Remove by wID. Idempotent (returns nullptr if absent). Called
    // from HandleConnection's exit path so a disconnected peer
    // doesn't linger.
    std::shared_ptr<PeerSession> Unregister(std::uint16_t wid);

    // Lookup by wID. nullptr if absent.
    std::shared_ptr<PeerSession> Find(std::uint16_t wid) const;

    std::size_t Size() const;

    // Snapshot every registered peer (e.g. for cluster-wide
    // broadcasts in W3a-3). Holds the shared lock for the duration
    // of the copy.
    std::vector<std::shared_ptr<PeerSession>> Snapshot() const;

private:
    mutable std::shared_mutex                                       m_mtx;
    std::unordered_map<std::uint16_t, std::shared_ptr<PeerSession>> m_peers;
};

} // namespace tworldsvr
