#include "operator_registry.h"
#include "../operator_session.h"

namespace tcontrolsvr {

std::uint32_t
OperatorRegistry::Register(std::shared_ptr<OperatorSession> session,
                           const std::string& user_id,
                           std::shared_ptr<OperatorSession>& existing_out)
{
    existing_out.reset();
    auto it = m_by_user_id.find(user_id);
    if (it != m_by_user_id.end())
    {
        if (auto sp = it->second.lock())
            existing_out = std::move(sp);
    }
    const std::uint32_t seq = ++m_next_seq;
    m_by_user_id[user_id] = session;
    m_by_seq[seq]         = std::move(session);
    return seq;
}

void OperatorRegistry::Unregister(OperatorSession* raw)
{
    if (!raw) return;
    for (auto it = m_by_user_id.begin(); it != m_by_user_id.end(); )
    {
        auto sp = it->second.lock();
        if (!sp || sp.get() == raw)
            it = m_by_user_id.erase(it);
        else
            ++it;
    }
    for (auto it = m_by_seq.begin(); it != m_by_seq.end(); )
    {
        auto sp = it->second.lock();
        if (!sp || sp.get() == raw)
            it = m_by_seq.erase(it);
        else
            ++it;
    }
}

std::shared_ptr<OperatorSession>
OperatorRegistry::FindByUserId(const std::string& user_id) const
{
    auto it = m_by_user_id.find(user_id);
    if (it == m_by_user_id.end()) return nullptr;
    return it->second.lock();
}

std::shared_ptr<OperatorSession>
OperatorRegistry::FindBySeq(std::uint32_t seq) const
{
    auto it = m_by_seq.find(seq);
    if (it == m_by_seq.end()) return nullptr;
    return it->second.lock();
}

std::vector<std::shared_ptr<OperatorSession>>
OperatorRegistry::SnapshotLoggedIn() const
{
    std::vector<std::shared_ptr<OperatorSession>> out;
    out.reserve(m_by_seq.size());
    for (const auto& [seq, weak] : m_by_seq)
    {
        if (auto sp = weak.lock())
            out.push_back(std::move(sp));
    }
    return out;
}

} // namespace tcontrolsvr
