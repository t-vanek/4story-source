#pragma once

// LocalConnectionRegistry — production-grade single-process session
// registry. Stores live session state in this process's memory; same
// shape as the legacy CTLoginSvrModule's `m_mapTUSER` map. The "Local"
// prefix distinguishes it from a hypothetical distributed alternative
// (Redis-backed, etc.) that a sharded multi-instance deploy would
// need; the IConnectionRegistry interface is the same.
//
// Production semantics:
//   * duplicate-kick policy (legacy parity)
//   * per-session metadata for the agreement gate + group-id stamping
//   * O(1) lookup by both user_id and session pointer
//   * no persistence — sessions die with the process, which is the
//     correct semantics (a session has no meaning across restarts)

#include "connection_registry.h"

#include <mutex>
#include <unordered_map>

namespace tloginsvr::services {

class LocalConnectionRegistry : public IConnectionRegistry
{
public:
    std::shared_ptr<tnetlib::AsioSession>
    Register(ConnectionEntry entry,
             std::shared_ptr<tnetlib::AsioSession> session) override;

    std::optional<ConnectionEntry>
    Lookup(const std::shared_ptr<tnetlib::AsioSession>& session) const override;

    void MarkHandoff(
        const std::shared_ptr<tnetlib::AsioSession>& session) override;

    void MarkHandoffWithChar(
        const std::shared_ptr<tnetlib::AsioSession>& session,
        std::int32_t char_id) override;

    void MarkAgreed(
        const std::shared_ptr<tnetlib::AsioSession>& session) override;

    void SetGroupId(
        const std::shared_ptr<tnetlib::AsioSession>& session,
        std::uint8_t group_id) override;

    void CompleteSecurityLogin(
        const std::shared_ptr<tnetlib::AsioSession>& session,
        std::uint32_t session_key) override;

    void Unregister(
        const std::shared_ptr<tnetlib::AsioSession>& session) override;

    std::size_t Count() const override;

    std::vector<LiveEntry> Snapshot() const override;

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
