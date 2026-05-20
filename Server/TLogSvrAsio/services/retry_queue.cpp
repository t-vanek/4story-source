#include "retry_queue.h"

#include <utility>

namespace tlogsvr {

RetryQueue::RetryQueue(std::size_t capacity)
    : m_capacity(capacity)
{
}

bool RetryQueue::PushBack(LogRecord rec)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (m_q.size() >= m_capacity) return false;
    m_q.push_back(std::move(rec));
    return true;
}

void RetryQueue::PushFront(LogRecord rec)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    m_q.push_front(std::move(rec));
}

bool RetryQueue::PopFront(LogRecord& out)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (m_q.empty()) return false;
    out = std::move(m_q.front());
    m_q.pop_front();
    return true;
}

std::size_t RetryQueue::Size() const
{
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_q.size();
}

bool RetryQueue::Empty() const
{
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_q.empty();
}

} // namespace tlogsvr
