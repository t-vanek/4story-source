#pragma once

// In-memory char_id → AsioSession map. Populated when a CS_CONNECT_REQ
// clears (F4/F5) and consulted by the world-side dispatch (F6+) to
// route DM_/MW_ inbound traffic to the right client session.
//
// Modeled on the legacy CTMapSvrModule::m_mapPLAYER container
// (CSHandler.cpp:323), minus the suspension / duplicate-ID dance —
// that lives in the per-handler logic when those features land.
//
// Thread-safety: a mutex guards both reads and writes so a SOCI
// callback running on a background worker (F8+) can Look up a session
// without racing the io_context's accept path. Methods do their own
// locking; callers never see the mutex.

#include "asio_session.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace tmapsvr {

class ISessionRegistry
{
public:
    virtual ~ISessionRegistry() = default;

    virtual void Bind(std::uint32_t char_id,
                      std::shared_ptr<tnetlib::AsioSession> session) = 0;
    virtual void Unbind(std::uint32_t char_id) = 0;

    // Remove any entry whose stored weak_ptr still locks to `session`.
    // Used by the MapServer's per-connection teardown hook to clear
    // the registry when the client drops the socket before any
    // explicit logout. O(N) walk — fine for the legacy ~8k cap.
    virtual std::size_t UnbindIfMatches(const tnetlib::AsioSession* session) = 0;

    // Returns the bound session or nullptr if the char isn't known
    // (never registered or already unbound). Holds the weak_ptr's
    // .lock() result, so the caller gets a strong ref valid for the
    // duration of its scope.
    virtual std::shared_ptr<tnetlib::AsioSession>
        Find(std::uint32_t char_id) const = 0;

    virtual std::size_t Size() const = 0;
};

class InMemorySessionRegistry final : public ISessionRegistry
{
public:
    void Bind(std::uint32_t char_id,
              std::shared_ptr<tnetlib::AsioSession> session) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_rows[char_id] = std::move(session);
    }

    void Unbind(std::uint32_t char_id) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        m_rows.erase(char_id);
    }

    std::size_t UnbindIfMatches(const tnetlib::AsioSession* session) override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        std::size_t removed = 0;
        for (auto it = m_rows.begin(); it != m_rows.end();)
        {
            auto sp = it->second.lock();
            if (!sp || sp.get() == session)
            {
                it = m_rows.erase(it);
                ++removed;
            }
            else
            {
                ++it;
            }
        }
        return removed;
    }

    std::shared_ptr<tnetlib::AsioSession>
        Find(std::uint32_t char_id) const override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        const auto it = m_rows.find(char_id);
        if (it == m_rows.end()) return nullptr;
        return it->second.lock();
    }

    std::size_t Size() const override
    {
        std::lock_guard<std::mutex> lk(m_mtx);
        return m_rows.size();
    }

private:
    mutable std::mutex                                                   m_mtx;
    std::unordered_map<std::uint32_t,
                       std::weak_ptr<tnetlib::AsioSession>>              m_rows;
};

} // namespace tmapsvr
