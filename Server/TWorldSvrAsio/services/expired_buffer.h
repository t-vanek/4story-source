#pragma once

// ExpiredBuffer — the sorted event-expiry queue (legacy m_vExpired,
// SSHandler.cpp:10668 + CheckEventExpired TWorldSvr.cpp:5280). Peers
// (and the world itself in legacy) push (type, deadline, v1, v2)
// entries via SM_EVENTEXPIRED_REQ; a periodic tick pops the due ones
// and dispatches them (wanted expiry, tactics-wanted expiry, tactics
// contract end).
//
// Kept legacy quirk (documented): a REMOVE request whose entry is
// not found INSERTS it instead — the legacy loop falls through to
// the unconditional sorted insert.

#include <cstdint>
#include <mutex>
#include <vector>

namespace tworldsvr {

// EXPIRED_TYPE (TWorldType.h:216).
inline constexpr std::uint8_t kExpiredGuildWanted   = 1; // EXPIRED_GMW
inline constexpr std::uint8_t kExpiredTacticsWanted = 2; // EXPIRED_GTW
inline constexpr std::uint8_t kExpiredTactics       = 3; // EXPIRED_GT

struct ExpiredEntry
{
    std::uint8_t  type = 0;
    std::int64_t  time = 0;
    std::uint32_t value1 = 0;
    std::uint32_t value2 = 0;
};

class ExpiredBuffer
{
public:
    ExpiredBuffer() = default;
    ExpiredBuffer(const ExpiredBuffer&) = delete;
    ExpiredBuffer& operator=(const ExpiredBuffer&) = delete;

    // Legacy OnSM_EVENTEXPIRED_REQ: !insert with an exact match →
    // erase; otherwise sorted insert (by time; ties keep arrival
    // order before the first later entry).
    void InsertOrRemove(bool insert, const ExpiredEntry& entry);

    // Pop every entry with time <= now (legacy CheckEventExpired).
    std::vector<ExpiredEntry> PopDue(std::int64_t now);

    std::size_t Size() const;

private:
    mutable std::mutex        m_mtx;
    std::vector<ExpiredEntry> m_entries;   // sorted by time asc
};

} // namespace tworldsvr
