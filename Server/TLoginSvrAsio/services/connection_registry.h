#pragma once

// IConnectionRegistry — tracks authenticated AsioSessions by their
// authenticated user_id, so the login flow can enforce the legacy
// duplicate-kick policy:
//
//   1. User A is logged in on session X.
//   2. User A logs in again on a fresh session Y.
//   3. Registry: Register(A, Y) returns X (previous holder).
//   4. Login handler closes X. Y stays.
//
// Modern divergence from legacy: legacy CSHandler.cpp:271-289 closes
// BOTH X (old TCP socket) AND Y (sent LR_DUPLICATE then closed). We
// keep Y because the modern UX expectation is "I just logged in,
// give me the working session" — the older session is the stale one.
// Documented in handlers::OnLoginReq.
//
// Implementation pointer: registry holds weak_ptr to each AsioSession
// (sessions are owned by their per-connection HandleConnection
// coroutine via shared_ptr). On Unregister it removes by raw pointer
// since the shared_ptr may have already expired by then.

#include "asio_session.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace tloginsvr::services {

class IConnectionRegistry
{
public:
    virtual ~IConnectionRegistry() = default;

    // Register an authenticated session under `user_id`. If another
    // session was previously registered under the same user_id,
    // returns a shared_ptr to it (caller should close it to enforce
    // duplicate-kick). Returns nullptr if no previous holder.
    virtual std::shared_ptr<tnetlib::AsioSession>
    Register(std::int32_t user_id,
             std::shared_ptr<tnetlib::AsioSession> session) = 0;

    // Remove a session. No-op if not registered. Always safe to call
    // from connection-close paths.
    virtual void Unregister(
        const std::shared_ptr<tnetlib::AsioSession>& session) = 0;

    // Currently-registered session count.
    virtual std::size_t Count() const = 0;
};

} // namespace tloginsvr::services
