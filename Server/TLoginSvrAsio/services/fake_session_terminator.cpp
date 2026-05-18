#include "fake_session_terminator.h"

namespace tloginsvr::services {

void FakeSessionTerminator::Terminate(std::int32_t user_id,
                                          std::uint32_t session_key,
                                          TerminationReason reason)
{
    if (user_id == 0 && session_key == 0)
    {
        // No-op for never-authenticated sessions — matches the
        // expected impl contract (no DB row exists to clean up).
        return;
    }
    std::lock_guard<std::mutex> lock(m_mtx);
    m_history.push_back(TerminationRecord{ user_id, session_key, reason });
}

std::vector<TerminationRecord>
FakeSessionTerminator::History() const
{
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_history; // copy under the lock
}

std::size_t FakeSessionTerminator::Count() const
{
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_history.size();
}

} // namespace tloginsvr::services
