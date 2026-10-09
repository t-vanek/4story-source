#include "registry_event_bus.h"

namespace tcontrolsvr {

const char* RegistryEventKindName(RegistryEventKind k)
{
    switch (k)
    {
        case RegistryEventKind::Registered:       return "registered";
        case RegistryEventKind::Heartbeat:        return "heartbeat";
        case RegistryEventKind::Deregistered:     return "deregistered";
        case RegistryEventKind::Expired:          return "expired";
        case RegistryEventKind::ScmStatusChanged: return "scm-status";
    }
    return "unknown";
}

std::uint64_t RegistryEventBus::Subscribe(Subscriber fn)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    const auto token = m_next_token++;
    m_subs.emplace(token, std::move(fn));
    return token;
}

void RegistryEventBus::Unsubscribe(std::uint64_t token)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    m_subs.erase(token);
}

void RegistryEventBus::Publish(const RegistryEvent& ev)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    for (const auto& [_, fn] : m_subs)
        fn(ev);
}

std::size_t RegistryEventBus::SubscriberCount() const
{
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_subs.size();
}

} // namespace tcontrolsvr
