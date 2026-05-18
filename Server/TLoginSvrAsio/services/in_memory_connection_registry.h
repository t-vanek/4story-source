#pragma once

// Thread-safe in-memory IConnectionRegistry. Suitable for production
// — the legacy server tracked the same state in `m_mapTUSER` (also
// in-memory, single-process). When the server is sharded across
// multiple instances, the duplicate-kick policy needs a distributed
// store (Redis hash + watch); the interface above accommodates that
// — we'd add SociConnectionRegistry / RedisConnectionRegistry beside
// this one without touching the handler.

#include "connection_registry.h"

#include <mutex>
#include <unordered_map>

namespace tloginsvr::services {

class InMemoryConnectionRegistry : public IConnectionRegistry
{
public:
    std::shared_ptr<tnetlib::AsioSession>
    Register(ConnectionEntry entry,
             std::shared_ptr<tnetlib::AsioSession> session) override;

    std::optional<ConnectionEntry>
    Lookup(const std::shared_ptr<tnetlib::AsioSession>& session) const override;

    void MarkHandoff(
        const std::shared_ptr<tnetlib::AsioSession>& session) override;

    void Unregister(
        const std::shared_ptr<tnetlib::AsioSession>& session) override;

    std::size_t Count() const override;

private:
    mutable std::mutex m_mtx;
    // user_id → live session (weak so we don't extend lifetime)
    std::unordered_map<std::int32_t,
                       std::weak_ptr<tnetlib::AsioSession>> m_by_user;
    // session pointer → entry (for Lookup + Unregister's reverse path).
    // Stores raw pointer keys, safe because we always erase before
    // the shared_ptr's last reference can be released.
    std::unordered_map<tnetlib::AsioSession*, ConnectionEntry> m_by_session;
};

} // namespace tloginsvr::services
