#include "expired_buffer.h"

namespace tworldsvr {

void ExpiredBuffer::InsertOrRemove(bool insert, const ExpiredEntry& e)
{
    std::lock_guard g(m_mtx);
    auto it = m_entries.begin();
    for (; it != m_entries.end(); ++it)
    {
        if (!insert &&
            e.time == it->time && e.type == it->type &&
            e.value1 == it->value1 && e.value2 == it->value2)
        {
            m_entries.erase(it);
            return;
        }
        if (e.time < it->time)
            break;
    }
    m_entries.insert(it, e);      // legacy: unconditional (quirk)
}

std::vector<ExpiredEntry> ExpiredBuffer::PopDue(std::int64_t now)
{
    std::lock_guard g(m_mtx);
    std::vector<ExpiredEntry> due;
    auto it = m_entries.begin();
    while (it != m_entries.end() && it->time <= now)
    {
        due.push_back(*it);
        it = m_entries.erase(it);
    }
    return due;
}

std::size_t ExpiredBuffer::Size() const
{
    std::lock_guard g(m_mtx);
    return m_entries.size();
}

} // namespace tworldsvr
