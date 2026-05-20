#include "peer_registry.h"
#include "../peer_session.h"

namespace tcontrolsvr {

PeerRegistry::PeerRegistry(const IServiceInventory& inventory)
{
    Rebind(inventory);
}

void PeerRegistry::Rebind(const IServiceInventory& inventory)
{
    std::vector<ServiceInstance>                  services;
    std::unordered_map<std::uint32_t, std::size_t> idx;
    services.reserve(inventory.Services().size());
    for (std::size_t i = 0; i < inventory.Services().size(); ++i)
    {
        const auto& s = inventory.Services()[i];
        idx[s.service_id] = services.size();
        services.push_back(s);
    }
    m_services = std::move(services);
    m_idx      = std::move(idx);

    // Trim status / connection maps to live IDs only. New services
    // start with default status (Unknown) — the 1Hz monitor will
    // populate them.
    for (auto it = m_status.begin(); it != m_status.end(); )
    {
        if (m_idx.find(it->first) == m_idx.end())
            it = m_status.erase(it);
        else
            ++it;
    }
    for (auto it = m_conn.begin(); it != m_conn.end(); )
    {
        if (m_idx.find(it->first) == m_idx.end())
            it = m_conn.erase(it);
        else
            ++it;
    }
}

const ServiceInstance*
PeerRegistry::FindService(std::uint32_t service_id) const
{
    auto it = m_idx.find(service_id);
    if (it == m_idx.end()) return nullptr;
    return &m_services[it->second];
}

RuntimeStatus* PeerRegistry::Status(std::uint32_t service_id)
{
    if (m_idx.find(service_id) == m_idx.end()) return nullptr;
    return &m_status[service_id];
}

const RuntimeStatus* PeerRegistry::Status(std::uint32_t service_id) const
{
    auto it = m_status.find(service_id);
    if (it == m_status.end()) return nullptr;
    return &it->second;
}

std::shared_ptr<PeerSession>
PeerRegistry::Connection(std::uint32_t service_id) const
{
    auto it = m_conn.find(service_id);
    if (it == m_conn.end()) return nullptr;
    return it->second;
}

void PeerRegistry::SetConnection(std::uint32_t service_id,
                                 std::shared_ptr<PeerSession> conn)
{
    m_conn[service_id] = std::move(conn);
}

void PeerRegistry::ClearConnection(std::uint32_t service_id)
{
    m_conn.erase(service_id);
}

std::vector<std::shared_ptr<PeerSession>>
PeerRegistry::FindByType(std::uint8_t group_id, std::uint8_t type_id) const
{
    std::vector<std::shared_ptr<PeerSession>> out;
    for (const auto& s : m_services)
    {
        if (s.group_id != group_id) continue;
        if (s.type_id  != type_id)  continue;
        auto it = m_conn.find(s.service_id);
        if (it == m_conn.end()) continue;
        if (it->second) out.push_back(it->second);
    }
    return out;
}

} // namespace tcontrolsvr
