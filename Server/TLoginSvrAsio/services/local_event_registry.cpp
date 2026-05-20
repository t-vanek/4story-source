// LocalEventRegistry implementation — in-process GM event store.
//
// Single map keyed by event_id under a plain mutex. Upsert / Remove
// match the legacy m_mapEVENT semantics (CSHandler.cpp:87-120). Both
// are called only from CT_EVENTUPDATE_REQ dispatch which is itself
// peer-IP-gated to control_server_ip in LoginServer::Dispatch.
//
// Legacy parity: Server/TLoginSvr/CTLoginSvrModule::m_mapEVENT under
// m_csLI critical section.

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
