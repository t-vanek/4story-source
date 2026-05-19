#include "local_connection_registry.h"

namespace tloginsvr::services {

std::shared_ptr<tnetlib::AsioSession>
LocalConnectionRegistry::Register(ConnectionEntry entry,
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
LocalConnectionRegistry::Lookup(
    const std::shared_ptr<tnetlib::AsioSession>& session) const
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (auto it = m_by_session.find(session.get()); it != m_by_session.end())
    {
        return it->second;
    }
    return std::nullopt;
}

void LocalConnectionRegistry::MarkHandoff(
    const std::shared_ptr<tnetlib::AsioSession>& session)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (auto it = m_by_session.find(session.get()); it != m_by_session.end())
    {
        it->second.handoff_to_map = true;
    }
}

void LocalConnectionRegistry::MarkHandoffWithChar(
    const std::shared_ptr<tnetlib::AsioSession>& session,
    std::int32_t char_id)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (auto it = m_by_session.find(session.get()); it != m_by_session.end())
    {
        it->second.handoff_to_map = true;
        it->second.last_char_id   = char_id;
    }
}

void LocalConnectionRegistry::MarkAgreed(
    const std::shared_ptr<tnetlib::AsioSession>& session)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (auto it = m_by_session.find(session.get()); it != m_by_session.end())
    {
        it->second.agreed = true;
    }
}

void LocalConnectionRegistry::SetGroupId(
    const std::shared_ptr<tnetlib::AsioSession>& session,
    std::uint8_t group_id)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (auto it = m_by_session.find(session.get()); it != m_by_session.end())
    {
        it->second.group_id = group_id;
    }
}

void LocalConnectionRegistry::CompleteSecurityLogin(
    const std::shared_ptr<tnetlib::AsioSession>& session,
    std::uint32_t session_key)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (auto it = m_by_session.find(session.get()); it != m_by_session.end())
    {
        it->second.session_key = session_key;
        it->second.awaiting_security = false;
    }
}

void LocalConnectionRegistry::Unregister(
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

std::size_t LocalConnectionRegistry::Count() const
{
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_by_session.size();
}

std::vector<IConnectionRegistry::LiveEntry>
LocalConnectionRegistry::Snapshot() const
{
    std::lock_guard<std::mutex> lock(m_mtx);
    std::vector<LiveEntry> out;
    out.reserve(m_by_session.size());
    // Walk m_by_user so we can lock the weak_ptr → shared_ptr cheaply.
    // m_by_session keys are raw pointers; the strong ref we need lives
    // on the user-side map. Sessions whose weak_ptr has expired since
    // the by_user insert but before Unregister fires (rare but possible
    // during a connection-close race) are silently skipped — the caller
    // doesn't need them: there's nobody to Terminate against.
    for (const auto& [uid, weak] : m_by_user)
    {
        auto sess = weak.lock();
        if (!sess) continue;
        auto it = m_by_session.find(sess.get());
        if (it == m_by_session.end()) continue;
        out.push_back(LiveEntry{ .entry = it->second, .session = std::move(sess) });
    }
    return out;
}

} // namespace tloginsvr::services
