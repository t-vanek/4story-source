#include "event_registry.h"

#include <mutex>

namespace tworldsvr {

void EventRegistry::Set(TEventInfo info)
{
    const std::uint32_t key = info.dw_index;
    std::unique_lock lk(m_lock);
    m_events.insert_or_assign(key, std::move(info));
}

bool EventRegistry::Erase(std::uint32_t dw_index)
{
    std::unique_lock lk(m_lock);
    return m_events.erase(dw_index) != 0;
}

std::vector<TEventInfo> EventRegistry::Snapshot() const
{
    std::shared_lock lk(m_lock);
    std::vector<TEventInfo> out;
    out.reserve(m_events.size());
    for (const auto& [_, info] : m_events)
        out.push_back(info);
    return out;
}

std::size_t EventRegistry::Size() const
{
    std::shared_lock lk(m_lock);
    return m_events.size();
}

} // namespace tworldsvr
