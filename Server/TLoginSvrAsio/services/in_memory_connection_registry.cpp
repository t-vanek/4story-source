#include "in_memory_connection_registry.h"

namespace tloginsvr::services {

std::shared_ptr<tnetlib::AsioSession>
InMemoryConnectionRegistry::Register(std::int32_t user_id,
                                     std::shared_ptr<tnetlib::AsioSession> session)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    std::shared_ptr<tnetlib::AsioSession> previous;
    if (auto it = m_by_user.find(user_id); it != m_by_user.end())
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

    // Also drop any prior reverse mapping for the new session — it
    // shouldn't happen (a session re-registering with a different
    // user_id), but defend against it so we never leave stale rows.
    if (auto it = m_by_session.find(session.get()); it != m_by_session.end())
    {
        m_by_user.erase(it->second);
        m_by_session.erase(it);
    }

    m_by_user[user_id] = session;
    m_by_session[session.get()] = user_id;
    return previous;
}

void InMemoryConnectionRegistry::Unregister(
    const std::shared_ptr<tnetlib::AsioSession>& session)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    auto it = m_by_session.find(session.get());
    if (it == m_by_session.end()) return;

    const auto user_id = it->second;
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
