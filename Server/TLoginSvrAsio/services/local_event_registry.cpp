#include "local_event_registry.h"

namespace tloginsvr::services {

void LocalEventRegistry::Upsert(EventEntry entry)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_entries[entry.event_id] = std::move(entry);
}

void LocalEventRegistry::Remove(std::uint8_t event_id)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_entries.erase(event_id);
}

std::vector<EventEntry> LocalEventRegistry::Snapshot() const
{
    std::lock_guard<std::mutex> lock(m_mtx);
    std::vector<EventEntry> out;
    out.reserve(m_entries.size());
    for (const auto& [_, v] : m_entries) out.push_back(v);
    return out;
}

} // namespace tloginsvr::services
