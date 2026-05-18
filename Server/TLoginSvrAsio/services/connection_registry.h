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
#include <optional>

namespace tloginsvr::services {

// Per-session metadata stamped at LOGIN-success time. The registry
// holds one Entry per live authenticated session; HandleConnection
// reads it back at close to drive SessionTerminator.
struct ConnectionEntry
{
    std::int32_t  user_id = 0;
    std::uint32_t session_key = 0;
    // Set by OnStartReq's SR_SUCCESS path. Flips the close-time
    // termination reason from Disconnect to MapHandoff so the real
    // SOCI impl can preserve TCURRENTUSER for the Map server's key
    // validation.
    bool          handoff_to_map = false;
    // Per-session agreement gate. Mirrors legacy CTUser::m_bAgreement
    // (TLoginSvr/TUser.h:28). Set to true when LR_SUCCESS comes back
    // from IAuthService::Authenticate (agreement already on file) or
    // when CS_AGREEMENT_REQ is acknowledged. CharList / Create /
    // Delete / Start / Veteran all gate on this — legacy
    // CSHandler.cpp:600 returns EC_SESSION_INVALIDCHAR if not set.
    bool          agreed = false;
    // Selected world group. Stamped on the first CS_CHARLIST_REQ (the
    // group_id byte the client sends). DELCHAR refuses if the request
    // arrives with a mismatching bGroupID — same defensive check as
    // legacy CSHandler.cpp:1223.
    std::uint8_t  group_id = 0;
    // Random 64-bit nonce stamped at LOGIN. Echoed in CS_LOGIN_ACK so
    // the client can derive a per-session HMAC seed for the legacy
    // exec-check feature. Modernized server doesn't use it but
    // populates it to match the wire layout.
    std::int64_t  check_key = 0;
};

class IConnectionRegistry
{
public:
    virtual ~IConnectionRegistry() = default;

    // Register an authenticated session under `entry.user_id`. If
    // another session was previously registered under the same
    // user_id, returns a shared_ptr to it (caller should close it
    // to enforce duplicate-kick). Returns nullptr if no previous
    // holder.
    virtual std::shared_ptr<tnetlib::AsioSession>
    Register(ConnectionEntry entry,
             std::shared_ptr<tnetlib::AsioSession> session) = 0;

    // Lookup the entry for a session. Returns nullopt if not
    // registered (unauthenticated session).
    virtual std::optional<ConnectionEntry>
    Lookup(const std::shared_ptr<tnetlib::AsioSession>& session) const = 0;

    // Flip handoff_to_map for the registered session.
    virtual void MarkHandoff(
        const std::shared_ptr<tnetlib::AsioSession>& session) = 0;

    // Flip the per-session agreement gate. Called from OnAgreementReq
    // after IAuthService::SetAgreement returns, and from OnLoginReq
    // when LR_SUCCESS arrives (agreement already on file). No-op if
    // the session isn't registered.
    virtual void MarkAgreed(
        const std::shared_ptr<tnetlib::AsioSession>& session) = 0;

    // Stamp the per-session selected world group. First CHARLIST_REQ
    // sets this; downstream handlers (DELCHAR, START) read it back
    // for cross-check. No-op if the session isn't registered.
    virtual void SetGroupId(
        const std::shared_ptr<tnetlib::AsioSession>& session,
        std::uint8_t group_id) = 0;

    // Remove a session. No-op if not registered. Always safe to call
    // from connection-close paths.
    virtual void Unregister(
        const std::shared_ptr<tnetlib::AsioSession>& session) = 0;

    // Currently-registered session count.
    virtual std::size_t Count() const = 0;
};

} // namespace tloginsvr::services
