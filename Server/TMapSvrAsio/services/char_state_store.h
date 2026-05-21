#pragma once

// In-memory live character state — char_id → CharSnapshot.
//
// Populated by the DM_LOADCHAR_REQ handler when a char loads from DB.
// Updated in-place by gameplay handlers (combat, movement, loot, …).
// Read by the MapServer teardown hook to persist the final snapshot
// via IPlayerService::SaveChar before the session is discarded.
//
// Thread-safety: mutex on all ops — handlers co_spawn off the
// io_context and SOCI workers run on a thread pool; both may touch
// this map concurrently.

#include "domain/character.h"
#include "asio_session.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace tmapsvr {

class ICharStateStore
{
public:
    virtual ~ICharStateStore() = default;

    // Store or overwrite a snapshot (called on successful char load).
    virtual void Store(std::uint32_t char_id, const CharSnapshot& snap) = 0;

    // In-place mutation — lets handlers patch individual fields without
    // copying the whole snapshot out and back.
    virtual void Update(std::uint32_t char_id,
                        const std::function<void(CharSnapshot&)>& fn) = 0;

    // Snapshot by value — copy so callers don't hold the lock.
    virtual std::optional<CharSnapshot> Get(std::uint32_t char_id) const = 0;

    // Remove by char_id (called after SaveChar completes).
    virtual void Remove(std::uint32_t char_id) = 0;

    // Reverse teardown: find and remove any entry whose char session
    // pointer matches — used when the client drops before full auth.
    // Returns the removed char_id, or 0 if no match.
    virtual std::uint32_t RemoveBySession(
        const tnetlib::AsioSession* sess) = 0;
};

class InMemoryCharStateStore final : public ICharStateStore
{
public:
    void Store(std::uint32_t char_id, const CharSnapshot& snap) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_rows[char_id] = snap;
    }

    void Update(std::uint32_t char_id,
                const std::function<void(CharSnapshot&)>& fn) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        const auto it = m_rows.find(char_id);
        if (it != m_rows.end()) fn(it->second);
    }

    std::optional<CharSnapshot> Get(std::uint32_t char_id) const override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        const auto it = m_rows.find(char_id);
        if (it == m_rows.end()) return std::nullopt;
        return it->second;
    }

    void Remove(std::uint32_t char_id) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_rows.erase(char_id);
    }

    // No per-session pointer stored here — the session_registry has
    // the char_id↔session mapping; look it up there first and call
    // Remove(char_id). RemoveBySession is a fallback for pre-auth drops.
    std::uint32_t RemoveBySession(const tnetlib::AsioSession*) override
    {
        return 0;   // char_state only populated after successful load
    }

private:
    mutable std::mutex                                       m_mtx;
    std::unordered_map<std::uint32_t, CharSnapshot>         m_rows;
};

} // namespace tmapsvr
