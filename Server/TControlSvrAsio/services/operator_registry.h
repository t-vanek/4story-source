#pragma once

// OperatorRegistry — tracks logged-in operators by GM id (string)
// and by the synthetic per-session DWORD that legacy `m_dwManagerSeq`
// hands out at login. Two roles:
//
//   1. duplicate-kick: a second login with the same szID kicks the
//      first session (legacy OnCT_OPLOGIN_REQ closes pAlreadyM).
//   2. broadcast: SERVICEDATA / SERVICECHANGE acks fan out to every
//      operator currently logged in (legacy walks m_mapMANAGER).
//
// The registry is single-threaded — everything runs on one io_context
// thread per the modernization plan, so plain std::unordered_map is
// fine. If we ever need to serve from a worker pool, wrap in a strand.

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tcontrolsvr {

class OperatorSession;

class OperatorRegistry
{
public:
    // Allocates a fresh dwID and stashes the session under both
    // (id_str, dwID) keys. If `existing` is set on return, the caller
    // owns the duplicate-kick (close pAlreadyM, then re-call Register).
    std::uint32_t Register(std::shared_ptr<OperatorSession> session,
                           const std::string& user_id,
                           std::shared_ptr<OperatorSession>& existing_out);

    void Unregister(OperatorSession* raw);

    std::shared_ptr<OperatorSession> FindByUserId(const std::string& user_id) const;
    std::shared_ptr<OperatorSession> FindBySeq(std::uint32_t seq) const;

    // Snapshot of currently logged-in operator sessions. Used by the
    // SERVICEDATA / SERVICECHANGE fan-out. Returns shared_ptrs so the
    // caller can safely send even if a session closes mid-iteration.
    std::vector<std::shared_ptr<OperatorSession>> SnapshotLoggedIn() const;

    std::size_t Size() const { return m_by_seq.size(); }

private:
    std::uint32_t m_next_seq = 0;
    std::unordered_map<std::string,   std::weak_ptr<OperatorSession>> m_by_user_id;
    std::unordered_map<std::uint32_t, std::weak_ptr<OperatorSession>> m_by_seq;
};

} // namespace tcontrolsvr
