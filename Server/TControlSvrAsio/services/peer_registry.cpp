#include "peer_registry.h"

#include "../peer_session.h"
#include "registry_persistence.h"

#include <chrono>

namespace tcontrolsvr {

namespace {
std::int64_t NowUnix()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}
} // namespace

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

// --- Dynamic registry -------------------------------------------------

std::uint64_t PeerRegistry::Register(const RegistryEntry& proposed)
{
    const auto now = std::chrono::steady_clock::now();
    RegistryEntry entry = proposed;
    entry.lease_epoch     = m_next_epoch++;
    entry.registered_at   = now;
    entry.last_heartbeat_at = now;
    m_registry[proposed.service_id] = std::move(entry);
    const auto& stored = m_registry[proposed.service_id];
    if (m_persistence) m_persistence->Upsert(stored);
    RegistryEvent ev{};
    ev.kind            = RegistryEventKind::Registered;
    ev.service_id      = stored.service_id;
    ev.reported_name   = stored.reported_name;
    ev.reported_addr   = stored.reported_addr;
    ev.reported_port   = stored.reported_port;
    ev.version         = stored.version;
    ev.lease_epoch     = stored.lease_epoch;
    ev.cur_users       = stored.cur_users;
    ev.max_users       = stored.max_users;
    m_events.Publish(ev);
    return stored.lease_epoch;
}

std::uint64_t PeerRegistry::Heartbeat(std::uint32_t service_id,
                                      std::uint64_t lease_epoch,
                                      std::uint32_t cur_users,
                                      std::uint32_t max_users)
{
    auto it = m_registry.find(service_id);
    if (it == m_registry.end() || it->second.lease_epoch != lease_epoch)
        return 0;
    it->second.last_heartbeat_at = std::chrono::steady_clock::now();
    it->second.cur_users = cur_users;
    it->second.max_users = max_users;
    if (m_persistence)
        m_persistence->Touch(service_id, lease_epoch, cur_users, max_users,
                             NowUnix());
    RegistryEvent ev{};
    ev.kind          = RegistryEventKind::Heartbeat;
    ev.service_id    = it->second.service_id;
    ev.reported_name = it->second.reported_name;
    ev.lease_epoch   = it->second.lease_epoch;
    ev.cur_users     = cur_users;
    ev.max_users     = max_users;
    m_events.Publish(ev);
    return it->second.lease_epoch;
}

bool PeerRegistry::Deregister(std::uint32_t service_id,
                              std::uint64_t lease_epoch)
{
    auto it = m_registry.find(service_id);
    if (it == m_registry.end() || it->second.lease_epoch != lease_epoch)
        return false;
    RegistryEvent ev{};
    ev.kind          = RegistryEventKind::Deregistered;
    ev.service_id    = it->second.service_id;
    ev.reported_name = it->second.reported_name;
    ev.lease_epoch   = it->second.lease_epoch;
    m_registry.erase(it);
    if (m_persistence) m_persistence->Remove(service_id);
    m_events.Publish(ev);
    return true;
}

std::size_t
PeerRegistry::ExpireStale(std::chrono::steady_clock::duration max_age)
{
    const auto cutoff = std::chrono::steady_clock::now() - max_age;
    std::size_t expired = 0;
    for (auto it = m_registry.begin(); it != m_registry.end(); )
    {
        if (it->second.last_heartbeat_at < cutoff)
        {
            RegistryEvent ev{};
            ev.kind          = RegistryEventKind::Expired;
            ev.service_id    = it->second.service_id;
            ev.reported_name = it->second.reported_name;
            ev.lease_epoch   = it->second.lease_epoch;
            const auto sid = it->second.service_id;
            it = m_registry.erase(it);
            if (m_persistence) m_persistence->Remove(sid);
            m_events.Publish(ev);
            ++expired;
        }
        else ++it;
    }
    return expired;
}

std::vector<RegistryEntry> PeerRegistry::Registry() const
{
    std::vector<RegistryEntry> out;
    out.reserve(m_registry.size());
    for (const auto& [_, e] : m_registry) out.push_back(e);
    return out;
}

const RegistryEntry*
PeerRegistry::FindRegistration(std::uint32_t service_id) const
{
    auto it = m_registry.find(service_id);
    if (it == m_registry.end()) return nullptr;
    return &it->second;
}

void PeerRegistry::Hydrate(const std::vector<RegistryEntry>& entries)
{
    // Replay each persisted entry into the in-RAM map + advance the
    // epoch counter past every stored lease. Do NOT call m_persistence
    // here — we're reading from it, not writing back, and the
    // persistence layer is already consistent. DO publish events so
    // any subscriber that came up before main called Hydrate (only
    // really main itself for now) sees the hydrated state.
    for (const auto& e : entries)
    {
        m_registry[e.service_id] = e;
        if (e.lease_epoch >= m_next_epoch)
            m_next_epoch = e.lease_epoch + 1;
        RegistryEvent ev{};
        ev.kind          = RegistryEventKind::Registered;
        ev.service_id    = e.service_id;
        ev.reported_name = e.reported_name;
        ev.reported_addr = e.reported_addr;
        ev.reported_port = e.reported_port;
        ev.version       = e.version;
        ev.lease_epoch   = e.lease_epoch;
        ev.cur_users     = e.cur_users;
        ev.max_users     = e.max_users;
        m_events.Publish(ev);
    }
}

} // namespace tcontrolsvr
