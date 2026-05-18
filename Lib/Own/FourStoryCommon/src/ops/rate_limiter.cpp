#include "fourstory/ops/rate_limiter.h"

#include <algorithm>

namespace fourstory::ops {

bool LoginRateLimiter::Allow(const std::string& peer_key)
{
    if (peer_key.empty()) return true;

    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(m_mtx);

    // Lazy GC — sweep expired buckets when the dict grows past a few
    // hundred entries. Keeps the steady-state working set bounded
    // without spinning a background thread.
    if (m_buckets.size() > 256)
    {
        for (auto it = m_buckets.begin(); it != m_buckets.end(); )
        {
            if (now - it->second.last_refill > m_cfg.idle_eviction)
                it = m_buckets.erase(it);
            else
                ++it;
        }
    }

    auto& b = m_buckets[peer_key];
    if (b.last_refill.time_since_epoch().count() == 0)
    {
        // Fresh bucket — full.
        b.tokens = static_cast<double>(m_cfg.burst);
        b.last_refill = now;
    }
    else
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - b.last_refill).count();
        const double refill = static_cast<double>(elapsed) /
            std::chrono::duration_cast<std::chrono::milliseconds>(
                m_cfg.refill_interval).count();
        b.tokens = std::min(static_cast<double>(m_cfg.burst), b.tokens + refill);
        b.last_refill = now;
    }

    if (b.tokens >= 1.0)
    {
        b.tokens -= 1.0;
        return true;
    }
    return false;
}

std::size_t LoginRateLimiter::BucketCount() const
{
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_buckets.size();
}

} // namespace fourstory::ops
