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
    Register(std::int32_t user_id,
             std::shared_ptr<tnetlib::AsioSession> session) override;

    void Unregister(
        const std::shared_ptr<tnetlib::AsioSession>& session) override;

    std::size_t Count() const override;

private:
    mutable std::mutex m_mtx;
    // user_id → live session (weak so we don't extend lifetime)
    std::unordered_map<std::int32_t,
                       std::weak_ptr<tnetlib::AsioSession>> m_by_user;
    // session pointer → user_id (for Unregister's reverse lookup).
    // Stores raw pointer keys, which is safe because we always
    // erase before the shared_ptr's last reference can be released.
    std::unordered_map<tnetlib::AsioSession*, std::int32_t> m_by_session;
};

} // namespace tloginsvr::services
