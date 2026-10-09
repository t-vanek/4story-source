// LocalConnectionRegistry implementation — single-process duplicate-
// kick + agreement gate state.
//
// Production-grade (not a test fake) for any single-process login
// deployment. Indexes under a single mutex:
//   m_by_user      (user_id → LiveEntry)  — for duplicate-kick lookup
//                                            on a fresh successful login.
//   m_by_session   (session ptr → user_id) — for the reverse lookup
//                                            used by lobby handlers
//                                            and the close-time cleanup.
//   m_sessions     (session ptr → weak session) — includes pending challenges.
//
// Register() returns any prior session that needs to be kicked — the
// caller (handlers::OnLoginReq) is responsible for closing it so the
// duplicate user sees a disconnect. Snapshot includes pending connections for
// shutdown/inspection; connection coroutines perform their own cleanup.
//
// Legacy parity: Server/TLoginSvr/CTLoginSvrModule::m_mapTUSER +
// m_csLI critical section.

#include "local_connection_registry.h"
#include <algorithm>

namespace tloginsvr::services {

std::shared_ptr<tnetlib::AsioSession>
LocalConnectionRegistry::Register(ConnectionEntry entry,
                                  std::shared_ptr<tnetlib::AsioSession> session)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    std::shared_ptr<tnetlib::AsioSession> previous;
    if (!entry.awaiting_security)
        if (auto it = m_by_user.find(entry.user_id); it != m_by_user.end())
            previous = it->second.lock();
    if (previous && previous != session)
    {
        m_by_session.erase(previous.get());
        m_sessions.erase(previous.get());
    }
    if (auto it = m_by_session.find(session.get()); it != m_by_session.end())
        if (auto u = m_by_user.find(it->second.user_id); u != m_by_user.end() && u->second.lock() == session)
            m_by_user.erase(u);
    if (!entry.awaiting_security) m_by_user[entry.user_id] = session;
    m_by_session[session.get()] = std::move(entry);
    m_sessions[session.get()] = session;
    if (previous == session) previous.reset();
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

void LocalConnectionRegistry::MarkSecurityVerified(
    const std::shared_ptr<tnetlib::AsioSession>& session)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (auto it = m_by_session.find(session.get()); it != m_by_session.end())
    {
        if (it->second.awaiting_security)
        {
            it->second.security_verified = true;
            it->second.security_deadline = std::min(it->second.security_deadline,
                std::chrono::steady_clock::now() + std::chrono::seconds(30));
        }
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
    m_sessions.erase(session.get());

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
    // Include pending connections without indexing them as authenticated users.
    for (const auto& [ptr, weak] : m_sessions)
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
