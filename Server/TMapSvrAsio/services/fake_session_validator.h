#pragma once

// FakeMapSessionValidator — accepts every lookup whose dwKEY is in the
// seeded allow-list. Wired by main.cpp when [database] is empty so
// dev / test runs without a real TCURRENTUSER. Mirrors the
// FakeAuthService pattern in TLoginSvrAsio.

#include "session_validator.h"

#include <atomic>
#include <mutex>
#include <unordered_set>

namespace tmapsvr {

class FakeMapSessionValidator final : public IMapSessionValidator
{
public:
    // Seed an accepted (user, char, key) tuple. Returns true on first
    // insert, false if it was already seeded — tests assert one or the
    // other to catch double-seeding.
    bool Seed(std::uint32_t user_id, std::uint32_t char_id, std::uint32_t key)
    {
        const std::uint64_t packed = Pack(user_id, char_id, key);
        std::lock_guard<std::mutex> lk(m_mu);
        return m_allow.insert(packed).second;
    }

    // Match-everything mode for the dev path where the operator just
    // wants to confirm the wire codec works against a fake client.
    void SetAcceptAll(bool accept) { m_accept_all.store(accept); }

    bool Validate(const MapSessionLookup& lookup) override
    {
        if (m_accept_all.load()) return true;
        const std::uint64_t packed = Pack(lookup.user_id, lookup.char_id, lookup.dw_key);
        std::lock_guard<std::mutex> lk(m_mu);
        return m_allow.count(packed) > 0;
    }

private:
    static std::uint64_t Pack(std::uint32_t u, std::uint32_t c, std::uint32_t k)
    {
        // 32-bit lookup is enough since (user, char) uniquely identifies a
        // legit session and we only need the equality check.
        std::uint64_t out = static_cast<std::uint64_t>(u) ^
            (static_cast<std::uint64_t>(c) << 21) ^
            (static_cast<std::uint64_t>(k) << 42);
        return out;
    }

    std::mutex                          m_mu;
    std::unordered_set<std::uint64_t>   m_allow;
    std::atomic<bool>                   m_accept_all{ false };
};

} // namespace tmapsvr
