#include "metrics.h"

#include <algorithm>
#include <vector>

namespace tmapsvr::ops {

void Latency::Record(std::uint64_t value_us) noexcept
{
    m_count.fetch_add(1,         std::memory_order_relaxed);
    m_sum  .fetch_add(value_us,  std::memory_order_relaxed);

    // Compare-exchange loops for min/max — uncontended path
    // executes a single CAS; contended path retries.
    auto cur_min = m_min.load(std::memory_order_relaxed);
    while (value_us < cur_min &&
           !m_min.compare_exchange_weak(cur_min, value_us,
               std::memory_order_relaxed, std::memory_order_relaxed))
        { /* retry */ }

    auto cur_max = m_max.load(std::memory_order_relaxed);
    while (value_us > cur_max &&
           !m_max.compare_exchange_weak(cur_max, value_us,
               std::memory_order_relaxed, std::memory_order_relaxed))
        { /* retry */ }
}

Counter& Metrics::HandlerCalls(std::uint16_t wId)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_h_calls[wId];
}

Counter& Metrics::HandlerErrors(std::uint16_t wId)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_h_errors[wId];
}

Latency& Metrics::HandlerLatency(std::uint16_t wId)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_h_latency[wId];
}

Counter& Metrics::DbQueryCalls(const char* name)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_q_calls[std::string(name)];
}

Latency& Metrics::DbQueryLatency(const char* name)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    return m_q_latency[std::string(name)];
}

Metrics::Snapshot Metrics::Sample() const
{
    Snapshot out;
    std::lock_guard<std::mutex> lk(m_mtx);

    // Build sorted-by-id handler rows so the Prometheus output is
    // stable across scrapes (eyeballing the same wId column in the
    // metrics dump shouldn't shift between dumps).
    std::vector<std::uint16_t> ids;
    ids.reserve(m_h_calls.size());
    for (const auto& [wId, _] : m_h_calls) ids.push_back(wId);
    std::sort(ids.begin(), ids.end());
    for (auto wId : ids)
    {
        const auto it_e = m_h_errors.find(wId);
        const auto it_l = m_h_latency.find(wId);
        out.handlers.push_back({
            wId,
            m_h_calls.at(wId).Value(),
            it_e == m_h_errors.end() ? 0 : it_e->second.Value(),
            it_l == m_h_latency.end() ? 0 : it_l->second.Count(),
            it_l == m_h_latency.end() ? 0 : it_l->second.SumUs(),
            it_l == m_h_latency.end() ? 0 : it_l->second.MinUs(),
            it_l == m_h_latency.end() ? 0 : it_l->second.MaxUs(),
        });
    }

    std::vector<std::string> qnames;
    qnames.reserve(m_q_calls.size());
    for (const auto& [n, _] : m_q_calls) qnames.push_back(n);
    std::sort(qnames.begin(), qnames.end());
    for (const auto& n : qnames)
    {
        const auto it_l = m_q_latency.find(n);
        out.queries.push_back({
            n,
            m_q_calls.at(n).Value(),
            it_l == m_q_latency.end() ? 0 : it_l->second.Count(),
            it_l == m_q_latency.end() ? 0 : it_l->second.SumUs(),
            it_l == m_q_latency.end() ? 0 : it_l->second.MinUs(),
            it_l == m_q_latency.end() ? 0 : it_l->second.MaxUs(),
        });
    }
    return out;
}

} // namespace tmapsvr::ops
