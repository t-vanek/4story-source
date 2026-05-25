#pragma once

// IMetrics — central counter / latency registry. Atomic primitives
// so handler coroutines on different threads (or on the strand)
// can record without locks.
//
// The format keeps things deliberately simple for this commit:
//   - Counter: monotonic uint64
//   - Latency: count + sum + min + max (no buckets yet)
//
// Histograms with Prometheus-style buckets land with the /metrics
// HTTP endpoint commit; the call sites here would migrate from
// `Record(us)` (still works) to a histogram-aware backend without
// touching the dispatch or handler code.

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace tmapsvr::ops {

class Counter
{
public:
    void Add(std::uint64_t n = 1) noexcept
    {
        m_value.fetch_add(n, std::memory_order_relaxed);
    }

    std::uint64_t Value() const noexcept
    {
        return m_value.load(std::memory_order_relaxed);
    }

private:
    std::atomic<std::uint64_t> m_value{0};
};

// Latency summary in microseconds. Lock-free via atomics; min/max
// use compare-exchange so concurrent calls don't lose updates.
class Latency
{
public:
    void Record(std::uint64_t value_us) noexcept;

    std::uint64_t Count()    const noexcept { return m_count.load(std::memory_order_relaxed); }
    std::uint64_t SumUs()    const noexcept { return m_sum.load(std::memory_order_relaxed);   }
    std::uint64_t MinUs()    const noexcept { return m_min.load(std::memory_order_relaxed);   }
    std::uint64_t MaxUs()    const noexcept { return m_max.load(std::memory_order_relaxed);   }

private:
    std::atomic<std::uint64_t> m_count{0};
    std::atomic<std::uint64_t> m_sum  {0};
    std::atomic<std::uint64_t> m_min  {UINT64_MAX};
    std::atomic<std::uint64_t> m_max  {0};
};

// Per-message-id metrics keyed by uint16 wire id. Per-DB-query
// metrics keyed by query name. Both maps are mutex-guarded for
// insertion; once a key is present, the returned reference is
// stable (atomic-counted), so the lookup is the only contended
// path.
class IMetrics
{
public:
    virtual ~IMetrics() = default;

    virtual Counter&  HandlerCalls   (std::uint16_t wId) = 0;
    virtual Counter&  HandlerErrors  (std::uint16_t wId) = 0;
    virtual Latency&  HandlerLatency (std::uint16_t wId) = 0;

    virtual Counter&  DbQueryCalls   (const char* name)  = 0;
    virtual Latency&  DbQueryLatency (const char* name)  = 0;
};

class Metrics final : public IMetrics
{
public:
    Counter&  HandlerCalls   (std::uint16_t wId) override;
    Counter&  HandlerErrors  (std::uint16_t wId) override;
    Latency&  HandlerLatency (std::uint16_t wId) override;
    Counter&  DbQueryCalls   (const char* name)  override;
    Latency&  DbQueryLatency (const char* name)  override;

    // Snapshot of every metric for the future /metrics endpoint.
    // Format-agnostic — the Prometheus formatter lives in a sibling
    // file when that commit lands.
    struct Snapshot
    {
        struct PerHandler { std::uint16_t wId; std::uint64_t calls; std::uint64_t errors;
                            std::uint64_t lat_count; std::uint64_t lat_sum_us;
                            std::uint64_t lat_min_us; std::uint64_t lat_max_us; };
        struct PerQuery   { std::string name; std::uint64_t calls;
                            std::uint64_t lat_count; std::uint64_t lat_sum_us;
                            std::uint64_t lat_min_us; std::uint64_t lat_max_us; };
        std::vector<PerHandler> handlers;
        std::vector<PerQuery>   queries;
    };
    Snapshot Sample() const;

private:
    template <class K, class V>
    using Map = std::unordered_map<K, V>;

    mutable std::mutex                     m_mtx;
    Map<std::uint16_t, Counter>            m_h_calls;
    Map<std::uint16_t, Counter>            m_h_errors;
    Map<std::uint16_t, Latency>            m_h_latency;
    Map<std::string,    Counter>           m_q_calls;
    Map<std::string,    Latency>           m_q_latency;
};

} // namespace tmapsvr::ops
