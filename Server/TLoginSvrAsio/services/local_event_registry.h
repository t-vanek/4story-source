#pragma once

// LocalEventRegistry — in-process IEventRegistry impl. Production-
// suitable for a single-process login deployment; matches legacy
// `m_mapEVENT` semantics under m_csLI critical section.

#include "event_registry.h"

#include <mutex>
#include <unordered_map>

namespace tloginsvr::services {

class LocalEventRegistry : public IEventRegistry
{
public:
    void Upsert(EventEntry entry) override;
    void Remove(std::uint8_t event_id) override;
    std::vector<EventEntry> Snapshot() const override;

private:
    mutable std::mutex m_mtx;
    std::unordered_map<std::uint8_t, EventEntry> m_entries;
};

} // namespace tloginsvr::services
