#include "in_memory_connection_registry.h"

namespace tloginsvr::services {

std::shared_ptr<tnetlib::AsioSession>
InMemoryConnectionRegistry::Register(ConnectionEntry entry,
                                     std::shared_ptr<tnetlib::AsioSession> session)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    std::shared_ptr<tnetlib::AsioSession> previous;
    if (auto it = m_by_user.find(entry.user_id); it != m_by_user.end())
    {
        previous = it->second.lock();
        if (previous)
        {
            // Drop the reverse mapping for the old session before
            // overwriting; otherwise the eventual Unregister(old)
            // would no-op for user_id but leak the by_session entry.
            m_by_session.erase(previous.get());
        }
    }

    // Defensive: drop any prior reverse mapping for the new session.
    if (auto it = m_by_session.find(session.get()); it != m_by_session.end())
    {
        m_by_user.erase(it->second.user_id);
        m_by_session.erase(it);
    }

    m_by_user[entry.user_id] = session;
    m_by_session[session.get()] = entry;
    return previous;
}

std::optional<ConnectionEntry>
InMemoryConnectionRegistry::Lookup(
    const std::shared_ptr<tnetlib::AsioSession>& session) const
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (auto it = m_by_session.find(session.get()); it != m_by_session.end())
    {
        return it->second;
    }
    return std::nullopt;
}

void InMemoryConnectionRegistry::MarkHandoff(
    const std::shared_ptr<tnetlib::AsioSession>& session)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (auto it = m_by_session.find(session.get()); it != m_by_session.end())
    {
        it->second.handoff_to_map = true;
    }
}

void InMemoryConnectionRegistry::Unregister(
    const std::shared_ptr<tnetlib::AsioSession>& session)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    auto it = m_by_session.find(session.get());
    if (it == m_by_session.end()) return;

    const auto user_id = it->second.user_id;
    m_by_session.erase(it);

    // Only erase the by_user entry if it still points at THIS session.
    // (If a later Register replaced us, that entry now belongs to the
    // new session and Unregister(this) should not touch it.)
    if (auto u = m_by_user.find(user_id); u != m_by_user.end())
    {
        if (u->second.lock() == session)
        {
            m_by_user.erase(u);
        }
    }
}

std::size_t InMemoryConnectionRegistry::Count() const
{
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_by_session.size();
}

} // namespace tloginsvr::services
