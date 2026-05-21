#include "rate_limiter.h"

namespace tmapsvr {

TokenBucketLimiter::TokenBucketLimiter(std::uint32_t burst,
                                       std::uint32_t refill_per_sec)
    : m_burst(burst)
    , m_refill_per_sec(refill_per_sec)
{
}

bool TokenBucketLimiter::TryAcquire(std::uint64_t key)
{
    // refill_per_sec == 0 means "limiter disabled" — every
    // TryAcquire succeeds without touching the map at all. Lets
    // main() instantiate one limiter unconditionally and toggle
    // behavior via config.
    if (m_refill_per_sec == 0 || m_burst == 0)
        return true;

    const auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lk(m_mtx);

    auto& b = m_buckets[key];
    if (b.tokens == 0.0 && b.last_refill.time_since_epoch().count() == 0)
    {
        // First touch — start full so a new connection isn't
        // immediately throttled.
        b.tokens      = static_cast<double>(m_burst);
        b.last_refill = now;
    }
    else
    {
        // Refill since last touch.
        const auto delta_s = std::chrono::duration<double>(now - b.last_refill).count();
        b.tokens = std::min<double>(
            static_cast<double>(m_burst),
            b.tokens + delta_s * static_cast<double>(m_refill_per_sec));
        b.last_refill = now;
    }

    if (b.tokens < 1.0)
    {
        m_dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    b.tokens -= 1.0;
    return true;
}

void TokenBucketLimiter::Remove(std::uint64_t key)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    m_buckets.erase(key);
}

} // namespace tmapsvr
