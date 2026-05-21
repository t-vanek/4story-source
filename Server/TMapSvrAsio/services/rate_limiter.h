#pragma once

// IRateLimiter — per-session message rate gate.
//
// The dispatch entry point calls TryAcquire before invoking any
// handler. False means "client is over budget" — the dispatcher
// drops the message, increments the rate-limit metric, and emits
// a HandlerError audit event so monitoring can alert on abuse.
//
// Key (uint64_t) is opaque to the limiter. The caller picks the
// scope: dispatch uses the raw AsioSession pointer cast to uint64
// (one bucket per connection), which both pre-auth and post-auth
// share. Char-id keying would lose track during the brief window
// between socket open and CS_CONNECT_REQ — session-ptr keying is
// stable across that handshake.
//
// Cleanup: when a connection closes, map_server.cpp's teardown
// hook calls Remove(key) to free the bucket. Without Remove, the
// map grows monotonically with unique sessions — bounded by the
// connection cap but still wasteful.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace tmapsvr {

class IRateLimiter
{
public:
    virtual ~IRateLimiter() = default;

    // Attempt to consume one token for `key`. Returns true when a
    // token was deducted, false when the bucket is empty.
    virtual bool TryAcquire(std::uint64_t key) = 0;

    // Release tracking for a key (called when the keyed session
    // closes). No-op if the key wasn't tracked.
    virtual void Remove(std::uint64_t key) = 0;
};

// Token-bucket implementation. Configurable burst (bucket size)
// and refill rate (tokens added per second, fractional accumulator).
//
// Thread-safe — guarded by a single mutex. Per-key contention is
// very low in practice because each session only emits from its
// own coroutine and the dispatcher is single-strand by default.
class TokenBucketLimiter final : public IRateLimiter
{
public:
    TokenBucketLimiter(std::uint32_t burst,
                       std::uint32_t refill_per_sec);

    bool TryAcquire(std::uint64_t key) override;
    void Remove(std::uint64_t key) override;

    // Total acquires that failed the gate, monotonic since boot.
    // Sampled by main() / metrics endpoint.
    std::uint64_t DroppedTotal() const noexcept
    {
        return m_dropped.load(std::memory_order_relaxed);
    }

private:
    struct Bucket
    {
        double                                tokens;
        std::chrono::steady_clock::time_point last_refill;
    };

    std::uint32_t              m_burst;
    std::uint32_t              m_refill_per_sec;
    std::atomic<std::uint64_t> m_dropped{0};
    std::mutex                                       m_mtx;
    std::unordered_map<std::uint64_t, Bucket>        m_buckets;
};

} // namespace tmapsvr
