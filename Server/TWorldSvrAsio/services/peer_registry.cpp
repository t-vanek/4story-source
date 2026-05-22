#include "services/peer_registry.h"

namespace tworldsvr {

bool PeerRegistry::Register(std::shared_ptr<PeerSession> peer)
{
    if (!peer || peer->Wid() == 0) return false;
    std::unique_lock lock(m_mtx);
    auto [it, inserted] = m_peers.emplace(peer->Wid(), std::move(peer));
    return inserted;
}

std::shared_ptr<PeerSession>
PeerRegistry::Unregister(std::uint16_t wid)
{
    if (wid == 0) return nullptr;
    std::unique_lock lock(m_mtx);
    auto it = m_peers.find(wid);
    if (it == m_peers.end()) return nullptr;
    auto out = std::move(it->second);
    m_peers.erase(it);
    return out;
}

std::shared_ptr<PeerSession>
PeerRegistry::Find(std::uint16_t wid) const
{
    std::shared_lock lock(m_mtx);
    auto it = m_peers.find(wid);
    return it == m_peers.end() ? nullptr : it->second;
}

std::size_t PeerRegistry::Size() const
{
    std::shared_lock lock(m_mtx);
    return m_peers.size();
}

std::vector<std::shared_ptr<PeerSession>> PeerRegistry::Snapshot() const
{
    std::vector<std::shared_ptr<PeerSession>> out;
    std::shared_lock lock(m_mtx);
    out.reserve(m_peers.size());
    for (const auto& [_, p] : m_peers) out.push_back(p);
    return out;
}

std::vector<std::shared_ptr<PeerSession>>
PeerRegistry::SnapshotExcept(std::uint16_t except_wid) const
{
    std::vector<std::shared_ptr<PeerSession>> out;
    std::shared_lock lock(m_mtx);
    out.reserve(m_peers.size());
    for (const auto& [wid, p] : m_peers)
        if (wid != except_wid) out.push_back(p);
    return out;
}

} // namespace tworldsvr
