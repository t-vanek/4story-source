#pragma once

// Per-peer-IP rate limiter for CS_LOGIN_REQ. Token-bucket, in-process.
// Caller asks "can this IP attempt a login right now?" before
// invoking IAuthService::Authenticate; on a denied attempt the
// handler returns AuthStatus::RateLimited (mapped to LR_INTERNAL on
// the wire for legacy-client compatibility).
//
// Not in the legacy server — added in the rewrite. Cheap defensive
// hardening against credential-stuffing / brute force from a single
// peer. The bucket survives the per-session lifetime so reconnect-
// flood doesn't bypass it.

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace fourstory::ops {

struct RateLimitConfig
{
    // Max attempts before tokens deplete. Defaults to 5 — generous
    // for a real user typo, hostile for a brute-force.
    std::size_t burst = 5;

    // Token refill interval. One token gets added every
    // refill_interval. Default 10s means a sustained 1 attempt /
    // 10s after burst is exhausted (= 360/h).
    std::chrono::seconds refill_interval{ 10 };

    // GC threshold — buckets idle beyond this are pruned on next
    // Allow() call. Bounds memory.
    std::chrono::seconds idle_eviction{ 600 };
};

class LoginRateLimiter
{
public:
    explicit LoginRateLimiter(RateLimitConfig cfg = {}) : m_cfg(cfg) {}

    // Try to consume one token for the given peer key (IP string).
    // Returns true on allow, false on deny. Empty key short-circuits
    // to true — we don't rate-limit "unknown peer" since that's a
    // codepath bug, not abuse.
    bool Allow(const std::string& peer_key);

    // Test introspection.
    std::size_t BucketCount() const;

private:
    struct Bucket
    {
        double tokens;
        std::chrono::steady_clock::time_point last_refill;
    };

    RateLimitConfig                       m_cfg;
    mutable std::mutex                    m_mtx;
    std::unordered_map<std::string, Bucket> m_buckets;
};

} // namespace fourstory::ops
